// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/pepper_plugin_delegate_impl.h"

#include "app/l10n_util.h"
#include "app/surface/transport_dib.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "base/time.h"
#include "chrome/common/child_thread.h"
#include "chrome/common/file_system/file_system_dispatcher.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "chrome/renderer/audio_message_filter.h"
#include "chrome/renderer/command_buffer_proxy.h"
#include "chrome/renderer/render_thread.h"
#include "chrome/renderer/render_view.h"
#include "chrome/renderer/webplugin_delegate_proxy.h"
#include "grit/locale_settings.h"
#include "third_party/ppapi/c/dev/pp_video_dev.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFileChooserCompletion.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFileChooserParams.h"
#include "third_party/WebKit/WebKit/chromium/public/WebPluginContainer.h"
#include "webkit/fileapi/file_system_callback_dispatcher.h"
#include "webkit/glue/plugins/pepper_file_io.h"
#include "webkit/glue/plugins/pepper_plugin_instance.h"
#include "webkit/glue/plugins/webplugin.h"

#if defined(OS_MACOSX)
#include "chrome/common/render_messages.h"
#include "chrome/renderer/render_thread.h"
#endif

namespace {

const int32 kDefaultCommandBufferSize = 1024 * 1024;

// Implements the Image2D using a TransportDIB.
class PlatformImage2DImpl : public pepper::PluginDelegate::PlatformImage2D {
 public:
  // This constructor will take ownership of the dib pointer.
  PlatformImage2DImpl(int width, int height, TransportDIB* dib)
      : width_(width),
        height_(height),
        dib_(dib) {
  }

  virtual skia::PlatformCanvas* Map() {
    return dib_->GetPlatformCanvas(width_, height_);
  }

  virtual intptr_t GetSharedMemoryHandle() const {
    return reinterpret_cast<intptr_t>(dib_.get());
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

#ifdef ENABLE_GPU

class PlatformContext3DImpl : public pepper::PluginDelegate::PlatformContext3D {
 public:
  explicit PlatformContext3DImpl(RenderView* render_view)
      : render_view_(render_view),
        nested_delegate_(NULL),
        command_buffer_(NULL),
        renderview_to_webplugin_adapter_(render_view) {}

  virtual ~PlatformContext3DImpl() {
    if (nested_delegate_) {
      nested_delegate_->DestroyCommandBuffer(command_buffer_);
      nested_delegate_->PluginDestroyed();
    }
  }

  virtual bool Init(const gfx::Rect& position, const gfx::Rect& clip);

  virtual gpu::CommandBuffer* GetCommandBuffer() {
    return command_buffer_;
  }

  virtual void SetNotifyRepaintTask(Task* task) {
    command_buffer_->SetNotifyRepaintTask(task);
  }

 private:

  class WebPluginAdapter : public webkit_glue::WebPlugin {
   public:
    explicit WebPluginAdapter(RenderView* render_view)
        : render_view_(render_view) {}

    virtual void SetWindow(gfx::PluginWindowHandle window) {
      render_view_->CreatedPluginWindow(window);
    }

    virtual void WillDestroyWindow(gfx::PluginWindowHandle window) {
      render_view_->WillDestroyPluginWindow(window);
    }

    virtual void SetAcceptsInputEvents(bool accepts) {
      NOTREACHED();
    }

#if defined(OS_WIN)
    virtual void SetWindowlessPumpEvent(HANDLE pump_messages_event) {
      NOTREACHED();
    }
#endif

    virtual void CancelResource(unsigned long id) {
      NOTREACHED();
    }

    virtual void Invalidate() {
      NOTREACHED();
    }

    virtual void InvalidateRect(const gfx::Rect& rect) {
      NOTREACHED();
    }

    virtual NPObject* GetWindowScriptNPObject() {
      NOTREACHED();
      return NULL;
    }

    virtual NPObject* GetPluginElement() {
      NOTREACHED();
      return NULL;
    }

    virtual void SetCookie(const GURL& url,
                           const GURL& first_party_for_cookies,
                           const std::string& cookie) {
      NOTREACHED();
    }

    virtual std::string GetCookies(const GURL& url,
                                   const GURL& first_party_for_cookies) {
      NOTREACHED();
      return std::string();
    }

    virtual void ShowModalHTMLDialog(const GURL& url, int width, int height,
                                     const std::string& json_arguments,
                                     std::string* json_retval) {
      NOTREACHED();
    }

    virtual void OnMissingPluginStatus(int status) {
      NOTREACHED();
    }

    virtual void HandleURLRequest(const char* url,
                                  const char* method,
                                  const char* target,
                                  const char* buf,
                                  unsigned int len,
                                  int notify_id,
                                  bool popups_allowed) {
      NOTREACHED();
    }

    virtual void CancelDocumentLoad() {
      NOTREACHED();
    }

    virtual void InitiateHTTPRangeRequest(const char* url,
                                          const char* range_info,
                                          int range_request_id) {
      NOTREACHED();
    }

    virtual bool IsOffTheRecord() {
      NOTREACHED();
      return false;
    }

    virtual void SetDeferResourceLoading(unsigned long resource_id,
                                         bool defer) {
      NOTREACHED();
    }

   private:
    RenderView* render_view_;
  };

  void SendNestedDelegateGeometryToBrowser(const gfx::Rect& window_rect,
                                           const gfx::Rect& clip_rect);

  RenderView* render_view_;
  WebPluginDelegateProxy* nested_delegate_;
  CommandBufferProxy* command_buffer_;
  WebPluginAdapter renderview_to_webplugin_adapter_;
};

#endif  // ENABLE_GPU

class PlatformAudioImpl
    : public pepper::PluginDelegate::PlatformAudio,
      public AudioMessageFilter::Delegate {
 public:
  explicit PlatformAudioImpl(scoped_refptr<AudioMessageFilter> filter)
      : client_(NULL), filter_(filter), stream_id_(0) {
    DCHECK(filter_);
  }

  virtual ~PlatformAudioImpl() {
    // Make sure we have been shut down.
    DCHECK_EQ(0, stream_id_);
    DCHECK(!client_);
  }

  // Initialize this audio context. StreamCreated() will be called when the
  // stream is created.
  bool Initialize(uint32_t sample_rate, uint32_t sample_count,
       pepper::PluginDelegate::PlatformAudio::Client* client);

  virtual bool StartPlayback() {
    return filter_ && filter_->Send(
        new ViewHostMsg_PlayAudioStream(0, stream_id_));
  }

  virtual bool StopPlayback() {
    return filter_ && filter_->Send(
        new ViewHostMsg_PauseAudioStream(0, stream_id_));
  }

  virtual void ShutDown();

 private:
  virtual void OnRequestPacket(AudioBuffersState buffers_state) {
    LOG(FATAL) << "Should never get OnRequestPacket in PlatformAudioImpl";
  }

  virtual void OnStateChanged(const ViewMsg_AudioStreamState_Params& state) { }

  virtual void OnCreated(base::SharedMemoryHandle handle, uint32 length) {
    LOG(FATAL) << "Should never get OnCreated in PlatformAudioImpl";
  }

  virtual void OnLowLatencyCreated(base::SharedMemoryHandle handle,
                                   base::SyncSocket::Handle socket_handle,
                                   uint32 length);

  virtual void OnVolume(double volume) { }

  // The client to notify when the stream is created.
  pepper::PluginDelegate::PlatformAudio::Client* client_;
  // MessageFilter used to send/receive IPC.
  scoped_refptr<AudioMessageFilter> filter_;
  // Our ID on the MessageFilter.
  int32 stream_id_;

  DISALLOW_COPY_AND_ASSIGN(PlatformAudioImpl);
};

#ifdef ENABLE_GPU

bool PlatformContext3DImpl::Init(const gfx::Rect& position,
                                 const gfx::Rect& clip) {
#if defined(ENABLE_GPU)
  // Ignore initializing more than once.
  if (nested_delegate_)
    return true;

  // Create an instance of the GPU plugin that is responsible for 3D
  // rendering.
  nested_delegate_ = new WebPluginDelegateProxy(
      std::string("application/vnd.google.chrome.gpu-plugin"),
      render_view_->AsWeakPtr());

  if (nested_delegate_->Initialize(GURL(), std::vector<std::string>(),
                                   std::vector<std::string>(),
                                   &renderview_to_webplugin_adapter_,
                                   false)) {
    // Ensure the window has the correct size before initializing the
    // command buffer.
    nested_delegate_->UpdateGeometry(position, clip);

    // Ask the GPU plugin to create a command buffer and return a proxy.
    command_buffer_ = nested_delegate_->CreateCommandBuffer();
    if (command_buffer_) {
      // Initialize the proxy command buffer.
      if (command_buffer_->Initialize(kDefaultCommandBufferSize)) {
#if defined(OS_MACOSX)
        command_buffer_->SetWindowSize(position.size());
#endif  // OS_MACOSX

        // Make sure the nested delegate shows up in the right place
        // on the page.
        SendNestedDelegateGeometryToBrowser(position, clip);

        return true;
      }
    }

    nested_delegate_->DestroyCommandBuffer(command_buffer_);
    command_buffer_ = NULL;
  }

  nested_delegate_->PluginDestroyed();
  nested_delegate_ = NULL;
#endif  // ENABLE_GPU
  return false;
}

void PlatformContext3DImpl::SendNestedDelegateGeometryToBrowser(
    const gfx::Rect& window_rect,
    const gfx::Rect& clip_rect) {
  // Inform the browser about the location of the plugin on the page.
  // It appears that initially the plugin does not get laid out correctly --
  // possibly due to lazy creation of the nested delegate.
  if (!nested_delegate_ ||
      !nested_delegate_->GetPluginWindowHandle() ||
      !render_view_) {
    return;
  }

  webkit_glue::WebPluginGeometry geom;
  geom.window = nested_delegate_->GetPluginWindowHandle();
  geom.window_rect = window_rect;
  geom.clip_rect = clip_rect;
  // Rects_valid must be true for this to work in the Gtk port;
  // hopefully not having the cutout rects will not cause incorrect
  // clipping.
  geom.rects_valid = true;
  geom.visible = true;
  render_view_->DidMovePlugin(geom);
}

#endif  // ENABLE_GPU

bool PlatformAudioImpl::Initialize(
    uint32_t sample_rate, uint32_t sample_count,
    pepper::PluginDelegate::PlatformAudio::Client* client) {

  DCHECK(client);
  // Make sure we don't call init more than once.
  DCHECK_EQ(0, stream_id_);

  client_ = client;

  ViewHostMsg_Audio_CreateStream_Params params;
  params.params.format = AudioParameters::AUDIO_PCM_LINEAR;
  params.params.channels = 2;
  params.params.sample_rate = sample_rate;
  params.params.bits_per_sample = 16;

  params.packet_size = sample_count * params.params.channels *
      (params.params.bits_per_sample >> 3);

  stream_id_ = filter_->AddDelegate(this);
  return filter_->Send(new ViewHostMsg_CreateAudioStream(0, stream_id_, params,
                                                         true));
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

  client_->StreamCreated(handle, length, socket_handle);
}

void PlatformAudioImpl::ShutDown() {
  // Make sure we don't call shutdown more than once.
  if (!stream_id_) {
    return;
  }
  filter_->Send(new ViewHostMsg_CloseAudioStream(0, stream_id_));
  filter_->RemoveDelegate(stream_id_);
  stream_id_ = 0;
  client_ = NULL;
}

// Implements the VideoDecoder.
class PlatformVideoDecoderImpl
    : public pepper::PluginDelegate::PlatformVideoDecoder {
 public:
  PlatformVideoDecoderImpl()
      : input_buffer_size_(0),
        next_dib_id_(0),
        dib_(NULL) {
    memset(&decoder_config_, 0, sizeof(decoder_config_));
    memset(&flush_callback_, 0, sizeof(flush_callback_));
  }

  virtual bool Init(const PP_VideoDecoderConfig_Dev& decoder_config) {
    decoder_config_ = decoder_config;
    input_buffer_size_ = 1024 << 4;

    // Allocate the transport DIB.
    TransportDIB* dib = TransportDIB::Create(input_buffer_size_,
                                             next_dib_id_++);
    if (!dib)
      return false;

    // TODO(wjia): Create video decoder in GPU process.

    return true;
  }

  virtual bool Decode(PP_VideoCompressedDataBuffer_Dev& input_buffer) {
    // TODO(wjia): Implement me!
    NOTIMPLEMENTED();

    input_buffers_.push(&input_buffer);

    // Copy input data to dib_ and send it to GPU video decoder.

    return false;
  }

  virtual int32_t Flush(PP_CompletionCallback& callback) {
    // TODO(wjia): Implement me!
    NOTIMPLEMENTED();

    // Do nothing if there is a flush pending.
    if (flush_callback_.func)
      return PP_ERROR_BADARGUMENT;

    flush_callback_ = callback;

    // Call GPU video decoder to flush.

    return PP_ERROR_WOULDBLOCK;
  }

  virtual bool ReturnUncompressedDataBuffer(
      PP_VideoUncompressedDataBuffer_Dev& buffer) {
    // TODO(wjia): Implement me!
    NOTIMPLEMENTED();

    // Deliver the buffer to GPU video decoder.

    return false;
  }

  void OnFlushDone() {
    if (!flush_callback_.func)
      return;

    flush_callback_.func(flush_callback_.user_data, PP_OK);
    flush_callback_.func = NULL;
  }

  virtual intptr_t GetSharedMemoryHandle() const {
    return reinterpret_cast<intptr_t>(dib_.get());
  }

 private:
  size_t input_buffer_size_;
  int next_dib_id_;
  scoped_ptr<TransportDIB> dib_;
  PP_VideoDecoderConfig_Dev decoder_config_;
  std::queue<PP_VideoCompressedDataBuffer_Dev*> input_buffers_;
  PP_CompletionCallback flush_callback_;

  DISALLOW_COPY_AND_ASSIGN(PlatformVideoDecoderImpl);
};

}  // namespace

PepperPluginDelegateImpl::PepperPluginDelegateImpl(RenderView* render_view)
    : render_view_(render_view),
      id_generator_(0) {
}

void PepperPluginDelegateImpl::ViewInitiatedPaint() {
  // Notify all of our instances that we started painting. This is used for
  // internal bookkeeping only, so we know that the set can not change under
  // us.
  for (std::set<pepper::PluginInstance*>::iterator i =
           active_instances_.begin();
       i != active_instances_.end(); ++i)
    (*i)->ViewInitiatedPaint();
}

void PepperPluginDelegateImpl::ViewFlushedPaint() {
  // Notify all instances that we painted. This will call into the plugin, and
  // we it may ask to close itself as a result. This will, in turn, modify our
  // set, possibly invalidating the iterator. So we iterate on a copy that
  // won't change out from under us.
  std::set<pepper::PluginInstance*> plugins = active_instances_;
  for (std::set<pepper::PluginInstance*>::iterator i = plugins.begin();
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

bool PepperPluginDelegateImpl::GetBitmapForOptimizedPluginPaint(
    const gfx::Rect& paint_bounds,
    TransportDIB** dib,
    gfx::Rect* location,
    gfx::Rect* clip) {
  for (std::set<pepper::PluginInstance*>::iterator i =
           active_instances_.begin();
       i != active_instances_.end(); ++i) {
    pepper::PluginInstance* instance = *i;
    if (instance->GetBitmapForOptimizedPluginPaint(
            paint_bounds, dib, location, clip))
      return true;
  }
  return false;
}

void PepperPluginDelegateImpl::InstanceCreated(
    pepper::PluginInstance* instance) {
  active_instances_.insert(instance);

  // Set the initial focus.
  instance->SetContentAreaFocus(render_view_->has_focus());
}

void PepperPluginDelegateImpl::InstanceDeleted(
    pepper::PluginInstance* instance) {
  active_instances_.erase(instance);
}

pepper::PluginDelegate::PlatformImage2D*
PepperPluginDelegateImpl::CreateImage2D(int width, int height) {
  uint32 buffer_size = width * height * 4;

  // Allocate the transport DIB and the PlatformCanvas pointing to it.
#if defined(OS_MACOSX)
  // On the Mac, shared memory has to be created in the browser in order to
  // work in the sandbox.  Do this by sending a message to the browser
  // requesting a TransportDIB (see also
  // chrome/renderer/webplugin_delegate_proxy.cc, method
  // WebPluginDelegateProxy::CreateBitmap() for similar code).  Note that the
  // TransportDIB is _not_ cached in the browser; this is because this memory
  // gets flushed by the renderer into another TransportDIB that represents the
  // page, which is then in turn flushed to the screen by the browser process.
  // When |transport_dib_| goes out of scope in the dtor, all of its shared
  // memory gets reclaimed.
  TransportDIB::Handle dib_handle;
  IPC::Message* msg = new ViewHostMsg_AllocTransportDIB(buffer_size,
                                                        false,
                                                        &dib_handle);
  if (!RenderThread::current()->Send(msg))
    return NULL;
  if (!TransportDIB::is_valid(dib_handle))
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

pepper::PluginDelegate::PlatformContext3D*
    PepperPluginDelegateImpl::CreateContext3D() {
#ifdef ENABLE_GPU
  return new PlatformContext3DImpl(render_view_);
#else
  return NULL;
#endif
}

pepper::PluginDelegate::PlatformVideoDecoder*
PepperPluginDelegateImpl::CreateVideoDecoder(
    const PP_VideoDecoderConfig_Dev& decoder_config) {
  scoped_ptr<PlatformVideoDecoderImpl> decoder(new PlatformVideoDecoderImpl());

  if (!decoder->Init(decoder_config))
    return NULL;

  return decoder.release();
}

void PepperPluginDelegateImpl::DidChangeNumberOfFindResults(int identifier,
                                                           int total,
                                                           bool final_result) {
  render_view_->reportFindInPageMatchCount(identifier, total, final_result);
}

void PepperPluginDelegateImpl::DidChangeSelectedFindResult(int identifier,
                                                          int index) {
  render_view_->reportFindInPageSelection(
      identifier, index + 1, WebKit::WebRect());
}

pepper::PluginDelegate::PlatformAudio* PepperPluginDelegateImpl::CreateAudio(
    uint32_t sample_rate, uint32_t sample_count,
    pepper::PluginDelegate::PlatformAudio::Client* client) {
  scoped_ptr<PlatformAudioImpl> audio(
      new PlatformAudioImpl(render_view_->audio_message_filter()));
  if (audio->Initialize(sample_rate, sample_count, client)) {
    return audio.release();
  } else {
    return NULL;
  }
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
  for (std::set<pepper::PluginInstance*>::iterator i =
         active_instances_.begin();
       i != active_instances_.end(); ++i)
    (*i)->SetContentAreaFocus(has_focus);
}

bool PepperPluginDelegateImpl::MakeDirectory(
    const FilePath& path,
    bool recursive,
    fileapi::FileSystemCallbackDispatcher* dispatcher) {
  FileSystemDispatcher* file_system_dispatcher =
      ChildThread::current()->file_system_dispatcher();
  return file_system_dispatcher->Create(
      path, false, true, recursive, dispatcher);
}

bool PepperPluginDelegateImpl::Query(
    const FilePath& path,
    fileapi::FileSystemCallbackDispatcher* dispatcher) {
  FileSystemDispatcher* file_system_dispatcher =
      ChildThread::current()->file_system_dispatcher();
  return file_system_dispatcher->ReadMetadata(path, dispatcher);
}

bool PepperPluginDelegateImpl::Touch(
    const FilePath& path,
    const base::Time& last_access_time,
    const base::Time& last_modified_time,
    fileapi::FileSystemCallbackDispatcher* dispatcher) {
  FileSystemDispatcher* file_system_dispatcher =
      ChildThread::current()->file_system_dispatcher();
  return file_system_dispatcher->TouchFile(path, last_access_time,
                                           last_modified_time, dispatcher);
}

bool PepperPluginDelegateImpl::Delete(
    const FilePath& path,
    fileapi::FileSystemCallbackDispatcher* dispatcher) {
  FileSystemDispatcher* file_system_dispatcher =
      ChildThread::current()->file_system_dispatcher();
  return file_system_dispatcher->Remove(path, dispatcher);
}

bool PepperPluginDelegateImpl::Rename(
    const FilePath& file_path,
    const FilePath& new_file_path,
    fileapi::FileSystemCallbackDispatcher* dispatcher) {
  FileSystemDispatcher* file_system_dispatcher =
      ChildThread::current()->file_system_dispatcher();
  return file_system_dispatcher->Move(file_path, new_file_path, dispatcher);
}

scoped_refptr<base::MessageLoopProxy>
PepperPluginDelegateImpl::GetFileThreadMessageLoopProxy() {
  return RenderThread::current()->GetFileThreadMessageLoopProxy();
}

pepper::FullscreenContainer*
PepperPluginDelegateImpl::CreateFullscreenContainer(
    pepper::PluginInstance* instance) {
  return render_view_->CreatePepperFullscreenContainer(instance);
}

std::string PepperPluginDelegateImpl::GetDefaultEncoding() {
  // TODO(brettw) bug 56615: Somehow get the preference for the default
  // encoding here rather than using the global default for the UI language.
  return l10n_util::GetStringUTF8(IDS_DEFAULT_ENCODING);
}
