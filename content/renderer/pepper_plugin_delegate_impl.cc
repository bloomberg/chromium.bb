// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper_plugin_delegate_impl.h"

#include <cmath>
#include <queue>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_split.h"
#include "base/sync_socket.h"
#include "base/task.h"
#include "base/time.h"
#include "content/common/audio_messages.h"
#include "content/common/child_process_messages.h"
#include "content/common/child_thread.h"
#include "content/common/content_switches.h"
#include "content/common/file_system/file_system_dispatcher.h"
#include "content/common/pepper_file_messages.h"
#include "content/common/pepper_plugin_registry.h"
#include "content/common/pepper_messages.h"
#include "content/common/view_messages.h"
#include "content/renderer/audio_message_filter.h"
#include "content/renderer/command_buffer_proxy.h"
#include "content/renderer/content_renderer_client.h"
#include "content/renderer/renderer_gl_context.h"
#include "content/renderer/gpu_channel_host.h"
#include "content/renderer/p2p/p2p_transport_impl.h"
#include "content/renderer/pepper_platform_context_3d_impl.h"
#include "content/renderer/pepper_platform_video_decoder_impl.h"
#include "content/renderer/render_thread.h"
#include "content/renderer/render_view.h"
#include "content/renderer/render_widget_fullscreen_pepper.h"
#include "content/renderer/webgraphicscontext3d_command_buffer_impl.h"
#include "content/renderer/webplugin_delegate_proxy.h"
#include "ipc/ipc_channel_handle.h"
#include "ppapi/c/dev/pp_video_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_flash.h"
#include "ppapi/c/private/ppb_flash_net_connector.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFileChooserCompletion.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFileChooserParams.h"
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

using WebKit::WebView;

namespace {

const int32 kDefaultCommandBufferSize = 1024 * 1024;

int32_t PlatformFileToInt(base::PlatformFile handle) {
#if defined(OS_WIN)
  return static_cast<int32_t>(reinterpret_cast<intptr_t>(handle));
#elif defined(OS_POSIX)
  return handle;
#else
  #error Not implemented.
#endif
}

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
      RenderThread::current()->Send(
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
#elif defined(OS_LINUX)
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
  explicit PlatformAudioImpl(scoped_refptr<AudioMessageFilter> filter)
      : client_(NULL), filter_(filter), stream_id_(0),
        main_message_loop_(MessageLoop::current()) {
    DCHECK(filter_);
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
       webkit::ppapi::PluginDelegate::PlatformAudio::Client* client);

  // PlatformAudio implementation (called on main thread).
  virtual bool StartPlayback();
  virtual bool StopPlayback();
  virtual void ShutDown();

 private:
  // I/O thread backends to above functions.
  void InitializeOnIOThread(const AudioParameters& params);
  void StartPlaybackOnIOThread();
  void StopPlaybackOnIOThread();
  void ShutDownOnIOThread();

  virtual void OnRequestPacket(AudioBuffersState buffers_state) {
    LOG(FATAL) << "Should never get OnRequestPacket in PlatformAudioImpl";
  }

  virtual void OnStateChanged(AudioStreamState state) {}

  virtual void OnCreated(base::SharedMemoryHandle handle, uint32 length) {
    LOG(FATAL) << "Should never get OnCreated in PlatformAudioImpl";
  }

  virtual void OnLowLatencyCreated(base::SharedMemoryHandle handle,
                                   base::SyncSocket::Handle socket_handle,
                                   uint32 length);

  virtual void OnVolume(double volume) {}

  // The client to notify when the stream is created. THIS MUST ONLY BE
  // ACCESSED ON THE MAIN THREAD.
  webkit::ppapi::PluginDelegate::PlatformAudio::Client* client_;

  // MessageFilter used to send/receive IPC. THIS MUST ONLY BE ACCESSED ON THE
  // I/O thread except to send messages and get the message loop.
  scoped_refptr<AudioMessageFilter> filter_;

  // Our ID on the MessageFilter. THIS MUST ONLY BE ACCESSED ON THE I/O THREAD
  // or else you could race with the initialize function which sets it.
  int32 stream_id_;

  MessageLoop* main_message_loop_;

  DISALLOW_COPY_AND_ASSIGN(PlatformAudioImpl);
};

bool PlatformAudioImpl::Initialize(
    uint32_t sample_rate, uint32_t sample_count,
    webkit::ppapi::PluginDelegate::PlatformAudio::Client* client) {

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

  filter_->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(this, &PlatformAudioImpl::InitializeOnIOThread,
                        params));
  return true;
}

bool PlatformAudioImpl::StartPlayback() {
  if (filter_) {
    filter_->message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(this, &PlatformAudioImpl::StartPlaybackOnIOThread));
    return true;
  }
  return false;
}

bool PlatformAudioImpl::StopPlayback() {
  if (filter_) {
    filter_->message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(this, &PlatformAudioImpl::StopPlaybackOnIOThread));
    return true;
  }
  return false;
}

void PlatformAudioImpl::ShutDown() {
  // Called on the main thread to stop all audio callbacks. We must only change
  // the client on the main thread, and the delegates from the I/O thread.
  client_ = NULL;
  filter_->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(this, &PlatformAudioImpl::ShutDownOnIOThread));
}

void PlatformAudioImpl::InitializeOnIOThread(const AudioParameters& params) {
  stream_id_ = filter_->AddDelegate(this);
  filter_->Send(new AudioHostMsg_CreateStream(0, stream_id_, params, true));
}

void PlatformAudioImpl::StartPlaybackOnIOThread() {
  if (stream_id_)
    filter_->Send(new AudioHostMsg_PlayStream(0, stream_id_));
}

void PlatformAudioImpl::StopPlaybackOnIOThread() {
  if (stream_id_)
    filter_->Send(new AudioHostMsg_PauseStream(0, stream_id_));
}

void PlatformAudioImpl::ShutDownOnIOThread() {
  // Make sure we don't call shutdown more than once.
  if (!stream_id_)
    return;

  filter_->Send(new AudioHostMsg_CloseStream(0, stream_id_));
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

  if (MessageLoop::current() == main_message_loop_) {
    // Must dereference the client only on the main thread. Shutdown may have
    // occurred while the request was in-flight, so we need to NULL check.
    if (client_)
      client_->StreamCreated(handle, length, socket_handle);
  } else {
    main_message_loop_->PostTask(FROM_HERE,
        NewRunnableMethod(this, &PlatformAudioImpl::OnLowLatencyCreated,
                          handle, socket_handle, length));
  }
}

class DispatcherWrapper
    : public webkit::ppapi::PluginDelegate::OutOfProcessProxy {
 public:
  DispatcherWrapper() {}
  virtual ~DispatcherWrapper() {}

  bool Init(base::ProcessHandle plugin_process_handle,
            const IPC::ChannelHandle& channel_handle,
            PP_Module pp_module,
            pp::proxy::Dispatcher::GetInterfaceFunc local_get_interface);

  // OutOfProcessProxy implementation.
  virtual const void* GetProxiedInterface(const char* name) {
    return dispatcher_->GetProxiedInterface(name);
  }
  virtual void AddInstance(PP_Instance instance) {
    pp::proxy::HostDispatcher::SetForInstance(instance, dispatcher_.get());
  }
  virtual void RemoveInstance(PP_Instance instance) {
    pp::proxy::HostDispatcher::RemoveForInstance(instance);
  }

 private:
  scoped_ptr<pp::proxy::HostDispatcher> dispatcher_;
};

}  // namespace

bool DispatcherWrapper::Init(
    base::ProcessHandle plugin_process_handle,
    const IPC::ChannelHandle& channel_handle,
    PP_Module pp_module,
    pp::proxy::Dispatcher::GetInterfaceFunc local_get_interface) {
  dispatcher_.reset(new pp::proxy::HostDispatcher(
      plugin_process_handle, pp_module, local_get_interface));

  if (!dispatcher_->InitHostWithChannel(PepperPluginRegistry::GetInstance(),
                                        channel_handle, true)) {
    dispatcher_.reset();
    return false;
  }
  dispatcher_->channel()->SetRestrictDispatchToSameChannel(true);
  return true;
}

BrokerDispatcherWrapper::BrokerDispatcherWrapper() {
}

BrokerDispatcherWrapper::~BrokerDispatcherWrapper() {
}

bool BrokerDispatcherWrapper::Init(
    base::ProcessHandle plugin_process_handle,
    const IPC::ChannelHandle& channel_handle) {
  dispatcher_.reset(
      new pp::proxy::BrokerHostDispatcher(plugin_process_handle));

  if (!dispatcher_->InitBrokerWithChannel(PepperPluginRegistry::GetInstance(),
                                          channel_handle,
                                          true)) {
    dispatcher_.reset();
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

  if (!dispatcher_->Send(
      new PpapiMsg_ConnectToPlugin(instance, foreign_socket_handle))) {
    // The plugin did not receive the handle, so it must be closed.
    // The easiest way to clean it up is to just put it in an object
    // and then close it. This failure case is not performance critical.
    // The handle could still leak if Send succeeded but the IPC later failed.
    base::SyncSocket temp_socket(
        IPC::PlatformFileForTransitToPlatformFile(foreign_socket_handle));
    return PP_ERROR_FAILED;
  }

  return PP_OK;
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
          PlatformFileToInt(base::kInvalidPlatformFileValue), PP_ERROR_ABORTED);
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

    // Process all pending channel requests from the renderers.
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
            PlatformFileToInt(base::kInvalidPlatformFileValue),
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

    result = dispatcher_->SendHandleToBroker(client->instance()->pp_instance(),
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
  client->BrokerConnected(PlatformFileToInt(plugin_handle), result);
}

PepperPluginDelegateImpl::PepperPluginDelegateImpl(RenderView* render_view)
    : render_view_(render_view),
      has_saved_context_menu_action_(false),
      saved_context_menu_action_(0),
      id_generator_(0) {
}

PepperPluginDelegateImpl::~PepperPluginDelegateImpl() {
}

scoped_refptr<webkit::ppapi::PluginModule>
PepperPluginDelegateImpl::CreatePepperPlugin(
    const FilePath& path,
    bool* pepper_plugin_was_registered) {
  *pepper_plugin_was_registered = true;

  // See if a module has already been loaded for this plugin.
  scoped_refptr<webkit::ppapi::PluginModule> module =
      PepperPluginRegistry::GetInstance()->GetLiveModule(path);
  if (module)
    return module;

  // In-process plugins will have always been created up-front to avoid the
  // sandbox restrictions. So getting here implies it doesn't exist or should
  // be out of process.
  const PepperPluginInfo* info =
      PepperPluginRegistry::GetInstance()->GetInfoForPlugin(path);
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
  scoped_ptr<DispatcherWrapper> dispatcher(new DispatcherWrapper);
  if (!dispatcher->Init(
          plugin_process_handle, channel_handle,
          module->pp_module(),
          webkit::ppapi::PluginModule::GetLocalGetInterfaceFunc()))
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

  scoped_refptr<PpapiBrokerImpl> broker =
      *pending_connect_broker_.Lookup(request_id);
  if (broker) {
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

void PepperPluginDelegateImpl::PluginCrashed(
    webkit::ppapi::PluginInstance* instance) {
  render_view_->PluginCrashed(instance->module()->path());
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
  if (!RenderThread::current()->Send(msg))
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
  WebGraphicsContext3DCommandBufferImpl* context =
      static_cast<WebGraphicsContext3DCommandBufferImpl*>(
          render_view_->webview()->graphicsContext3D());
  if (!context || context->isContextLost())
    return NULL;

  RendererGLContext* parent_context = context->context();
  if (!parent_context)
    return NULL;

  return new PlatformContext3DImpl(parent_context);
#else
  return NULL;
#endif
}

webkit::ppapi::PluginDelegate::PlatformVideoDecoder*
PepperPluginDelegateImpl::CreateVideoDecoder(
    media::VideoDecodeAccelerator::Client* client) {
  return new PlatformVideoDecoderImpl(client);
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
    uint32_t sample_rate, uint32_t sample_count,
    webkit::ppapi::PluginDelegate::PlatformAudio::Client* client) {
  scoped_refptr<PlatformAudioImpl> audio(
      new PlatformAudioImpl(render_view_->audio_message_filter()));
  if (audio->Initialize(sample_rate, sample_count, client)) {
    // Balanced by Release invoked in PlatformAudioImpl::ShutDownOnIOThread().
    return audio.release();
  } else {
    return NULL;
  }
}

// If a broker has not already been created for this plugin, creates one.
webkit::ppapi::PluginDelegate::PpapiBroker*
PepperPluginDelegateImpl::ConnectToPpapiBroker(
    webkit::ppapi::PPB_Broker_Impl* client) {
  CHECK(client);

  // If a broker needs to be created, this will ensure it does not get deleted
  // before Connect() adds a reference.
  scoped_refptr<PpapiBrokerImpl> broker_impl;

  webkit::ppapi::PluginModule* plugin_module = client->instance()->module();
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

bool PepperPluginDelegateImpl::AsyncOpenFile(const FilePath& path,
                                             int flags,
                                             AsyncOpenFileCallback* callback) {
  int message_id = id_generator_++;
  DCHECK(!messages_waiting_replies_.Lookup(message_id));
  messages_waiting_replies_.AddWithID(callback, message_id);
  IPC::Message* msg = new ViewHostMsg_AsyncOpenFile(
      render_view_->routing_id(), path, flags, message_id);
  return render_view_->Send(msg);
}

void PepperPluginDelegateImpl::OnAsyncFileOpened(
    base::PlatformFileError error_code,
    base::PlatformFile file,
    int message_id) {
  AsyncOpenFileCallback* callback =
      messages_waiting_replies_.Lookup(message_id);
  DCHECK(callback);
  messages_waiting_replies_.Remove(message_id);
  callback->Run(error_code, file);
  delete callback;
}

void PepperPluginDelegateImpl::OnSetFocus(bool has_focus) {
  for (std::set<webkit::ppapi::PluginInstance*>::iterator i =
         active_instances_.begin();
       i != active_instances_.end(); ++i)
    (*i)->SetContentAreaFocus(has_focus);
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

class AsyncOpenFileSystemURLCallbackTranslator :
    public fileapi::FileSystemCallbackDispatcher {
public:
  AsyncOpenFileSystemURLCallbackTranslator(
      webkit::ppapi::PluginDelegate::AsyncOpenFileCallback* callback)
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
    callback_->Run(error_code, base::kInvalidPlatformFileValue);
  }

  virtual void DidWrite(int64 bytes, bool complete) {
    NOTREACHED();
  }

  virtual void DidOpenFile(
      base::PlatformFile file,
      base::ProcessHandle unused) {
    callback_->Run(base::PLATFORM_FILE_OK, file);
  }

private:  // TODO(ericu): Delete this?
  webkit::ppapi::PluginDelegate::AsyncOpenFileCallback* callback_;
};

bool PepperPluginDelegateImpl::AsyncOpenFileSystemURL(
    const GURL& path, int flags, AsyncOpenFileCallback* callback) {

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

scoped_refptr<base::MessageLoopProxy>
PepperPluginDelegateImpl::GetFileThreadMessageLoopProxy() {
  return RenderThread::current()->GetFileThreadMessageLoopProxy();
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
    const struct PP_Flash_NetAddress* addr) {
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
    const PP_Flash_NetAddress& local_addr,
    const PP_Flash_NetAddress& remote_addr) {
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
  if (instance->IsFullscreen()) {
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
  if (instance->IsFullscreen()) {
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
  scoped_refptr<webkit::ppapi::PPB_Flash_Menu_Impl> menu =
      *pending_context_menus_.Lookup(request_id);
  if (!menu) {
    NOTREACHED() << "CompleteShowContextMenu() called twice for the same menu.";
    return;
  }
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
  int net_error;
  std::string proxy_result;
  RenderThread::current()->Send(
      new ChildProcessHostMsg_ResolveProxy(url, &net_error, &proxy_result));
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

void PepperPluginDelegateImpl::HasUnsupportedFeature() {
  render_view_->Send(new ViewHostMsg_PDFHasUnsupportedFeature(
      render_view_->routing_id()));
}

void PepperPluginDelegateImpl::SaveURLAs(const GURL& url) {
  render_view_->Send(new ViewHostMsg_SaveURLAs(
      render_view_->routing_id(), url));
}

P2PSocketDispatcher* PepperPluginDelegateImpl::GetP2PSocketDispatcher() {
  return render_view_->p2p_socket_dispatcher();
}

webkit_glue::P2PTransport* PepperPluginDelegateImpl::CreateP2PTransport() {
#if defined(ENABLE_P2P_APIS)
  return new P2PTransportImpl(render_view_->p2p_socket_dispatcher());
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
  scoped_ptr<base::SharedMemory> shm(new base::SharedMemory());
  if (shm->CreateAnonymous(size))
    return shm.release();
  // The sandbox can prevent CreateAnonymous from succeeding, in which case we
  // need to IPC to the browser process.
  base::SharedMemoryHandle handle;
  if (!render_view_->Send(
          new ViewHostMsg_AllocateSharedMemoryBuffer(size, &handle))) {
    DLOG(WARNING) << "Browser allocation request message failed";
    return NULL;
  }
  if (!base::SharedMemory::IsHandleValid(handle)) {
    DLOG(WARNING) << "Browser failed to allocate shared memory";
    return NULL;
  }
  shm.reset(new base::SharedMemory(handle, false));
  return shm.release();
}
