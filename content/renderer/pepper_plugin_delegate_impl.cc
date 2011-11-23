// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper_plugin_delegate_impl.h"

#include <cmath>
#include <queue>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util_proxy.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_split.h"
#include "base/sync_socket.h"
#include "base/task.h"
#include "base/time.h"
#include "content/common/child_process.h"
#include "content/common/child_process_messages.h"
#include "content/common/child_thread.h"
#include "content/common/file_system/file_system_dispatcher.h"
#include "content/common/file_system_messages.h"
#include "content/common/media/audio_messages.h"
#include "content/common/pepper_file_messages.h"
#include "content/common/pepper_plugin_registry.h"
#include "content/common/pepper_messages.h"
#include "content/common/quota_dispatcher.h"
#include "content/common/view_messages.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/gpu/command_buffer_proxy.h"
#include "content/renderer/gpu/gpu_channel_host.h"
#include "content/renderer/gpu/renderer_gl_context.h"
#include "content/renderer/gpu/webgraphicscontext3d_command_buffer_impl.h"
#include "content/renderer/media/audio_input_message_filter.h"
#include "content/renderer/media/audio_message_filter.h"
#include "content/renderer/media/video_capture_impl_manager.h"
#include "content/renderer/p2p/p2p_transport_impl.h"
#include "content/renderer/pepper_platform_context_3d_impl.h"
#include "content/renderer/pepper_platform_video_decoder_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/render_widget_fullscreen_pepper.h"
#include "content/renderer/webplugin_delegate_proxy.h"
#include "ipc/ipc_channel_handle.h"
#include "media/audio/audio_manager_base.h"
#include "media/video/capture/video_capture_proxy.h"
#include "ppapi/c/dev/pp_video_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_flash.h"
#include "ppapi/c/private/ppb_flash_net_connector.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/platform_file.h"
#include "ppapi/shared_impl/ppapi_preferences.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCursorInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFileChooserCompletion.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFileChooserParams.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginContainer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScreenInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/gfx/size.h"
#include "ui/gfx/surface/transport_dib.h"
#include "webkit/fileapi/file_system_callback_dispatcher.h"
#include "webkit/glue/context_menu.h"
#include "webkit/plugins/npapi/webplugin.h"
#include "webkit/plugins/ppapi/file_path.h"
#include "webkit/plugins/ppapi/ppb_file_io_impl.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/ppb_broker_impl.h"
#include "webkit/plugins/ppapi/ppb_flash_impl.h"
#include "webkit/plugins/ppapi/ppb_flash_net_connector_impl.h"
#include "webkit/plugins/ppapi/resource_helper.h"
#include "webkit/plugins/webplugininfo.h"

using WebKit::WebView;

namespace {

base::SyncSocket::Handle DuplicateHandle(base::SyncSocket::Handle handle) {
  base::SyncSocket::Handle out_handle = base::kInvalidPlatformFileValue;
#if defined(OS_WIN)
  DWORD options = DUPLICATE_SAME_ACCESS;
  if (!::DuplicateHandle(::GetCurrentProcess(),
                         handle,
                         ::GetCurrentProcess(),
                         &out_handle,
                         0,
                         FALSE,
                         options)) {
    out_handle = base::kInvalidPlatformFileValue;
  }
#elif defined(OS_POSIX)
  // If asked to close the source, we can simply re-use the source fd instead of
  // dup()ing and close()ing.
  out_handle = ::dup(handle);
#else
  #error Not implemented.
#endif
  return out_handle;
}

// Implements the Image2D using a TransportDIB.
class PlatformImage2DImpl
    : public webkit::ppapi::PluginDelegate::PlatformImage2D {
 public:
  // This constructor will take ownership of the dib pointer.
  // On Mac, we assume that the dib is cached by the browser, so on destruction
  // we'll tell the browser to free it.
  PlatformImage2DImpl(int width, int height, TransportDIB* dib)
      : width_(width),
        height_(height),
        dib_(dib) {
  }

#if defined(OS_MACOSX)
  // On Mac, we have to tell the browser to free the transport DIB.
  virtual ~PlatformImage2DImpl() {
    if (dib_.get()) {
      RenderThreadImpl::current()->Send(
          new ViewHostMsg_FreeTransportDIB(dib_->id()));
    }
  }
#endif

  virtual skia::PlatformCanvas* Map() {
    return dib_->GetPlatformCanvas(width_, height_);
  }

  virtual intptr_t GetSharedMemoryHandle(uint32* byte_count) const {
    *byte_count = dib_->size();
#if defined(OS_WIN)
    return reinterpret_cast<intptr_t>(dib_->handle());
#elif defined(OS_MACOSX)
    return static_cast<intptr_t>(dib_->handle().fd);
#elif defined(OS_POSIX)
    return static_cast<intptr_t>(dib_->handle());
#endif
  }

  virtual TransportDIB* GetTransportDIB() const {
    return dib_.get();
  }

 private:
  int width_;
  int height_;
  scoped_ptr<TransportDIB> dib_;

  DISALLOW_COPY_AND_ASSIGN(PlatformImage2DImpl);
};

class PlatformAudioImpl
    : public webkit::ppapi::PluginDelegate::PlatformAudio,
      public AudioMessageFilter::Delegate,
      public base::RefCountedThreadSafe<PlatformAudioImpl> {
 public:
  PlatformAudioImpl()
      : client_(NULL), stream_id_(0),
        main_message_loop_proxy_(base::MessageLoopProxy::current()) {
    filter_ = RenderThreadImpl::current()->audio_message_filter();
  }

  virtual ~PlatformAudioImpl() {
    // Make sure we have been shut down. Warning: this will usually happen on
    // the I/O thread!
    DCHECK_EQ(0, stream_id_);
    DCHECK(!client_);
  }

  // Initialize this audio context. StreamCreated() will be called when the
  // stream is created.
  bool Initialize(uint32_t sample_rate, uint32_t sample_count,
       webkit::ppapi::PluginDelegate::PlatformAudioCommonClient* client);

  // PlatformAudio implementation (called on main thread).
  virtual bool StartPlayback() OVERRIDE;
  virtual bool StopPlayback() OVERRIDE;
  virtual void ShutDown() OVERRIDE;

 private:
  // I/O thread backends to above functions.
  void InitializeOnIOThread(const AudioParameters& params);
  void StartPlaybackOnIOThread();
  void StopPlaybackOnIOThread();
  void ShutDownOnIOThread();

  virtual void OnRequestPacket(AudioBuffersState buffers_state) OVERRIDE {
    LOG(FATAL) << "Should never get OnRequestPacket in PlatformAudioImpl";
  }

  virtual void OnStateChanged(AudioStreamState state) OVERRIDE {}

  virtual void OnCreated(base::SharedMemoryHandle handle,
                         uint32 length) OVERRIDE {
    LOG(FATAL) << "Should never get OnCreated in PlatformAudioImpl";
  }

  virtual void OnLowLatencyCreated(base::SharedMemoryHandle handle,
                                   base::SyncSocket::Handle socket_handle,
                                   uint32 length) OVERRIDE;

  virtual void OnVolume(double volume) OVERRIDE {}

  // The client to notify when the stream is created. THIS MUST ONLY BE
  // ACCESSED ON THE MAIN THREAD.
  webkit::ppapi::PluginDelegate::PlatformAudioCommonClient* client_;

  // MessageFilter used to send/receive IPC. THIS MUST ONLY BE ACCESSED ON THE
  // I/O thread except to send messages and get the message loop.
  scoped_refptr<AudioMessageFilter> filter_;

  // Our ID on the MessageFilter. THIS MUST ONLY BE ACCESSED ON THE I/O THREAD
  // or else you could race with the initialize function which sets it.
  int32 stream_id_;

  base::MessageLoopProxy* main_message_loop_proxy_;

  DISALLOW_COPY_AND_ASSIGN(PlatformAudioImpl);
};

bool PlatformAudioImpl::Initialize(
    uint32_t sample_rate, uint32_t sample_count,
    webkit::ppapi::PluginDelegate::PlatformAudioCommonClient* client) {

  DCHECK(client);
  // Make sure we don't call init more than once.
  DCHECK_EQ(0, stream_id_);

  client_ = client;

  AudioParameters params;
  params.format = AudioParameters::AUDIO_PCM_LINEAR;
  params.channels = 2;
  params.sample_rate = sample_rate;
  params.bits_per_sample = 16;
  params.samples_per_packet = sample_count;

  ChildProcess::current()->io_message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &PlatformAudioImpl::InitializeOnIOThread,
                        params));
  return true;
}

bool PlatformAudioImpl::StartPlayback() {
  if (filter_) {
    ChildProcess::current()->io_message_loop()->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &PlatformAudioImpl::StartPlaybackOnIOThread));
    return true;
  }
  return false;
}

bool PlatformAudioImpl::StopPlayback() {
  if (filter_) {
    ChildProcess::current()->io_message_loop()->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &PlatformAudioImpl::StopPlaybackOnIOThread));
    return true;
  }
  return false;
}

void PlatformAudioImpl::ShutDown() {
  // Called on the main thread to stop all audio callbacks. We must only change
  // the client on the main thread, and the delegates from the I/O thread.
  client_ = NULL;
  ChildProcess::current()->io_message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&PlatformAudioImpl::ShutDownOnIOThread, this));
}

void PlatformAudioImpl::InitializeOnIOThread(const AudioParameters& params) {
  stream_id_ = filter_->AddDelegate(this);
  filter_->Send(new AudioHostMsg_CreateStream(stream_id_, params, true));
}

void PlatformAudioImpl::StartPlaybackOnIOThread() {
  if (stream_id_)
    filter_->Send(new AudioHostMsg_PlayStream(stream_id_));
}

void PlatformAudioImpl::StopPlaybackOnIOThread() {
  if (stream_id_)
    filter_->Send(new AudioHostMsg_PauseStream(stream_id_));
}

void PlatformAudioImpl::ShutDownOnIOThread() {
  // Make sure we don't call shutdown more than once.
  if (!stream_id_)
    return;

  filter_->Send(new AudioHostMsg_CloseStream(stream_id_));
  filter_->RemoveDelegate(stream_id_);
  stream_id_ = 0;

  Release();  // Release for the delegate, balances out the reference taken in
              // PepperPluginDelegateImpl::CreateAudio.
}

void PlatformAudioImpl::OnLowLatencyCreated(
    base::SharedMemoryHandle handle, base::SyncSocket::Handle socket_handle,
    uint32 length) {
#if defined(OS_WIN)
  DCHECK(handle);
  DCHECK(socket_handle);
#else
  DCHECK_NE(-1, handle.fd);
  DCHECK_NE(-1, socket_handle);
#endif
  DCHECK(length);

  if (base::MessageLoopProxy::current() == main_message_loop_proxy_) {
    // Must dereference the client only on the main thread. Shutdown may have
    // occurred while the request was in-flight, so we need to NULL check.
    if (client_)
      client_->StreamCreated(handle, length, socket_handle);
  } else {
    main_message_loop_proxy_->PostTask(FROM_HERE,
        NewRunnableMethod(this, &PlatformAudioImpl::OnLowLatencyCreated,
                          handle, socket_handle, length));
  }
}

class PlatformAudioInputImpl
    : public webkit::ppapi::PluginDelegate::PlatformAudioInput,
      public AudioInputMessageFilter::Delegate,
      public base::RefCountedThreadSafe<PlatformAudioInputImpl> {
 public:
  PlatformAudioInputImpl()
      : client_(NULL), stream_id_(0),
        main_message_loop_proxy_(base::MessageLoopProxy::current()) {
    filter_ = RenderThreadImpl::current()->audio_input_message_filter();
  }

  virtual ~PlatformAudioInputImpl() {
    // Make sure we have been shut down. Warning: this will usually happen on
    // the I/O thread!
    DCHECK_EQ(0, stream_id_);
    DCHECK(!client_);
  }

  // Initialize this audio context. StreamCreated() will be called when the
  // stream is created.
  bool Initialize(
    uint32_t sample_rate, uint32_t sample_count,
    webkit::ppapi::PluginDelegate::PlatformAudioCommonClient* client);

  // PlatformAudio implementation (called on main thread).
  virtual bool StartCapture() OVERRIDE;
  virtual bool StopCapture() OVERRIDE;
  virtual void ShutDown() OVERRIDE;

 private:
  // I/O thread backends to above functions.
  void InitializeOnIOThread(const AudioParameters& params);
  void StartCaptureOnIOThread();
  void StopCaptureOnIOThread();
  void ShutDownOnIOThread();

  virtual void OnLowLatencyCreated(base::SharedMemoryHandle handle,
                                   base::SyncSocket::Handle socket_handle,
                                   uint32 length) OVERRIDE;

  virtual void OnVolume(double volume) OVERRIDE {}

  virtual void OnStateChanged(AudioStreamState state) OVERRIDE {}

  virtual void OnDeviceReady(const std::string&) OVERRIDE {}

  // The client to notify when the stream is created. THIS MUST ONLY BE
  // ACCESSED ON THE MAIN THREAD.
  webkit::ppapi::PluginDelegate::PlatformAudioCommonClient* client_;

  // MessageFilter used to send/receive IPC. THIS MUST ONLY BE ACCESSED ON THE
  // I/O thread except to send messages and get the message loop.
  scoped_refptr<AudioInputMessageFilter> filter_;

  // Our ID on the MessageFilter. THIS MUST ONLY BE ACCESSED ON THE I/O THREAD
  // or else you could race with the initialize function which sets it.
  int32 stream_id_;

  base::MessageLoopProxy* main_message_loop_proxy_;

  DISALLOW_COPY_AND_ASSIGN(PlatformAudioInputImpl);
};

bool PlatformAudioInputImpl::Initialize(
    uint32_t sample_rate, uint32_t sample_count,
    webkit::ppapi::PluginDelegate::PlatformAudioCommonClient* client) {
  DCHECK(client);
  // Make sure we don't call init more than once.
  DCHECK_EQ(0, stream_id_);

  client_ = client;

  AudioParameters params;
  params.format = AudioParameters::AUDIO_PCM_LINEAR;
  params.channels = 1;
  params.sample_rate = sample_rate;
  params.bits_per_sample = 16;
  params.samples_per_packet = sample_count;

  ChildProcess::current()->io_message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &PlatformAudioInputImpl::InitializeOnIOThread,
                        params));
  return true;
}

bool PlatformAudioInputImpl::StartCapture() {
  ChildProcess::current()->io_message_loop()->PostTask(
    FROM_HERE,
    NewRunnableMethod(this,
        &PlatformAudioInputImpl::StartCaptureOnIOThread));
  return true;
}

bool PlatformAudioInputImpl::StopCapture() {
  ChildProcess::current()->io_message_loop()->PostTask(
    FROM_HERE,
    NewRunnableMethod(this,
        &PlatformAudioInputImpl::StopCaptureOnIOThread));
  return true;
}

void PlatformAudioInputImpl::ShutDown() {
  // Called on the main thread to stop all audio callbacks. We must only change
  // the client on the main thread, and the delegates from the I/O thread.
  client_ = NULL;
  ChildProcess::current()->io_message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&PlatformAudioInputImpl::ShutDownOnIOThread, this));
}

void PlatformAudioInputImpl::InitializeOnIOThread(
    const AudioParameters& params) {
  stream_id_ = filter_->AddDelegate(this);
  filter_->Send(new AudioInputHostMsg_CreateStream(
      stream_id_, params, true, AudioManagerBase::kDefaultDeviceId));
}

void PlatformAudioInputImpl::StartCaptureOnIOThread() {
  if (stream_id_)
    filter_->Send(new AudioInputHostMsg_RecordStream(stream_id_));
}

void PlatformAudioInputImpl::StopCaptureOnIOThread() {
  if (stream_id_)
    filter_->Send(new AudioInputHostMsg_CloseStream(stream_id_));
}

void PlatformAudioInputImpl::ShutDownOnIOThread() {
  // Make sure we don't call shutdown more than once.
  if (!stream_id_)
    return;

  filter_->Send(new AudioInputHostMsg_CloseStream(stream_id_));
  filter_->RemoveDelegate(stream_id_);
  stream_id_ = 0;

  Release();  // Release for the delegate, balances out the reference taken in
              // PepperPluginDelegateImpl::CreateAudioInput.
}

void PlatformAudioInputImpl::OnLowLatencyCreated(
    base::SharedMemoryHandle handle,
    base::SyncSocket::Handle socket_handle,
    uint32 length) {

#if defined(OS_WIN)
  DCHECK(handle);
  DCHECK(socket_handle);
#else
  DCHECK_NE(-1, handle.fd);
  DCHECK_NE(-1, socket_handle);
#endif
  DCHECK(length);

  if (base::MessageLoopProxy::current() == main_message_loop_proxy_) {
    // Must dereference the client only on the main thread. Shutdown may have
    // occurred while the request was in-flight, so we need to NULL check.
    if (client_)
      client_->StreamCreated(handle, length, socket_handle);
  } else {
    main_message_loop_proxy_->PostTask(FROM_HERE,
        NewRunnableMethod(this, &PlatformAudioInputImpl::OnLowLatencyCreated,
                          handle, socket_handle, length));
  }
}

class DispatcherDelegate : public ppapi::proxy::ProxyChannel::Delegate {
 public:
  virtual ~DispatcherDelegate() {}

  // ProxyChannel::Delegate implementation.
  virtual base::MessageLoopProxy* GetIPCMessageLoop() {
    // This is called only in the renderer so we know we have a child process.
    DCHECK(ChildProcess::current()) << "Must be in the renderer.";
    return ChildProcess::current()->io_message_loop_proxy();
  }
  virtual base::WaitableEvent* GetShutdownEvent() {
    DCHECK(ChildProcess::current()) << "Must be in the renderer.";
    return ChildProcess::current()->GetShutDownEvent();
  }
};

class HostDispatcherWrapper
    : public webkit::ppapi::PluginDelegate::OutOfProcessProxy {
 public:
  HostDispatcherWrapper() {}
  virtual ~HostDispatcherWrapper() {}

  bool Init(base::ProcessHandle plugin_process_handle,
            const IPC::ChannelHandle& channel_handle,
            PP_Module pp_module,
            ppapi::proxy::Dispatcher::GetInterfaceFunc local_get_interface,
            const ppapi::Preferences& preferences) {
    if (channel_handle.name.empty())
      return false;

#if defined(OS_POSIX)
    if (channel_handle.socket.fd == -1)
      return false;
#endif

    dispatcher_delegate_.reset(new DispatcherDelegate);
    dispatcher_.reset(new ppapi::proxy::HostDispatcher(
        plugin_process_handle, pp_module, local_get_interface));

    if (!dispatcher_->InitHostWithChannel(dispatcher_delegate_.get(),
                                          channel_handle,
                                          true,  // Client.
                                          preferences)) {
      dispatcher_.reset();
      dispatcher_delegate_.reset();
      return false;
    }
    dispatcher_->channel()->SetRestrictDispatchToSameChannel(true);
    return true;
  }

  // OutOfProcessProxy implementation.
  virtual const void* GetProxiedInterface(const char* name) {
    return dispatcher_->GetProxiedInterface(name);
  }
  virtual void AddInstance(PP_Instance instance) {
    ppapi::proxy::HostDispatcher::SetForInstance(instance, dispatcher_.get());
  }
  virtual void RemoveInstance(PP_Instance instance) {
    ppapi::proxy::HostDispatcher::RemoveForInstance(instance);
  }

 private:
  scoped_ptr<ppapi::proxy::HostDispatcher> dispatcher_;
  scoped_ptr<ppapi::proxy::ProxyChannel::Delegate> dispatcher_delegate_;
};

class QuotaCallbackTranslator : public QuotaDispatcher::Callback {
 public:
  typedef webkit::ppapi::PluginDelegate::AvailableSpaceCallback PluginCallback;
  explicit QuotaCallbackTranslator(const PluginCallback& cb) : callback_(cb) {}
  virtual void DidQueryStorageUsageAndQuota(int64 usage, int64 quota) OVERRIDE {
    callback_.Run(std::max(static_cast<int64>(0), quota - usage));
  }
  virtual void DidGrantStorageQuota(int64 granted_quota) OVERRIDE {
    NOTREACHED();
  }
  virtual void DidFail(quota::QuotaStatusCode error) OVERRIDE {
    callback_.Run(0);
  }
 private:
  PluginCallback callback_;
};

class PlatformVideoCaptureImpl
    : public webkit::ppapi::PluginDelegate::PlatformVideoCapture {
 public:
  PlatformVideoCaptureImpl(media::VideoCapture::EventHandler* handler)
      : handler_proxy_(new media::VideoCaptureHandlerProxy(
            handler, base::MessageLoopProxy::current())) {
    VideoCaptureImplManager* manager =
        RenderThreadImpl::current()->video_capture_impl_manager();
    // 1 means the "default" video capture device.
    // TODO(piman): Add a way to enumerate devices and pass them through the
    // API.
    video_capture_ = manager->AddDevice(1, handler_proxy_.get());
  }

  // Overrides from media::VideoCapture::EventHandler
  virtual ~PlatformVideoCaptureImpl() {
    VideoCaptureImplManager* manager =
        RenderThreadImpl::current()->video_capture_impl_manager();
    manager->RemoveDevice(1, handler_proxy_.get());
  }

  virtual void StartCapture(
      EventHandler* handler,
      const VideoCaptureCapability& capability) OVERRIDE {
    DCHECK(handler == handler_proxy_->proxied());
    video_capture_->StartCapture(handler_proxy_.get(), capability);
  }

  virtual void StopCapture(EventHandler* handler) OVERRIDE {
    DCHECK(handler == handler_proxy_->proxied());
    video_capture_->StopCapture(handler_proxy_.get());
  }

  virtual void FeedBuffer(scoped_refptr<VideoFrameBuffer> buffer) OVERRIDE {
    video_capture_->FeedBuffer(buffer);
  }

  virtual bool CaptureStarted() OVERRIDE {
    return handler_proxy_->state().started;
  }

  virtual int CaptureWidth() OVERRIDE {
    return handler_proxy_->state().width;
  }

  virtual int CaptureHeight() OVERRIDE {
    return handler_proxy_->state().height;
  }

  virtual int CaptureFrameRate() OVERRIDE {
    return handler_proxy_->state().frame_rate;
  }

 private:
  scoped_ptr<media::VideoCaptureHandlerProxy> handler_proxy_;
  media::VideoCapture* video_capture_;
};

}  // namespace

BrokerDispatcherWrapper::BrokerDispatcherWrapper() {
}

BrokerDispatcherWrapper::~BrokerDispatcherWrapper() {
}

bool BrokerDispatcherWrapper::Init(
    base::ProcessHandle broker_process_handle,
    const IPC::ChannelHandle& channel_handle) {
  if (channel_handle.name.empty())
    return false;

#if defined(OS_POSIX)
  if (channel_handle.socket.fd == -1)
    return false;
#endif

  dispatcher_delegate_.reset(new DispatcherDelegate);
  dispatcher_.reset(
      new ppapi::proxy::BrokerHostDispatcher(broker_process_handle));

  if (!dispatcher_->InitBrokerWithChannel(dispatcher_delegate_.get(),
                                          channel_handle,
                                          true)) {  // Client.
    dispatcher_.reset();
    dispatcher_delegate_.reset();
    return false;
  }
  dispatcher_->channel()->SetRestrictDispatchToSameChannel(true);
  return true;
}

// Does not take ownership of the local pipe.
int32_t BrokerDispatcherWrapper::SendHandleToBroker(
    PP_Instance instance,
    base::SyncSocket::Handle handle) {
  IPC::PlatformFileForTransit foreign_socket_handle =
      dispatcher_->ShareHandleWithRemote(handle, false);
  if (foreign_socket_handle == IPC::InvalidPlatformFileForTransit())
    return PP_ERROR_FAILED;

  int32_t result;
  if (!dispatcher_->Send(
      new PpapiMsg_ConnectToPlugin(instance, foreign_socket_handle, &result))) {
    // The plugin did not receive the handle, so it must be closed.
    // The easiest way to clean it up is to just put it in an object
    // and then close it. This failure case is not performance critical.
    // The handle could still leak if Send succeeded but the IPC later failed.
    base::SyncSocket temp_socket(
        IPC::PlatformFileForTransitToPlatformFile(foreign_socket_handle));
    return PP_ERROR_FAILED;
  }

  return result;
}

PpapiBrokerImpl::PpapiBrokerImpl(webkit::ppapi::PluginModule* plugin_module,
                                 PepperPluginDelegateImpl* delegate)
    : plugin_module_(plugin_module),
      delegate_(delegate->AsWeakPtr()) {
  DCHECK(plugin_module_);
  DCHECK(delegate_);

  plugin_module_->SetBroker(this);
}

PpapiBrokerImpl::~PpapiBrokerImpl() {
  // Report failure to all clients that had pending operations.
  for (ClientMap::iterator i = pending_connects_.begin();
       i != pending_connects_.end(); ++i) {
    base::WeakPtr<webkit::ppapi::PPB_Broker_Impl>& weak_ptr = i->second;
    if (weak_ptr) {
      weak_ptr->BrokerConnected(
          ppapi::PlatformFileToInt(base::kInvalidPlatformFileValue),
          PP_ERROR_ABORTED);
    }
  }
  pending_connects_.clear();

  plugin_module_->SetBroker(NULL);
  plugin_module_ = NULL;
}

// If the channel is not ready, queue the connection.
void PpapiBrokerImpl::Connect(webkit::ppapi::PPB_Broker_Impl* client) {
  DCHECK(pending_connects_.find(client) == pending_connects_.end())
      << "Connect was already called for this client";

  // Ensure this object and the associated broker exist as long as the
  // client exists. There is a corresponding Release() call in Disconnect(),
  // which is called when the PPB_Broker_Impl is destroyed. The only other
  // possible reference is in pending_connect_broker_, which only holds a
  // transient reference. This ensures the broker is available as long as the
  // plugin needs it and allows the plugin to release the broker when it is no
  // longer using it.
  AddRef();

  if (!dispatcher_.get()) {
    pending_connects_[client] = client->AsWeakPtr();
    return;
  }
  DCHECK(pending_connects_.empty());

  ConnectPluginToBroker(client);
}

void PpapiBrokerImpl::Disconnect(webkit::ppapi::PPB_Broker_Impl* client) {
  // Remove the pending connect if one exists. This class will not call client's
  // callback.
  pending_connects_.erase(client);

  // TODO(ddorwin): Send message disconnect message using dispatcher_.

  if (pending_connects_.empty()) {
    // There are no more clients of this broker. Ensure it will be deleted even
    // if the IPC response never comes and OnPpapiBrokerChannelCreated is not
    // called to remove this object from pending_connect_broker_.
    // Before the broker is connected, clients must either be in
    // pending_connects_ or not yet associated with this object. Thus, if this
    // object is in pending_connect_broker_, there can be no associated clients
    // once pending_connects_ is empty and it is thus safe to remove this from
    // pending_connect_broker_. Doing so will cause this object to be deleted,
    // removing it from the PluginModule. Any new clients will create a new
    // instance of this object.
    // This doesn't solve all potential problems, but it helps with the ones
    // we can influence.
    if (delegate_) {
      bool stopped = delegate_->StopWaitingForPpapiBrokerConnection(this);

      // Verify the assumption that there are no references other than the one
      // client holds, which will be released below.
      DCHECK(!stopped || HasOneRef());
    }
  }

  // Release the reference added in Connect().
  // This must be the last statement because it may delete this object.
  Release();
}

void PpapiBrokerImpl::OnBrokerChannelConnected(
    base::ProcessHandle broker_process_handle,
    const IPC::ChannelHandle& channel_handle) {
  scoped_ptr<BrokerDispatcherWrapper> dispatcher(new BrokerDispatcherWrapper);
  if (dispatcher->Init(broker_process_handle, channel_handle)) {
    dispatcher_.reset(dispatcher.release());

    // Process all pending channel requests from the plugins.
    for (ClientMap::iterator i = pending_connects_.begin();
         i != pending_connects_.end(); ++i) {
      base::WeakPtr<webkit::ppapi::PPB_Broker_Impl>& weak_ptr = i->second;
      if (weak_ptr)
        ConnectPluginToBroker(weak_ptr);
    }
  } else {
    // Report failure to all clients.
    for (ClientMap::iterator i = pending_connects_.begin();
         i != pending_connects_.end(); ++i) {
      base::WeakPtr<webkit::ppapi::PPB_Broker_Impl>& weak_ptr = i->second;
      if (weak_ptr) {
        weak_ptr->BrokerConnected(
            ppapi::PlatformFileToInt(base::kInvalidPlatformFileValue),
            PP_ERROR_FAILED);
      }
    }
  }
  pending_connects_.clear();
}

void PpapiBrokerImpl::ConnectPluginToBroker(
    webkit::ppapi::PPB_Broker_Impl* client) {
  base::SyncSocket::Handle plugin_handle = base::kInvalidPlatformFileValue;
  int32_t result = PP_OK;

  base::SyncSocket* sockets[2] = {0};
  if (base::SyncSocket::CreatePair(sockets)) {
    // The socket objects will be deleted when this function exits, closing the
    // handles. Any uses of the socket must duplicate them.
    scoped_ptr<base::SyncSocket> broker_socket(sockets[0]);
    scoped_ptr<base::SyncSocket> plugin_socket(sockets[1]);

    result = dispatcher_->SendHandleToBroker(client->pp_instance(),
                                             broker_socket->handle());

    // If the broker has its pipe handle, duplicate the plugin's handle.
    // Otherwise, the plugin's handle will be automatically closed.
    if (result == PP_OK)
      plugin_handle = DuplicateHandle(plugin_socket->handle());
  } else {
    result = PP_ERROR_FAILED;
  }

  // TOOD(ddorwin): Change the IPC to asynchronous: Queue an object containing
  // client and plugin_socket.release(), then return.
  // That message handler will then call client->BrokerConnected() with the
  // saved pipe handle.
  // Temporarily, just call back.
  client->BrokerConnected(ppapi::PlatformFileToInt(plugin_handle), result);
}

PepperPluginDelegateImpl::PepperPluginDelegateImpl(RenderViewImpl* render_view)
    : render_view_(render_view),
      has_saved_context_menu_action_(false),
      saved_context_menu_action_(0),
      focused_plugin_(NULL),
      mouse_lock_owner_(NULL),
      mouse_locked_(false),
      pending_lock_request_(false),
      pending_unlock_request_(false),
      last_mouse_event_target_(NULL) {
}

PepperPluginDelegateImpl::~PepperPluginDelegateImpl() {
  DCHECK(!mouse_lock_owner_);
}

scoped_refptr<webkit::ppapi::PluginModule>
PepperPluginDelegateImpl::CreatePepperPluginModule(
    const webkit::WebPluginInfo& webplugin_info,
    bool* pepper_plugin_was_registered) {
  *pepper_plugin_was_registered = true;

  // See if a module has already been loaded for this plugin.
  FilePath path(webplugin_info.path);
  scoped_refptr<webkit::ppapi::PluginModule> module =
      PepperPluginRegistry::GetInstance()->GetLiveModule(path);
  if (module)
    return module;

  // In-process plugins will have always been created up-front to avoid the
  // sandbox restrictions. So getting here implies it doesn't exist or should
  // be out of process.
  const content::PepperPluginInfo* info =
      PepperPluginRegistry::GetInstance()->GetInfoForPlugin(webplugin_info);
  if (!info) {
    *pepper_plugin_was_registered = false;
    return scoped_refptr<webkit::ppapi::PluginModule>();
  } else if (!info->is_out_of_process) {
    // In-process plugin not preloaded, it probably couldn't be initialized.
    return scoped_refptr<webkit::ppapi::PluginModule>();
  }

  // Out of process: have the browser start the plugin process for us.
  base::ProcessHandle plugin_process_handle = base::kNullProcessHandle;
  IPC::ChannelHandle channel_handle;
  render_view_->Send(new ViewHostMsg_OpenChannelToPepperPlugin(
      path, &plugin_process_handle, &channel_handle));
  if (channel_handle.name.empty()) {
    // Couldn't be initialized.
    return scoped_refptr<webkit::ppapi::PluginModule>();
  }

  // Create a new HostDispatcher for the proxying, and hook it to a new
  // PluginModule. Note that AddLiveModule must be called before any early
  // returns since the module's destructor will remove itself.
  module = new webkit::ppapi::PluginModule(info->name, path,
                                           PepperPluginRegistry::GetInstance());
  PepperPluginRegistry::GetInstance()->AddLiveModule(path, module);
  scoped_ptr<HostDispatcherWrapper> dispatcher(new HostDispatcherWrapper);
  if (!dispatcher->Init(
          plugin_process_handle,
          channel_handle,
          module->pp_module(),
          webkit::ppapi::PluginModule::GetLocalGetInterfaceFunc(),
          GetPreferences()))
    return scoped_refptr<webkit::ppapi::PluginModule>();
  module->InitAsProxied(dispatcher.release());
  return module;
}

scoped_refptr<PpapiBrokerImpl> PepperPluginDelegateImpl::CreatePpapiBroker(
    webkit::ppapi::PluginModule* plugin_module) {
  DCHECK(plugin_module);
  DCHECK(!plugin_module->GetBroker());

  // The broker path is the same as the plugin.
  const FilePath& broker_path = plugin_module->path();

  scoped_refptr<PpapiBrokerImpl> broker =
      new PpapiBrokerImpl(plugin_module, this);

  int request_id =
      pending_connect_broker_.Add(new scoped_refptr<PpapiBrokerImpl>(broker));

  // Have the browser start the broker process for us.
  IPC::Message* msg =
      new ViewHostMsg_OpenChannelToPpapiBroker(render_view_->routing_id(),
                                               request_id,
                                               broker_path);
  if (!render_view_->Send(msg)) {
    pending_connect_broker_.Remove(request_id);
    return scoped_refptr<PpapiBrokerImpl>();
  }

  return broker;
}

void PepperPluginDelegateImpl::OnPpapiBrokerChannelCreated(
    int request_id,
    base::ProcessHandle broker_process_handle,
    const IPC::ChannelHandle& handle) {
  scoped_refptr<PpapiBrokerImpl>* broker_ptr =
      pending_connect_broker_.Lookup(request_id);
  if (broker_ptr) {
    scoped_refptr<PpapiBrokerImpl> broker = *broker_ptr;
    pending_connect_broker_.Remove(request_id);
    broker->OnBrokerChannelConnected(broker_process_handle, handle);
  } else {
    // There is no broker waiting for this channel. Close it so the broker can
    // clean up and possibly exit.
    // The easiest way to clean it up is to just put it in an object
    // and then close them. This failure case is not performance critical.
    BrokerDispatcherWrapper temp_dispatcher;
    temp_dispatcher.Init(broker_process_handle, handle);
  }
}

// Iterates through pending_connect_broker_ to find the broker.
// Cannot use Lookup() directly because pending_connect_broker_ does not store
// the raw pointer to the broker. Assumes maximum of one copy of broker exists.
bool PepperPluginDelegateImpl::StopWaitingForPpapiBrokerConnection(
    PpapiBrokerImpl* broker) {
  for (BrokerMap::iterator i(&pending_connect_broker_);
       !i.IsAtEnd(); i.Advance()) {
    if (i.GetCurrentValue()->get() == broker) {
      pending_connect_broker_.Remove(i.GetCurrentKey());
      return true;
    }
  }
  return false;
}

void PepperPluginDelegateImpl::ViewInitiatedPaint() {
  // Notify all of our instances that we started painting. This is used for
  // internal bookkeeping only, so we know that the set can not change under
  // us.
  for (std::set<webkit::ppapi::PluginInstance*>::iterator i =
           active_instances_.begin();
       i != active_instances_.end(); ++i)
    (*i)->ViewInitiatedPaint();
}

void PepperPluginDelegateImpl::ViewFlushedPaint() {
  // Notify all instances that we painted. This will call into the plugin, and
  // we it may ask to close itself as a result. This will, in turn, modify our
  // set, possibly invalidating the iterator. So we iterate on a copy that
  // won't change out from under us.
  std::set<webkit::ppapi::PluginInstance*> plugins = active_instances_;
  for (std::set<webkit::ppapi::PluginInstance*>::iterator i = plugins.begin();
       i != plugins.end(); ++i) {
    // The copy above makes sure our iterator is never invalid if some plugins
    // are destroyed. But some plugin may decide to close all of its views in
    // response to a paint in one of them, so we need to make sure each one is
    // still "current" before using it.
    //
    // It's possible that a plugin was destroyed, but another one was created
    // with the same address. In this case, we'll call ViewFlushedPaint on that
    // new plugin. But that's OK for this particular case since we're just
    // notifying all of our instances that the view flushed, and the new one is
    // one of our instances.
    //
    // What about the case where a new one is created in a callback at a new
    // address and we don't issue the callback? We're still OK since this
    // callback is used for flush callbacks and we could not have possibly
    // started a new paint (ViewInitiatedPaint) for the new plugin while
    // processing a previous paint for an existing one.
    if (active_instances_.find(*i) != active_instances_.end())
      (*i)->ViewFlushedPaint();
  }
}

webkit::ppapi::PluginInstance*
PepperPluginDelegateImpl::GetBitmapForOptimizedPluginPaint(
    const gfx::Rect& paint_bounds,
    TransportDIB** dib,
    gfx::Rect* location,
    gfx::Rect* clip) {
  for (std::set<webkit::ppapi::PluginInstance*>::iterator i =
           active_instances_.begin();
       i != active_instances_.end(); ++i) {
    webkit::ppapi::PluginInstance* instance = *i;
    if (instance->GetBitmapForOptimizedPluginPaint(
            paint_bounds, dib, location, clip))
      return *i;
  }
  return NULL;
}

void PepperPluginDelegateImpl::PluginFocusChanged(
    webkit::ppapi::PluginInstance* instance,
    bool focused) {
  if (focused)
    focused_plugin_ = instance;
  else if (focused_plugin_ == instance)
    focused_plugin_ = NULL;
  if (render_view_)
    render_view_->PpapiPluginFocusChanged();
}

void PepperPluginDelegateImpl::PluginTextInputTypeChanged(
    webkit::ppapi::PluginInstance* instance) {
  if (focused_plugin_ == instance && render_view_)
    render_view_->PpapiPluginTextInputTypeChanged();
}

void PepperPluginDelegateImpl::PluginCaretPositionChanged(
    webkit::ppapi::PluginInstance* instance) {
  if (focused_plugin_ == instance && render_view_)
    render_view_->PpapiPluginCaretPositionChanged();
}

void PepperPluginDelegateImpl::PluginRequestedCancelComposition(
    webkit::ppapi::PluginInstance* instance) {
  if (focused_plugin_ == instance && render_view_)
    render_view_->PpapiPluginCancelComposition();
}

void PepperPluginDelegateImpl::OnImeSetComposition(
    const string16& text,
    const std::vector<WebKit::WebCompositionUnderline>& underlines,
    int selection_start,
    int selection_end) {
  if (!IsPluginAcceptingCompositionEvents()) {
    composition_text_ = text;
  } else {
    // TODO(kinaba) currently all composition events are sent directly to
    // plugins. Use DOM event mechanism after WebKit is made aware about
    // plugins that support composition.
    // The code below mimics the behavior of WebCore::Editor::setComposition.

    // Empty -> nonempty: composition started.
    if (composition_text_.empty() && !text.empty())
      focused_plugin_->HandleCompositionStart(string16());
    // Nonempty -> empty: composition canceled.
    if (!composition_text_.empty() && text.empty())
       focused_plugin_->HandleCompositionEnd(string16());
    composition_text_ = text;
    // Nonempty: composition is ongoing.
    if (!composition_text_.empty()) {
      focused_plugin_->HandleCompositionUpdate(composition_text_, underlines,
                                               selection_start, selection_end);
    }
  }
}

void PepperPluginDelegateImpl::OnImeConfirmComposition(const string16& text) {
  // Here, text.empty() has a special meaning. It means to commit the last
  // update of composition text (see RenderWidgetHost::ImeConfirmComposition()).
  const string16& last_text = text.empty() ? composition_text_ : text;

  // last_text is empty only when both text and composition_text_ is. Ignore it.
  if (last_text.empty())
    return;

  if (!IsPluginAcceptingCompositionEvents()) {
    for (size_t i = 0; i < text.size(); ++i) {
      WebKit::WebKeyboardEvent char_event;
      char_event.type = WebKit::WebInputEvent::Char;
      char_event.timeStampSeconds = base::Time::Now().ToDoubleT();
      char_event.modifiers = 0;
      char_event.windowsKeyCode = last_text[i];
      char_event.nativeKeyCode = last_text[i];
      char_event.text[0] = last_text[i];
      char_event.unmodifiedText[0] = last_text[i];
      if (render_view_->webwidget())
        render_view_->webwidget()->handleInputEvent(char_event);
    }
  } else {
    // Mimics the order of events sent by WebKit.
    // See WebCore::Editor::setComposition() for the corresponding code.
    focused_plugin_->HandleCompositionEnd(last_text);
    focused_plugin_->HandleTextInput(last_text);
  }
  composition_text_.clear();
}

gfx::Rect PepperPluginDelegateImpl::GetCaretBounds() const {
  if (!focused_plugin_)
    return gfx::Rect(0, 0, 0, 0);
  return focused_plugin_->GetCaretBounds();
}

ui::TextInputType PepperPluginDelegateImpl::GetTextInputType() const {
  if (!focused_plugin_)
    return ui::TEXT_INPUT_TYPE_NONE;
  return focused_plugin_->text_input_type();
}

bool PepperPluginDelegateImpl::IsPluginAcceptingCompositionEvents() const {
  if (!focused_plugin_)
    return false;
  return focused_plugin_->IsPluginAcceptingCompositionEvents();
}

bool PepperPluginDelegateImpl::CanComposeInline() const {
  return IsPluginAcceptingCompositionEvents();
}

void PepperPluginDelegateImpl::PluginCrashed(
    webkit::ppapi::PluginInstance* instance) {
  render_view_->PluginCrashed(instance->module()->path());

  UnlockMouse(instance);
}

void PepperPluginDelegateImpl::InstanceCreated(
    webkit::ppapi::PluginInstance* instance) {
  active_instances_.insert(instance);

  // Set the initial focus.
  instance->SetContentAreaFocus(render_view_->has_focus());
}

void PepperPluginDelegateImpl::InstanceDeleted(
    webkit::ppapi::PluginInstance* instance) {
  active_instances_.erase(instance);

  if (mouse_lock_owner_ && mouse_lock_owner_ == instance) {
    // UnlockMouse() will determine whether a ViewHostMsg_UnlockMouse needs to
    // be sent, and set internal state properly. We only need to forget about
    // the current |mouse_lock_owner_|.
    UnlockMouse(mouse_lock_owner_);
    mouse_lock_owner_ = NULL;
  }
  if (last_mouse_event_target_ == instance)
    last_mouse_event_target_ = NULL;
  if (focused_plugin_ == instance)
    PluginFocusChanged(instance, false);
}

SkBitmap* PepperPluginDelegateImpl::GetSadPluginBitmap() {
  return content::GetContentClient()->renderer()->GetSadPluginBitmap();
}

webkit::ppapi::PluginDelegate::PlatformImage2D*
PepperPluginDelegateImpl::CreateImage2D(int width, int height) {
  uint32 buffer_size = width * height * 4;

  // Allocate the transport DIB and the PlatformCanvas pointing to it.
#if defined(OS_MACOSX)
  // On the Mac, shared memory has to be created in the browser in order to
  // work in the sandbox.  Do this by sending a message to the browser
  // requesting a TransportDIB (see also
  // chrome/renderer/webplugin_delegate_proxy.cc, method
  // WebPluginDelegateProxy::CreateBitmap() for similar code). The TransportDIB
  // is cached in the browser, and is freed (in typical cases) by the
  // PlatformImage2DImpl's destructor.
  TransportDIB::Handle dib_handle;
  IPC::Message* msg = new ViewHostMsg_AllocTransportDIB(buffer_size,
                                                        true,
                                                        &dib_handle);
  if (!RenderThreadImpl::current()->Send(msg))
    return NULL;
  if (!TransportDIB::is_valid_handle(dib_handle))
    return NULL;

  TransportDIB* dib = TransportDIB::Map(dib_handle);
#else
  static int next_dib_id = 0;
  TransportDIB* dib = TransportDIB::Create(buffer_size, next_dib_id++);
  if (!dib)
    return NULL;
#endif

  return new PlatformImage2DImpl(width, height, dib);
}

webkit::ppapi::PluginDelegate::PlatformContext3D*
    PepperPluginDelegateImpl::CreateContext3D() {
#ifdef ENABLE_GPU
  // If accelerated compositing of plugins is disabled, fail to create a 3D
  // context, because it won't be visible. This allows graceful fallback in the
  // modules.
  if (!render_view_->webkit_preferences().accelerated_plugins_enabled)
    return NULL;
  return new PlatformContext3DImpl(this);
#else
  return NULL;
#endif
}

webkit::ppapi::PluginDelegate::PlatformVideoCapture*
PepperPluginDelegateImpl::CreateVideoCapture(
      media::VideoCapture::EventHandler* handler) {
  return new PlatformVideoCaptureImpl(handler);
}

webkit::ppapi::PluginDelegate::PlatformVideoDecoder*
PepperPluginDelegateImpl::CreateVideoDecoder(
    media::VideoDecodeAccelerator::Client* client,
    int32 command_buffer_route_id) {
  return new PlatformVideoDecoderImpl(client, command_buffer_route_id);
}

void PepperPluginDelegateImpl::NumberOfFindResultsChanged(int identifier,
                                                          int total,
                                                          bool final_result) {
  render_view_->reportFindInPageMatchCount(identifier, total, final_result);
}

void PepperPluginDelegateImpl::SelectedFindResultChanged(int identifier,
                                                         int index) {
  render_view_->reportFindInPageSelection(
      identifier, index + 1, WebKit::WebRect());
}

webkit::ppapi::PluginDelegate::PlatformAudio*
PepperPluginDelegateImpl::CreateAudio(
    uint32_t sample_rate,
    uint32_t sample_count,
    webkit::ppapi::PluginDelegate::PlatformAudioCommonClient* client) {
  scoped_refptr<PlatformAudioImpl> audio(new PlatformAudioImpl());
  if (audio->Initialize(sample_rate, sample_count, client)) {
    // Balanced by Release invoked in PlatformAudioImpl::ShutDownOnIOThread().
    return audio.release();
  } else {
    return NULL;
  }
}

webkit::ppapi::PluginDelegate::PlatformAudioInput*
PepperPluginDelegateImpl::CreateAudioInput(
    uint32_t sample_rate,
    uint32_t sample_count,
    webkit::ppapi::PluginDelegate::PlatformAudioCommonClient* client) {
  scoped_refptr<PlatformAudioInputImpl>
    audio_input(new PlatformAudioInputImpl());
  if (audio_input->Initialize(sample_rate, sample_count, client)) {
    // Balanced by Release invoked in
    // PlatformAudioInputImpl::ShutDownOnIOThread().
    return audio_input.release();
  }
  return NULL;
}

// If a broker has not already been created for this plugin, creates one.
webkit::ppapi::PluginDelegate::PpapiBroker*
PepperPluginDelegateImpl::ConnectToPpapiBroker(
    webkit::ppapi::PPB_Broker_Impl* client) {
  CHECK(client);

  // If a broker needs to be created, this will ensure it does not get deleted
  // before Connect() adds a reference.
  scoped_refptr<PpapiBrokerImpl> broker_impl;

  webkit::ppapi::PluginModule* plugin_module =
      webkit::ppapi::ResourceHelper::GetPluginModule(client);
  PpapiBroker* broker = plugin_module->GetBroker();
  if (!broker) {
    broker_impl = CreatePpapiBroker(plugin_module);
    if (!broker_impl.get())
      return NULL;
    broker = broker_impl;
  }

  // Adds a reference, ensuring not deleted when broker_impl goes out of scope.
  broker->Connect(client);
  return broker;
}

bool PepperPluginDelegateImpl::RunFileChooser(
    const WebKit::WebFileChooserParams& params,
    WebKit::WebFileChooserCompletion* chooser_completion) {
  return render_view_->runFileChooser(params, chooser_completion);
}

bool PepperPluginDelegateImpl::AsyncOpenFile(
    const FilePath& path,
    int flags,
    const AsyncOpenFileCallback& callback) {
  int message_id = pending_async_open_files_.Add(
      new AsyncOpenFileCallback(callback));
  IPC::Message* msg = new ViewHostMsg_AsyncOpenFile(
      render_view_->routing_id(), path, flags, message_id);
  return render_view_->Send(msg);
}

void PepperPluginDelegateImpl::OnAsyncFileOpened(
    base::PlatformFileError error_code,
    base::PlatformFile file,
    int message_id) {
  AsyncOpenFileCallback* callback =
      pending_async_open_files_.Lookup(message_id);
  DCHECK(callback);
  pending_async_open_files_.Remove(message_id);
  callback->Run(error_code, base::PassPlatformFile(&file));
  // Make sure we won't leak file handle if the requester has died.
  if (file != base::kInvalidPlatformFileValue)
    base::FileUtilProxy::Close(GetFileThreadMessageLoopProxy(), file,
                               base::FileUtilProxy::StatusCallback());
  delete callback;
}

void PepperPluginDelegateImpl::OnSetFocus(bool has_focus) {
  for (std::set<webkit::ppapi::PluginInstance*>::iterator i =
           active_instances_.begin();
       i != active_instances_.end(); ++i)
    (*i)->SetContentAreaFocus(has_focus);
}

bool PepperPluginDelegateImpl::IsPluginFocused() const {
  return focused_plugin_ != NULL;
}

void PepperPluginDelegateImpl::OnLockMouseACK(bool succeeded) {
  DCHECK(!mouse_locked_ && pending_lock_request_);

  mouse_locked_ = succeeded;
  pending_lock_request_ = false;
  if (pending_unlock_request_ && !succeeded) {
    // We have sent an unlock request after the lock request. However, since
    // the lock request has failed, the unlock request will be ignored by the
    // browser side and there won't be any response to it.
    pending_unlock_request_ = false;
  }
  // If the PluginInstance has been deleted, |mouse_lock_owner_| can be NULL.
  if (mouse_lock_owner_) {
    webkit::ppapi::PluginInstance* last_mouse_lock_owner = mouse_lock_owner_;
    if (!succeeded) {
      // Reset |mouse_lock_owner_| to NULL before calling OnLockMouseACK(), so
      // that if OnLockMouseACK() results in calls to any mouse lock method
      // (e.g., LockMouse()), the method will see consistent internal state.
      mouse_lock_owner_ = NULL;
    }

    last_mouse_lock_owner->OnLockMouseACK(succeeded ? PP_OK : PP_ERROR_FAILED);
  }
}

void PepperPluginDelegateImpl::OnMouseLockLost() {
  DCHECK(mouse_locked_ && !pending_lock_request_);

  mouse_locked_ = false;
  pending_unlock_request_ = false;
  // If the PluginInstance has been deleted, |mouse_lock_owner_| can be NULL.
  if (mouse_lock_owner_) {
    // Reset |mouse_lock_owner_| to NULL before calling OnMouseLockLost(), so
    // that if OnMouseLockLost() results in calls to any mouse lock method
    // (e.g., LockMouse()), the method will see consistent internal state.
    webkit::ppapi::PluginInstance* last_mouse_lock_owner = mouse_lock_owner_;
    mouse_lock_owner_ = NULL;

    last_mouse_lock_owner->OnMouseLockLost();
  }
}

bool PepperPluginDelegateImpl::HandleMouseEvent(
    const WebKit::WebMouseEvent& event) {
  // This method is called for every mouse event that the render view receives.
  // And then the mouse event is forwarded to WebKit, which dispatches it to the
  // event target. Potentially a Pepper plugin will receive the event.
  // In order to tell whether a plugin gets the last mouse event and which it
  // is, we set |last_mouse_event_target_| to NULL here. If a plugin gets the
  // event, it will notify us via DidReceiveMouseEvent() and set itself as
  // |last_mouse_event_target_|.
  last_mouse_event_target_ = NULL;

  if (mouse_locked_) {
    if (mouse_lock_owner_) {
      // |cursor_info| is ignored since it is hidden when the mouse is locked.
      WebKit::WebCursorInfo cursor_info;
      mouse_lock_owner_->HandleInputEvent(event, &cursor_info);
    }

    // If the mouse is locked, only the current owner of the mouse lock can
    // process mouse events.
    return true;
  }
  return false;
}

bool PepperPluginDelegateImpl::OpenFileSystem(
    const GURL& url,
    fileapi::FileSystemType type,
    long long size,
    fileapi::FileSystemCallbackDispatcher* dispatcher) {
  FileSystemDispatcher* file_system_dispatcher =
      ChildThread::current()->file_system_dispatcher();
  return file_system_dispatcher->OpenFileSystem(
      url.GetWithEmptyPath(), type, size, true /* create */, dispatcher);
}

bool PepperPluginDelegateImpl::MakeDirectory(
    const GURL& path,
    bool recursive,
    fileapi::FileSystemCallbackDispatcher* dispatcher) {
  FileSystemDispatcher* file_system_dispatcher =
      ChildThread::current()->file_system_dispatcher();
  return file_system_dispatcher->Create(
      path, false, true, recursive, dispatcher);
}

bool PepperPluginDelegateImpl::Query(
    const GURL& path,
    fileapi::FileSystemCallbackDispatcher* dispatcher) {
  FileSystemDispatcher* file_system_dispatcher =
      ChildThread::current()->file_system_dispatcher();
  return file_system_dispatcher->ReadMetadata(path, dispatcher);
}

bool PepperPluginDelegateImpl::Touch(
    const GURL& path,
    const base::Time& last_access_time,
    const base::Time& last_modified_time,
    fileapi::FileSystemCallbackDispatcher* dispatcher) {
  FileSystemDispatcher* file_system_dispatcher =
      ChildThread::current()->file_system_dispatcher();
  return file_system_dispatcher->TouchFile(path, last_access_time,
                                           last_modified_time, dispatcher);
}

bool PepperPluginDelegateImpl::Delete(
    const GURL& path,
    fileapi::FileSystemCallbackDispatcher* dispatcher) {
  FileSystemDispatcher* file_system_dispatcher =
      ChildThread::current()->file_system_dispatcher();
  return file_system_dispatcher->Remove(path, false /* recursive */,
                                        dispatcher);
}

bool PepperPluginDelegateImpl::Rename(
    const GURL& file_path,
    const GURL& new_file_path,
    fileapi::FileSystemCallbackDispatcher* dispatcher) {
  FileSystemDispatcher* file_system_dispatcher =
      ChildThread::current()->file_system_dispatcher();
  return file_system_dispatcher->Move(file_path, new_file_path, dispatcher);
}

bool PepperPluginDelegateImpl::ReadDirectory(
    const GURL& directory_path,
    fileapi::FileSystemCallbackDispatcher* dispatcher) {
  FileSystemDispatcher* file_system_dispatcher =
      ChildThread::current()->file_system_dispatcher();
  return file_system_dispatcher->ReadDirectory(directory_path, dispatcher);
}

void PepperPluginDelegateImpl::QueryAvailableSpace(
    const GURL& origin, quota::StorageType type,
    const AvailableSpaceCallback& callback) {
  ChildThread::current()->quota_dispatcher()->QueryStorageUsageAndQuota(
      origin, type, new QuotaCallbackTranslator(callback));
}

void PepperPluginDelegateImpl::WillUpdateFile(const GURL& path) {
  ChildThread::current()->Send(new FileSystemHostMsg_WillUpdate(path));
}

void PepperPluginDelegateImpl::DidUpdateFile(const GURL& path, int64_t delta) {
  ChildThread::current()->Send(new FileSystemHostMsg_DidUpdate(path, delta));
}

class AsyncOpenFileSystemURLCallbackTranslator
    : public fileapi::FileSystemCallbackDispatcher {
 public:
  AsyncOpenFileSystemURLCallbackTranslator(
      const webkit::ppapi::PluginDelegate::AsyncOpenFileCallback& callback)
    : callback_(callback) {
  }

  virtual ~AsyncOpenFileSystemURLCallbackTranslator() {}

  virtual void DidSucceed() {
    NOTREACHED();
  }
  virtual void DidReadMetadata(
      const base::PlatformFileInfo& file_info,
      const FilePath& platform_path) {
    NOTREACHED();
  }
  virtual void DidReadDirectory(
      const std::vector<base::FileUtilProxy::Entry>& entries,
      bool has_more) {
    NOTREACHED();
  }
  virtual void DidOpenFileSystem(const std::string& name,
                                 const GURL& root) {
    NOTREACHED();
  }

  virtual void DidFail(base::PlatformFileError error_code) {
    base::PlatformFile invalid_file = base::kInvalidPlatformFileValue;
    callback_.Run(error_code, base::PassPlatformFile(&invalid_file));
  }

  virtual void DidWrite(int64 bytes, bool complete) {
    NOTREACHED();
  }

  virtual void DidOpenFile(
      base::PlatformFile file,
      base::ProcessHandle unused) {
    callback_.Run(base::PLATFORM_FILE_OK, base::PassPlatformFile(&file));
    // Make sure we won't leak file handle if the requester has died.
    if (file != base::kInvalidPlatformFileValue) {
      base::FileUtilProxy::Close(
          RenderThreadImpl::current()->GetFileThreadMessageLoopProxy(), file,
          base::FileUtilProxy::StatusCallback());
    }
  }

private:  // TODO(ericu): Delete this?
  webkit::ppapi::PluginDelegate::AsyncOpenFileCallback callback_;
};

bool PepperPluginDelegateImpl::AsyncOpenFileSystemURL(
    const GURL& path, int flags, const AsyncOpenFileCallback& callback) {

  FileSystemDispatcher* file_system_dispatcher =
      ChildThread::current()->file_system_dispatcher();
  return file_system_dispatcher->OpenFile(path, flags,
      new AsyncOpenFileSystemURLCallbackTranslator(callback));
}

base::PlatformFileError PepperPluginDelegateImpl::OpenFile(
    const webkit::ppapi::PepperFilePath& path,
    int flags,
    base::PlatformFile* file) {
  IPC::PlatformFileForTransit transit_file;
  base::PlatformFileError error;
  IPC::Message* msg = new PepperFileMsg_OpenFile(
      path, flags, &error, &transit_file);
  if (!render_view_->Send(msg)) {
    *file = base::kInvalidPlatformFileValue;
    return base::PLATFORM_FILE_ERROR_FAILED;
  }
  *file = IPC::PlatformFileForTransitToPlatformFile(transit_file);
  return error;
}

base::PlatformFileError PepperPluginDelegateImpl::RenameFile(
    const webkit::ppapi::PepperFilePath& from_path,
    const webkit::ppapi::PepperFilePath& to_path) {
  base::PlatformFileError error;
  IPC::Message* msg = new PepperFileMsg_RenameFile(from_path, to_path, &error);
  if (!render_view_->Send(msg))
    return base::PLATFORM_FILE_ERROR_FAILED;
  return error;
}

base::PlatformFileError PepperPluginDelegateImpl::DeleteFileOrDir(
    const webkit::ppapi::PepperFilePath& path,
    bool recursive) {
  base::PlatformFileError error;
  IPC::Message* msg = new PepperFileMsg_DeleteFileOrDir(
      path, recursive, &error);
  if (!render_view_->Send(msg))
    return base::PLATFORM_FILE_ERROR_FAILED;
  return error;
}

base::PlatformFileError PepperPluginDelegateImpl::CreateDir(
    const webkit::ppapi::PepperFilePath& path) {
  base::PlatformFileError error;
  IPC::Message* msg = new PepperFileMsg_CreateDir(path, &error);
  if (!render_view_->Send(msg))
    return base::PLATFORM_FILE_ERROR_FAILED;
  return error;
}

base::PlatformFileError PepperPluginDelegateImpl::QueryFile(
    const webkit::ppapi::PepperFilePath& path,
    base::PlatformFileInfo* info) {
  base::PlatformFileError error;
  IPC::Message* msg = new PepperFileMsg_QueryFile(path, info, &error);
  if (!render_view_->Send(msg))
    return base::PLATFORM_FILE_ERROR_FAILED;
  return error;
}

base::PlatformFileError PepperPluginDelegateImpl::GetDirContents(
    const webkit::ppapi::PepperFilePath& path,
    webkit::ppapi::DirContents* contents) {
  base::PlatformFileError error;
  IPC::Message* msg = new PepperFileMsg_GetDirContents(path, contents, &error);
  if (!render_view_->Send(msg))
    return base::PLATFORM_FILE_ERROR_FAILED;
  return error;
}

void PepperPluginDelegateImpl::SyncGetFileSystemPlatformPath(
    const GURL& url, FilePath* platform_path) {
  RenderThreadImpl::current()->Send(new FileSystemHostMsg_SyncGetPlatformPath(
      url, platform_path));
}

scoped_refptr<base::MessageLoopProxy>
PepperPluginDelegateImpl::GetFileThreadMessageLoopProxy() {
  return RenderThreadImpl::current()->GetFileThreadMessageLoopProxy();
}

int32_t PepperPluginDelegateImpl::ConnectTcp(
    webkit::ppapi::PPB_Flash_NetConnector_Impl* connector,
    const char* host,
    uint16_t port) {
  int request_id = pending_connect_tcps_.Add(
      new scoped_refptr<webkit::ppapi::PPB_Flash_NetConnector_Impl>(connector));
  IPC::Message* msg =
      new PepperMsg_ConnectTcp(render_view_->routing_id(),
                               request_id,
                               std::string(host),
                               port);
  if (!render_view_->Send(msg)) {
    pending_connect_tcps_.Remove(request_id);
    return PP_ERROR_FAILED;
  }

  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperPluginDelegateImpl::ConnectTcpAddress(
    webkit::ppapi::PPB_Flash_NetConnector_Impl* connector,
    const struct PP_NetAddress_Private* addr) {
  int request_id = pending_connect_tcps_.Add(
      new scoped_refptr<webkit::ppapi::PPB_Flash_NetConnector_Impl>(connector));
  IPC::Message* msg =
      new PepperMsg_ConnectTcpAddress(render_view_->routing_id(),
                                      request_id,
                                      *addr);
  if (!render_view_->Send(msg)) {
    pending_connect_tcps_.Remove(request_id);
    return PP_ERROR_FAILED;
  }

  return PP_OK_COMPLETIONPENDING;
}

void PepperPluginDelegateImpl::OnConnectTcpACK(
    int request_id,
    base::PlatformFile socket,
    const PP_NetAddress_Private& local_addr,
    const PP_NetAddress_Private& remote_addr) {
  scoped_refptr<webkit::ppapi::PPB_Flash_NetConnector_Impl> connector =
      *pending_connect_tcps_.Lookup(request_id);
  pending_connect_tcps_.Remove(request_id);

  connector->CompleteConnectTcp(socket, local_addr, remote_addr);
}

int32_t PepperPluginDelegateImpl::ShowContextMenu(
    webkit::ppapi::PluginInstance* instance,
    webkit::ppapi::PPB_Flash_Menu_Impl* menu,
    const gfx::Point& position) {
  int32 render_widget_id = render_view_->routing_id();
  if (instance->FlashIsFullscreen(instance->pp_instance())) {
    webkit::ppapi::FullscreenContainer* container =
        instance->fullscreen_container();
    DCHECK(container);
    render_widget_id =
        static_cast<RenderWidgetFullscreenPepper*>(container)->routing_id();
  }

  int request_id = pending_context_menus_.Add(
      new scoped_refptr<webkit::ppapi::PPB_Flash_Menu_Impl>(menu));

  ContextMenuParams params;
  params.x = position.x();
  params.y = position.y();
  params.custom_context.is_pepper_menu = true;
  params.custom_context.request_id = request_id;
  params.custom_context.render_widget_id = render_widget_id;
  params.custom_items = menu->menu_data();

  // Transform the position to be in render view's coordinates.
  if (instance->IsFullscreen(instance->pp_instance()) ||
      instance->FlashIsFullscreen(instance->pp_instance())) {
    WebKit::WebRect rect = render_view_->windowRect();
    params.x -= rect.x;
    params.y -= rect.y;
  } else {
    params.x += instance->position().x();
    params.y += instance->position().y();
  }

  IPC::Message* msg = new ViewHostMsg_ContextMenu(render_view_->routing_id(),
                                                  params);
  if (!render_view_->Send(msg)) {
    pending_context_menus_.Remove(request_id);
    return PP_ERROR_FAILED;
  }

  return PP_OK_COMPLETIONPENDING;
}

void PepperPluginDelegateImpl::OnContextMenuClosed(
    const webkit_glue::CustomContextMenuContext& custom_context) {
  int request_id = custom_context.request_id;
  scoped_refptr<webkit::ppapi::PPB_Flash_Menu_Impl>* menu_ptr =
      pending_context_menus_.Lookup(request_id);
  if (!menu_ptr) {
    NOTREACHED() << "CompleteShowContextMenu() called twice for the same menu.";
    return;
  }
  scoped_refptr<webkit::ppapi::PPB_Flash_Menu_Impl> menu = *menu_ptr;
  DCHECK(menu.get());
  pending_context_menus_.Remove(request_id);

  if (has_saved_context_menu_action_) {
    menu->CompleteShow(PP_OK, saved_context_menu_action_);
    has_saved_context_menu_action_ = false;
    saved_context_menu_action_ = 0;
  } else {
    menu->CompleteShow(PP_ERROR_USERCANCEL, 0);
  }
}

void PepperPluginDelegateImpl::OnCustomContextMenuAction(
    const webkit_glue::CustomContextMenuContext& custom_context,
    unsigned action) {
  // Just save the action.
  DCHECK(!has_saved_context_menu_action_);
  has_saved_context_menu_action_ = true;
  saved_context_menu_action_ = action;
}

webkit::ppapi::FullscreenContainer*
PepperPluginDelegateImpl::CreateFullscreenContainer(
    webkit::ppapi::PluginInstance* instance) {
  return render_view_->CreatePepperFullscreenContainer(instance);
}

gfx::Size PepperPluginDelegateImpl::GetScreenSize() {
  WebKit::WebScreenInfo info = render_view_->screenInfo();
  return gfx::Size(info.rect.width, info.rect.height);
}

std::string PepperPluginDelegateImpl::GetDefaultEncoding() {
  // TODO(brettw) bug 56615: Somehow get the preference for the default
  // encoding here rather than using the global default for the UI language.
  return content::GetContentClient()->renderer()->GetDefaultEncoding();
}

void PepperPluginDelegateImpl::ZoomLimitsChanged(double minimum_factor,
                                                 double maximum_factor) {
  double minimum_level = WebView::zoomFactorToZoomLevel(minimum_factor);
  double maximum_level = WebView::zoomFactorToZoomLevel(maximum_factor);
  render_view_->webview()->zoomLimitsChanged(minimum_level, maximum_level);
}

std::string PepperPluginDelegateImpl::ResolveProxy(const GURL& url) {
  bool result;
  std::string proxy_result;
  RenderThreadImpl::current()->Send(
      new ViewHostMsg_ResolveProxy(url, &result, &proxy_result));
  return proxy_result;
}

void PepperPluginDelegateImpl::DidStartLoading() {
  render_view_->DidStartLoadingForPlugin();
}

void PepperPluginDelegateImpl::DidStopLoading() {
  render_view_->DidStopLoadingForPlugin();
}

void PepperPluginDelegateImpl::SetContentRestriction(int restrictions) {
  render_view_->Send(new ViewHostMsg_UpdateContentRestrictions(
      render_view_->routing_id(), restrictions));
}

void PepperPluginDelegateImpl::SaveURLAs(const GURL& url) {
  render_view_->Send(new ViewHostMsg_SaveURLAs(
      render_view_->routing_id(), url));
}

webkit_glue::P2PTransport* PepperPluginDelegateImpl::CreateP2PTransport() {
#if defined(ENABLE_P2P_APIS)
  return new content::P2PTransportImpl(render_view_->p2p_socket_dispatcher());
#else
  return NULL;
#endif
}

double PepperPluginDelegateImpl::GetLocalTimeZoneOffset(base::Time t) {
  double result = 0.0;
  render_view_->Send(new PepperMsg_GetLocalTimeZoneOffset(
      t, &result));
  return result;
}

std::string PepperPluginDelegateImpl::GetFlashCommandLineArgs() {
  return CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      switches::kPpapiFlashArgs);
}

base::SharedMemory* PepperPluginDelegateImpl::CreateAnonymousSharedMemory(
    uint32_t size) {
  if (size == 0)
    return NULL;
  base::SharedMemoryHandle handle;
  if (!render_view_->Send(
          new ChildProcessHostMsg_SyncAllocateSharedMemory(size, &handle))) {
    DLOG(WARNING) << "Browser allocation request message failed";
    return NULL;
  }
  if (!base::SharedMemory::IsHandleValid(handle)) {
    DLOG(WARNING) << "Browser failed to allocate shared memory";
    return NULL;
  }
  return new base::SharedMemory(handle, false);
}

ppapi::Preferences PepperPluginDelegateImpl::GetPreferences() {
  return ppapi::Preferences(render_view_->webkit_preferences());
}

void PepperPluginDelegateImpl::LockMouse(
    webkit::ppapi::PluginInstance* instance) {
  DCHECK(instance);
  if (!MouseLockedOrPending()) {
    DCHECK(!mouse_lock_owner_);
    pending_lock_request_ = true;
    mouse_lock_owner_ = instance;

    render_view_->Send(
        new ViewHostMsg_LockMouse(render_view_->routing_id()));
  } else if (instance != mouse_lock_owner_) {
    // Another plugin instance is using mouse lock. Fail immediately.
    instance->OnLockMouseACK(PP_ERROR_FAILED);
  } else {
    if (mouse_locked_) {
      instance->OnLockMouseACK(PP_OK);
    } else if (pending_lock_request_) {
      instance->OnLockMouseACK(PP_ERROR_INPROGRESS);
    } else {
      // The only case left here is
      // !mouse_locked_ && !pending_lock_request_ && pending_unlock_request_,
      // which is not possible.
      NOTREACHED();
      instance->OnLockMouseACK(PP_ERROR_FAILED);
    }
  }
}

void PepperPluginDelegateImpl::UnlockMouse(
    webkit::ppapi::PluginInstance* instance) {
  DCHECK(instance);

  // If no one is using mouse lock or the user is not |instance|, ignore
  // the unlock request.
  if (MouseLockedOrPending() && mouse_lock_owner_ == instance) {
    if (mouse_locked_ || pending_lock_request_) {
      DCHECK(!mouse_locked_ || !pending_lock_request_);
      if (!pending_unlock_request_) {
        pending_unlock_request_ = true;

        render_view_->Send(
            new ViewHostMsg_UnlockMouse(render_view_->routing_id()));
      }
    } else {
      // The only case left here is
      // !mouse_locked_ && !pending_lock_request_ && pending_unlock_request_,
      // which is not possible.
      NOTREACHED();
    }
  }
}

void PepperPluginDelegateImpl::DidChangeCursor(
    webkit::ppapi::PluginInstance* instance,
    const WebKit::WebCursorInfo& cursor) {
  // Update the cursor appearance immediately if the requesting plugin is the
  // one which receives the last mouse event. Otherwise, the new cursor won't be
  // picked up until the plugin gets the next input event. That is bad if, e.g.,
  // the plugin would like to set an invisible cursor when there isn't any user
  // input for a while.
  if (instance == last_mouse_event_target_)
    render_view_->didChangeCursor(cursor);
}

void PepperPluginDelegateImpl::DidReceiveMouseEvent(
    webkit::ppapi::PluginInstance* instance) {
  last_mouse_event_target_ = instance;
}

bool PepperPluginDelegateImpl::IsInFullscreenMode() {
  return render_view_->is_fullscreen();
}

int PepperPluginDelegateImpl::GetRoutingId() const {
  return render_view_->routing_id();
}

RendererGLContext*
PepperPluginDelegateImpl::GetParentContextForPlatformContext3D() {
  WebGraphicsContext3DCommandBufferImpl* context =
      static_cast<WebGraphicsContext3DCommandBufferImpl*>(
          render_view_->webview()->graphicsContext3D());
  if (!context)
    return NULL;
  if (!context->makeContextCurrent() || context->isContextLost())
    return NULL;

  RendererGLContext* parent_context = context->context();
  if (!parent_context)
    return NULL;
  return parent_context;
}
