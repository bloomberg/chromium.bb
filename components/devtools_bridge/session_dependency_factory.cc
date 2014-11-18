// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/devtools_bridge/session_dependency_factory.h"

#include "components/devtools_bridge/abstract_data_channel.h"
#include "components/devtools_bridge/abstract_peer_connection.h"
#include "components/devtools_bridge/rtc_configuration.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediaconstraintsinterface.h"
#include "third_party/libjingle/source/talk/app/webrtc/peerconnectioninterface.h"
#include "third_party/webrtc/base/bind.h"
#include "third_party/webrtc/base/ssladapter.h"
#include "third_party/webrtc/base/thread.h"

namespace devtools_bridge {

class RTCConfiguration::Impl
    : public RTCConfiguration,
      public webrtc::PeerConnectionInterface::RTCConfiguration {
 public:

  virtual void AddIceServer(
      const std::string& uri,
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
  virtual ~MediaConstraints() {}

  virtual const Constraints& GetMandatory() const override {
    return mandatory_;
  }

  virtual const Constraints& GetOptional() const override {
    return optional_;
  }

  void AddMandatory(const std::string& key, const std::string& value) {
    mandatory_.push_back(Constraint(key, value));
  }

 private:
  Constraints mandatory_;
  Constraints optional_;
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

  virtual void OnStateChange() override {
    bool open = data_channel_->state() == webrtc::DataChannelInterface::kOpen;

    if (open == open_) return;

    open_ = open;
    if (open) {
      observer_->OnOpen();
    } else {
      observer_->OnClose();
    }
  }

  virtual void OnMessage(const webrtc::DataBuffer& buffer) override {
    observer_->OnMessage(buffer.data.data(), buffer.size());
  }

 private:
  webrtc::DataChannelInterface* const data_channel_;
  scoped_ptr<AbstractDataChannel::Observer> const observer_;
  bool open_;
};

class DataChannelImpl : public AbstractDataChannel {
 public:
  explicit DataChannelImpl(
      rtc::Thread* const signaling_thread,
      rtc::scoped_refptr<webrtc::DataChannelInterface> impl)
      : signaling_thread_(signaling_thread),
        impl_(impl) {
  }

  virtual void RegisterObserver(scoped_ptr<Observer> observer) override {
    observer_.reset(new DataChannelObserverImpl(impl_.get(), observer.Pass()));
    signaling_thread_->Invoke<void>(rtc::Bind(
        &DataChannelImpl::RegisterObserverOnSignalingThread, this));
  }

  virtual void UnregisterObserver() override {
    DCHECK(observer_.get() != NULL);
    impl_->UnregisterObserver();
    observer_.reset();
  }

  virtual void SendBinaryMessage(void* data, size_t length) override {
    SendMessage(data, length, true);
  }

  virtual void SendTextMessage(void* data, size_t length) override {
    SendMessage(data, length, false);
  }

  void SendMessage(void* data, size_t length, bool is_binary) {
    impl_->Send(webrtc::DataBuffer(rtc::Buffer(data, length), is_binary));
  }

  void Close() override {
    impl_->Close();
  }

 private:
  void RegisterObserverOnSignalingThread() {
    // State initialization and observer registration happen atomically
    // if done on the signaling thread (see rtc::Thread::Send).
    observer_->InitState();
    impl_->RegisterObserver(observer_.get());
  }

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

  virtual void OnAddStream(webrtc::MediaStreamInterface* stream) override {}

  virtual void OnRemoveStream(webrtc::MediaStreamInterface* stream) override {}

  virtual void OnDataChannel(webrtc::DataChannelInterface* data_channel)
      override {}

  virtual void OnRenegotiationNeeded() override {}

  virtual void OnSignalingChange(
       webrtc::PeerConnectionInterface::SignalingState new_state) override {
  }

  virtual void OnIceConnectionChange(
      webrtc::PeerConnectionInterface::IceConnectionState new_state) override {
    bool connected =
        new_state == webrtc::PeerConnectionInterface::kIceConnectionConnected ||
        new_state == webrtc::PeerConnectionInterface::kIceConnectionCompleted;

    if (connected != connected_) {
      connected_ = connected;
      delegate_->OnIceConnectionChange(connected_);
    }
  }

  virtual void OnIceCandidate(const webrtc::IceCandidateInterface* candidate)
      override {
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

  virtual ~PeerConnectionHolder() {
    DCHECK(disposed_);
  }

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

  virtual void OnSuccess(webrtc::SessionDescriptionInterface* desc) override {
    if (holder_->IsDisposed()) return;

    type_ = desc->type();
    if (desc->ToString(&description_)) {
      holder_->connection()->SetLocalDescription(this, desc);
    } else {
      OnFailure("Can't serialize session description");
    }
  }

  virtual void OnSuccess() override {
    if (holder_->IsDisposed()) return;

    if (type_ == webrtc::SessionDescriptionInterface::kOffer) {
      holder_->delegate()->OnLocalOfferCreatedAndSetSet(description_);
    } else {
      DCHECK_EQ(webrtc::SessionDescriptionInterface::kAnswer, type_);

      holder_->delegate()->OnLocalAnswerCreatedAndSetSet(description_);
    }
  }

  virtual void OnFailure(const std::string& error) override {
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

  virtual void OnSuccess() override {
    if (holder_->IsDisposed()) return;

    holder_->delegate()->OnRemoteDescriptionSet();
  }

  virtual void OnFailure(const std::string& error) override {
    if (holder_->IsDisposed()) return;

    holder_->delegate()->OnFailure(error);
  }

 private:
  const rtc::scoped_refptr<PeerConnectionHolder> holder_;
};

class PeerConnectionImpl : public AbstractPeerConnection {
 public:
  PeerConnectionImpl(
      rtc::Thread* signaling_thread,
      rtc::scoped_refptr<webrtc::PeerConnectionInterface> connection,
      scoped_ptr<PeerConnectionObserverImpl> observer,
      scoped_ptr<AbstractPeerConnection::Delegate> delegate)
      : holder_(new rtc::RefCountedObject<PeerConnectionHolder>(
            signaling_thread, connection.get(), delegate.get())),
        signaling_thread_(signaling_thread),
        connection_(connection),
        observer_(observer.Pass()),
        delegate_(delegate.Pass()) {
  }

  virtual ~PeerConnectionImpl() {
    signaling_thread_->Invoke<void>(rtc::Bind(
        &PeerConnectionImpl::DisposeOnSignalingThread, this));
  }

  virtual void CreateAndSetLocalOffer() override {
    connection_->CreateOffer(MakeCreateAndSetHandler(), NULL);
  }

  virtual void CreateAndSetLocalAnswer() override {
    connection_->CreateAnswer(MakeCreateAndSetHandler(), NULL);
  }

  virtual void SetRemoteOffer(const std::string& description) override {
    SetRemoteDescription(
        webrtc::SessionDescriptionInterface::kOffer, description);
  }

  virtual void SetRemoteAnswer(const std::string& description) override {
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

  virtual void AddIceCandidate(
      const std::string& sdp_mid,
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

  virtual scoped_ptr<AbstractDataChannel> CreateDataChannel(
      int channelId) override {
    webrtc::DataChannelInit init;
    init.ordered = true;
    init.negotiated = true;
    init.id = channelId;

    return make_scoped_ptr(new DataChannelImpl(
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

  virtual ~SessionDependencyFactoryImpl() {
    signaling_thread_.Invoke<void>(rtc::Bind(
        &SessionDependencyFactoryImpl::DisposeOnSignalingThread, this));
  }

  virtual scoped_ptr<AbstractPeerConnection> CreatePeerConnection(
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
        &signaling_thread_, connection, observer.Pass(), delegate.Pass()));
  }

 private:
  void DisposeOnSignalingThread() {
    DCHECK(signaling_thread_.IsCurrent());
    CheckedRelease(&factory_);
    if (!cleanup_on_signaling_thread_.is_null()) {
      cleanup_on_signaling_thread_.Run();
    }
  }

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
