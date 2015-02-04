// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/devtools_bridge/session_dependency_factory.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/task_runner.h"
#include "base/threading/thread.h"
#include "components/devtools_bridge/abstract_data_channel.h"
#include "components/devtools_bridge/abstract_peer_connection.h"
#include "components/devtools_bridge/rtc_configuration.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediaconstraintsinterface.h"
#include "third_party/libjingle/source/talk/app/webrtc/peerconnectioninterface.h"
#include "third_party/webrtc/base/bind.h"
#include "third_party/webrtc/base/messagehandler.h"
#include "third_party/webrtc/base/messagequeue.h"
#include "third_party/webrtc/base/ssladapter.h"
#include "third_party/webrtc/base/thread.h"

namespace devtools_bridge {

class RTCConfiguration::Impl
    : public RTCConfiguration,
      public webrtc::PeerConnectionInterface::RTCConfiguration {
 public:
  void AddIceServer(const std::string& uri,
                    const std::string& username,
                    const std::string& credential) override {
    webrtc::PeerConnectionInterface::IceServer server;
    server.uri = uri;
    server.username = username;
    server.password = credential;
    servers.push_back(server);
  }

  const Impl& impl() const override {
    return *this;
  }

 private:
  webrtc::PeerConnectionInterface::RTCConfiguration base_;
};

namespace {

template <typename T>
void CheckedRelease(rtc::scoped_refptr<T>* ptr) {
  CHECK_EQ(0, ptr->release()->Release());
}

class MediaConstraints
    : public webrtc::MediaConstraintsInterface {
 public:
  ~MediaConstraints() override {}

  const Constraints& GetMandatory() const override { return mandatory_; }

  const Constraints& GetOptional() const override { return optional_; }

  void AddMandatory(const std::string& key, const std::string& value) {
    mandatory_.push_back(Constraint(key, value));
  }

 private:
  Constraints mandatory_;
  Constraints optional_;
};

/**
 * Posts tasks on signaling thread. If stopped (when SesseionDependencyFactry
 * is destroying) ignores posted tasks.
 */
class SignalingThreadTaskRunner : public base::TaskRunner,
                                  private rtc::MessageHandler {
 public:
  explicit SignalingThreadTaskRunner(rtc::Thread* thread) : thread_(thread) {}

  bool PostDelayedTask(const tracked_objects::Location& from_here,
                       const base::Closure& task,
                       base::TimeDelta delay) override {
    DCHECK(delay.ToInternalValue() == 0);

    rtc::CritScope scope(&critical_section_);

    if (thread_)
      thread_->Send(this, 0, new Task(task));

    return true;
  }

  bool RunsTasksOnCurrentThread() const override {
    rtc::CritScope scope(&critical_section_);

    return thread_ != NULL && thread_->IsCurrent();
  }

  void Stop() {
    rtc::CritScope scope(&critical_section_);
    thread_ = NULL;
  }

 private:
  typedef rtc::TypedMessageData<base::Closure> Task;

  ~SignalingThreadTaskRunner() override {}

  void OnMessage(rtc::Message* msg) override {
    static_cast<Task*>(msg->pdata)->data().Run();
  }

  mutable rtc::CriticalSection critical_section_;
  rtc::Thread* thread_;  // Guarded by |critical_section_|.
};

class DataChannelObserverImpl : public webrtc::DataChannelObserver {
 public:
  DataChannelObserverImpl(
      webrtc::DataChannelInterface* data_channel,
      scoped_ptr<AbstractDataChannel::Observer> observer)
      : data_channel_(data_channel),
        observer_(observer.Pass()) {
  }

  void InitState() {
    open_ = data_channel_->state() == webrtc::DataChannelInterface::kOpen;
  }

  void OnStateChange() override {
    bool open = data_channel_->state() == webrtc::DataChannelInterface::kOpen;

    if (open == open_) return;

    open_ = open;
    if (open) {
      observer_->OnOpen();
    } else {
      observer_->OnClose();
    }
  }

  void OnMessage(const webrtc::DataBuffer& buffer) override {
    observer_->OnMessage(buffer.data.data(), buffer.size());
  }

 private:
  webrtc::DataChannelInterface* const data_channel_;
  scoped_ptr<AbstractDataChannel::Observer> const observer_;
  bool open_;
};

/**
 * Thread-safe view on AbstractDataChannel.
 */
class DataChannelProxyImpl : public AbstractDataChannel::Proxy {
 public:
  DataChannelProxyImpl(
      SessionDependencyFactory* factory,
      rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel)
      : data_channel_(data_channel),
        signaling_thread_task_runner_(
            factory->signaling_thread_task_runner()) {
  }

  void StopOnSignalingThread() {
    data_channel_ = NULL;
  }

  void SendBinaryMessage(const void* data, size_t length) override {
    auto buffer = make_scoped_ptr(new webrtc::DataBuffer(rtc::Buffer(), true));
    buffer->data.SetData(data, length);

    signaling_thread_task_runner_->PostTask(
        FROM_HERE, base::Bind(
            &DataChannelProxyImpl::SendMessageOnSignalingThread,
            this,
            base::Passed(&buffer)));
  }

  void Close() override {
    signaling_thread_task_runner_->PostTask(
        FROM_HERE, base::Bind(&DataChannelProxyImpl::CloseOnSignalingThread,
                              this));
  }

 private:

  ~DataChannelProxyImpl() override {}

  void SendMessageOnSignalingThread(scoped_ptr<webrtc::DataBuffer> message) {
    if (data_channel_ != NULL)
      data_channel_->Send(*message);
  }

  void CloseOnSignalingThread() {
    if (data_channel_ != NULL)
      data_channel_->Close();
  }

  // Accessed on signaling thread.
  rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel_;

  const scoped_refptr<base::TaskRunner> signaling_thread_task_runner_;
};

class DataChannelImpl : public AbstractDataChannel {
 public:
  DataChannelImpl(
      SessionDependencyFactory* factory,
      rtc::Thread* const signaling_thread,
      rtc::scoped_refptr<webrtc::DataChannelInterface> impl)
      : factory_(factory),
        signaling_thread_(signaling_thread),
        impl_(impl) {
  }

  ~DataChannelImpl() override {
    if (proxy_.get()) {
      signaling_thread_->Invoke<void>(rtc::Bind(
          &DataChannelProxyImpl::StopOnSignalingThread, proxy_.get()));
    }
  }

  void RegisterObserver(scoped_ptr<Observer> observer) override {
    observer_.reset(new DataChannelObserverImpl(impl_.get(), observer.Pass()));
    signaling_thread_->Invoke<void>(rtc::Bind(
        &DataChannelImpl::RegisterObserverOnSignalingThread, this));
  }

  void UnregisterObserver() override {
    DCHECK(observer_.get() != NULL);
    impl_->UnregisterObserver();
    observer_.reset();
  }

  void SendBinaryMessage(void* data, size_t length) override {
    SendMessage(data, length, true);
  }

  void SendTextMessage(void* data, size_t length) override {
    SendMessage(data, length, false);
  }

  void SendMessage(void* data, size_t length, bool is_binary) {
    impl_->Send(webrtc::DataBuffer(rtc::Buffer(data, length), is_binary));
  }

  void Close() override {
    impl_->Close();
  }

  scoped_refptr<Proxy> proxy() override {
    if (!proxy_.get())
      proxy_ = new DataChannelProxyImpl(factory_, impl_);
    return proxy_;
  }

 private:
  void RegisterObserverOnSignalingThread() {
    // State initialization and observer registration happen atomically
    // if done on the signaling thread (see rtc::Thread::Send).
    observer_->InitState();
    impl_->RegisterObserver(observer_.get());
  }

  SessionDependencyFactory* const factory_;
  scoped_refptr<DataChannelProxyImpl> proxy_;
  rtc::Thread* const signaling_thread_;
  scoped_ptr<DataChannelObserverImpl> observer_;
  const rtc::scoped_refptr<webrtc::DataChannelInterface> impl_;
};

class PeerConnectionObserverImpl
    : public webrtc::PeerConnectionObserver {
 public:
  PeerConnectionObserverImpl(AbstractPeerConnection::Delegate* delegate)
      : delegate_(delegate),
        connected_(false) {
  }

  void OnAddStream(webrtc::MediaStreamInterface* stream) override {}

  void OnRemoveStream(webrtc::MediaStreamInterface* stream) override {}

  void OnDataChannel(webrtc::DataChannelInterface* data_channel) override {}

  void OnRenegotiationNeeded() override {}

  void OnSignalingChange(
      webrtc::PeerConnectionInterface::SignalingState new_state) override {}

  void OnIceConnectionChange(
      webrtc::PeerConnectionInterface::IceConnectionState new_state) override {
    bool connected =
        new_state == webrtc::PeerConnectionInterface::kIceConnectionConnected ||
        new_state == webrtc::PeerConnectionInterface::kIceConnectionCompleted;

    if (connected != connected_) {
      connected_ = connected;
      delegate_->OnIceConnectionChange(connected_);
    }
  }

  void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override {
    std::string sdp;
    candidate->ToString(&sdp);

    delegate_->OnIceCandidate(
        candidate->sdp_mid(), candidate->sdp_mline_index(), sdp);
  }

 private:
  AbstractPeerConnection::Delegate* const delegate_;
  bool connected_;
};

/**
 * Helper object which may outlive PeerConnectionImpl. Provides access
 * to the connection and the delegate to operaion callback objects
 * in a safe way. Always accessible on the signaling thread.
 */
class PeerConnectionHolder : public rtc::RefCountInterface {
 public:
  PeerConnectionHolder(
      rtc::Thread* signaling_thread,
      webrtc::PeerConnectionInterface* connection,
      AbstractPeerConnection::Delegate* delegate)
      : signaling_thread_(signaling_thread),
        connection_(connection),
        delegate_(delegate),
        disposed_(false) {
  }

  ~PeerConnectionHolder() override { DCHECK(disposed_); }

  void Dispose() {
    DCHECK(!IsDisposed());
    disposed_ = true;
  }

  webrtc::PeerConnectionInterface* connection() {
    DCHECK(!IsDisposed());
    return connection_;
  }

  AbstractPeerConnection::Delegate* delegate() {
    DCHECK(!IsDisposed());
    return delegate_;
  }

  bool IsDisposed() {
    DCHECK(signaling_thread_->IsCurrent());
    return disposed_;
  }

 private:
  rtc::Thread* const signaling_thread_;
  webrtc::PeerConnectionInterface* const connection_;
  AbstractPeerConnection::Delegate* const delegate_;
  bool disposed_;
};

class CreateAndSetHandler
    : public webrtc::CreateSessionDescriptionObserver,
      public webrtc::SetSessionDescriptionObserver {
 public:
  explicit CreateAndSetHandler(
      rtc::scoped_refptr<PeerConnectionHolder> holder)
      : holder_(holder) {
  }

  void OnSuccess(webrtc::SessionDescriptionInterface* desc) override {
    if (holder_->IsDisposed()) return;

    type_ = desc->type();
    if (desc->ToString(&description_)) {
      holder_->connection()->SetLocalDescription(this, desc);
    } else {
      OnFailure("Can't serialize session description");
    }
  }

  void OnSuccess() override {
    if (holder_->IsDisposed()) return;

    if (type_ == webrtc::SessionDescriptionInterface::kOffer) {
      holder_->delegate()->OnLocalOfferCreatedAndSetSet(description_);
    } else {
      DCHECK_EQ(webrtc::SessionDescriptionInterface::kAnswer, type_);

      holder_->delegate()->OnLocalAnswerCreatedAndSetSet(description_);
    }
  }

  void OnFailure(const std::string& error) override {
    if (holder_->IsDisposed()) return;

    holder_->delegate()->OnFailure(error);
  }

 private:
  const rtc::scoped_refptr<PeerConnectionHolder> holder_;
  std::string type_;
  std::string description_;
};

class SetRemoteDescriptionHandler
   : public webrtc::SetSessionDescriptionObserver {
 public:
  SetRemoteDescriptionHandler(
      rtc::scoped_refptr<PeerConnectionHolder> holder)
      : holder_(holder) {
  }

  void OnSuccess() override {
    if (holder_->IsDisposed()) return;

    holder_->delegate()->OnRemoteDescriptionSet();
  }

  void OnFailure(const std::string& error) override {
    if (holder_->IsDisposed()) return;

    holder_->delegate()->OnFailure(error);
  }

 private:
  const rtc::scoped_refptr<PeerConnectionHolder> holder_;
};

class PeerConnectionImpl : public AbstractPeerConnection {
 public:
  PeerConnectionImpl(
      SessionDependencyFactory* const factory,
      rtc::Thread* signaling_thread,
      rtc::scoped_refptr<webrtc::PeerConnectionInterface> connection,
      scoped_ptr<PeerConnectionObserverImpl> observer,
      scoped_ptr<AbstractPeerConnection::Delegate> delegate)
      : factory_(factory),
        holder_(new rtc::RefCountedObject<PeerConnectionHolder>(
            signaling_thread, connection.get(), delegate.get())),
        signaling_thread_(signaling_thread),
        connection_(connection),
        observer_(observer.Pass()),
        delegate_(delegate.Pass()) {
  }

  ~PeerConnectionImpl() override {
    signaling_thread_->Invoke<void>(rtc::Bind(
        &PeerConnectionImpl::DisposeOnSignalingThread, this));
  }

  void CreateAndSetLocalOffer() override {
    connection_->CreateOffer(MakeCreateAndSetHandler(), NULL);
  }

  void CreateAndSetLocalAnswer() override {
    connection_->CreateAnswer(MakeCreateAndSetHandler(), NULL);
  }

  void SetRemoteOffer(const std::string& description) override {
    SetRemoteDescription(
        webrtc::SessionDescriptionInterface::kOffer, description);
  }

  void SetRemoteAnswer(const std::string& description) override {
    SetRemoteDescription(
        webrtc::SessionDescriptionInterface::kAnswer, description);
  }

  void SetRemoteDescription(
      const std::string& type, const std::string& description) {
    webrtc::SdpParseError error;
    scoped_ptr<webrtc::SessionDescriptionInterface> value(
        webrtc::CreateSessionDescription(type, description, &error));
    if (value == NULL) {
      OnParseError(error);
      return;
    }
    // Takes ownership on |value|.
    connection_->SetRemoteDescription(
        new rtc::RefCountedObject<SetRemoteDescriptionHandler>(holder_),
        value.release());
  }

  void AddIceCandidate(const std::string& sdp_mid,
                       int sdp_mline_index,
                       const std::string& sdp) override {
    webrtc::SdpParseError error;
    auto candidate = webrtc::CreateIceCandidate(
        sdp_mid, sdp_mline_index, sdp, &error);
    if (candidate == NULL) {
      OnParseError(error);
      return;
    }
    // Doesn't takes ownership.
    connection_->AddIceCandidate(candidate);
    delete candidate;
  }

  scoped_ptr<AbstractDataChannel> CreateDataChannel(int channelId) override {
    webrtc::DataChannelInit init;
    init.ordered = true;
    init.negotiated = true;
    init.id = channelId;

    return make_scoped_ptr(new DataChannelImpl(
        factory_,
        signaling_thread_,
        connection_->CreateDataChannel("", &init)));
  }

 private:
  webrtc::CreateSessionDescriptionObserver* MakeCreateAndSetHandler() {
    return new rtc::RefCountedObject<CreateAndSetHandler>(holder_);
  }

  void DisposeOnSignalingThread() {
    DCHECK(signaling_thread_->IsCurrent());

    CheckedRelease(&connection_);
    holder_->Dispose();
  }

  void OnParseError(const webrtc::SdpParseError& error) {
    // TODO(serya): Send on signaling thread.
  }

  SessionDependencyFactory* const factory_;
  const rtc::scoped_refptr<PeerConnectionHolder> holder_;
  rtc::Thread* const signaling_thread_;
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> connection_;
  const scoped_ptr<PeerConnectionObserverImpl> observer_;
  const scoped_ptr<AbstractPeerConnection::Delegate> delegate_;
};

class SessionDependencyFactoryImpl : public SessionDependencyFactory {
 public:
  SessionDependencyFactoryImpl(
      const base::Closure& cleanup_on_signaling_thread)
      : cleanup_on_signaling_thread_(cleanup_on_signaling_thread) {
    signaling_thread_.SetName("signaling_thread", NULL);
    signaling_thread_.Start();
    worker_thread_.SetName("worker_thread", NULL);
    worker_thread_.Start();

    factory_ = webrtc::CreatePeerConnectionFactory(
        &worker_thread_, &signaling_thread_, NULL, NULL, NULL);
  }

  ~SessionDependencyFactoryImpl() override {
    if (signaling_thread_task_runner_.get())
      signaling_thread_task_runner_->Stop();

    signaling_thread_.Invoke<void>(rtc::Bind(
        &SessionDependencyFactoryImpl::DisposeOnSignalingThread, this));
  }

  scoped_ptr<AbstractPeerConnection> CreatePeerConnection(
      scoped_ptr<RTCConfiguration> config,
      scoped_ptr<AbstractPeerConnection::Delegate> delegate) override {
    auto observer = make_scoped_ptr(
        new PeerConnectionObserverImpl(delegate.get()));

    MediaConstraints constraints;
    constraints.AddMandatory(
        MediaConstraints::kEnableDtlsSrtp, MediaConstraints::kValueTrue);

    auto connection = factory_->CreatePeerConnection(
        config->impl(), &constraints, NULL, NULL, observer.get());

    return make_scoped_ptr(new PeerConnectionImpl(
        this, &signaling_thread_, connection, observer.Pass(),
        delegate.Pass()));
  }

  scoped_refptr<base::TaskRunner> signaling_thread_task_runner() override {
    if (!signaling_thread_task_runner_.get()) {
      signaling_thread_task_runner_ =
          new SignalingThreadTaskRunner(&signaling_thread_);
    }
    return signaling_thread_task_runner_;
  }

  scoped_refptr<base::TaskRunner> io_thread_task_runner() override {
    if (!io_thread_.get()) {
      io_thread_.reset(new base::Thread("devtools bridge IO thread"));
      base::Thread::Options options;
      options.message_loop_type = base::MessageLoop::TYPE_IO;
      CHECK(io_thread_->StartWithOptions(options));
    }
    return io_thread_->task_runner();
  }

 private:
  void DisposeOnSignalingThread() {
    DCHECK(signaling_thread_.IsCurrent());
    CheckedRelease(&factory_);
    if (!cleanup_on_signaling_thread_.is_null())
      cleanup_on_signaling_thread_.Run();
  }

  scoped_ptr<base::Thread> io_thread_;
  scoped_refptr<SignalingThreadTaskRunner> signaling_thread_task_runner_;
  base::Closure cleanup_on_signaling_thread_;
  rtc::Thread signaling_thread_;
  rtc::Thread worker_thread_;
  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory_;
};

} // namespace

// RTCCOnfiguration

// static
scoped_ptr<RTCConfiguration> RTCConfiguration::CreateInstance() {
  return make_scoped_ptr(new RTCConfiguration::Impl());
}

// SessionDependencyFactory

// static
bool SessionDependencyFactory::InitializeSSL() {
  return rtc::InitializeSSL();
}

// static
bool SessionDependencyFactory::CleanupSSL() {
  return rtc::CleanupSSL();
}

// static
scoped_ptr<SessionDependencyFactory> SessionDependencyFactory::CreateInstance(
    const base::Closure& cleanup_on_signaling_thread) {
  return make_scoped_ptr(new SessionDependencyFactoryImpl(
      cleanup_on_signaling_thread));
}

}  // namespace devtools_bridge
