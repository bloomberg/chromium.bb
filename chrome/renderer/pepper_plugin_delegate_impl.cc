// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>

#include "chrome/renderer/pepper_plugin_delegate_impl.h"

#include "app/l10n_util.h"
#include "app/surface/transport_dib.h"
#include "base/callback.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string_split.h"
#include "base/task.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/child_thread.h"
#include "chrome/common/file_system/file_system_dispatcher.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "chrome/renderer/audio_message_filter.h"
#include "chrome/renderer/command_buffer_proxy.h"
#include "chrome/renderer/ggl/ggl.h"
#include "chrome/renderer/gpu_channel_host.h"
#include "chrome/renderer/render_thread.h"
#include "chrome/renderer/render_view.h"
#include "chrome/renderer/webgraphicscontext3d_command_buffer_impl.h"
#include "chrome/renderer/webplugin_delegate_proxy.h"
#include "gfx/size.h"
#include "grit/locale_settings.h"
#include "ipc/ipc_channel_handle.h"
#include "ppapi/c/dev/pp_video_dev.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFileChooserCompletion.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFileChooserParams.h"
#include "third_party/WebKit/WebKit/chromium/public/WebPluginContainer.h"
#include "third_party/WebKit/WebKit/chromium/public/WebView.h"
#include "webkit/fileapi/file_system_callback_dispatcher.h"
#include "webkit/glue/plugins/pepper_file_io.h"
#include "webkit/glue/plugins/pepper_plugin_instance.h"
#include "webkit/glue/plugins/pepper_plugin_module.h"
#include "webkit/glue/plugins/webplugin.h"

#if defined(OS_MACOSX)
#include "chrome/common/render_messages.h"
#include "chrome/renderer/render_thread.h"
#endif

using WebKit::WebView;

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

#ifdef ENABLE_GPU

class PlatformContext3DImpl : public pepper::PluginDelegate::PlatformContext3D {
 public:
  explicit PlatformContext3DImpl(WebKit::WebView* web_view)
      : web_view_(web_view),
        context_(NULL) {
  }

  virtual ~PlatformContext3DImpl() {
    if (context_) {
      ggl::DestroyContext(context_);
      context_ = NULL;
    }
  }

  virtual bool Init();
  virtual bool SwapBuffers();
  virtual unsigned GetError();
  virtual void SetSwapBuffersCallback(Callback0::Type* callback);
  void ResizeBackingTexture(const gfx::Size& size);
  virtual unsigned GetBackingTextureId();
  virtual gpu::gles2::GLES2Implementation* GetGLES2Implementation();

 private:
  WebKit::WebView* web_view_;
  ggl::Context* context_;
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

bool PlatformContext3DImpl::Init() {
  // Ignore initializing more than once.
  if (context_)
    return true;

  WebGraphicsContext3DCommandBufferImpl* context =
      static_cast<WebGraphicsContext3DCommandBufferImpl*>(
          web_view_->graphicsContext3D());
  if (!context)
    return false;

  ggl::Context* parent_context = context->context();
  if (!parent_context)
    return false;

  RenderThread* render_thread = RenderThread::current();
  if (!render_thread)
    return false;

  GpuChannelHost* host = render_thread->GetGpuChannel();
  if (!host)
    return false;

  DCHECK(host->state() == GpuChannelHost::kConnected);

  // TODO(apatrick): Let Pepper plugins configure their back buffer surface.
  static const int32 attribs[] = {
    ggl::GGL_ALPHA_SIZE, 8,
    ggl::GGL_DEPTH_SIZE, 24,
    ggl::GGL_STENCIL_SIZE, 8,
    ggl::GGL_SAMPLES, 0,
    ggl::GGL_SAMPLE_BUFFERS, 0,
    ggl::GGL_NONE,
  };

  // TODO(apatrick): Decide which extensions to expose to Pepper plugins.
  // Currently they get only core GLES2.
  context_ = ggl::CreateOffscreenContext(host,
                                         parent_context,
                                         gfx::Size(1, 1),
                                         "",
                                         attribs);
  if (!context_)
    return false;

  return true;
}

bool PlatformContext3DImpl::SwapBuffers() {
  DCHECK(context_);
  return ggl::SwapBuffers(context_);
}

unsigned PlatformContext3DImpl::GetError() {
  DCHECK(context_);
  return ggl::GetError(context_);
}

void PlatformContext3DImpl::ResizeBackingTexture(const gfx::Size& size) {
  DCHECK(context_);
  ggl::ResizeOffscreenContext(context_, size);
}

void PlatformContext3DImpl::SetSwapBuffersCallback(Callback0::Type* callback) {
  DCHECK(context_);
  ggl::SetSwapBuffersCallback(context_, callback);
}

unsigned PlatformContext3DImpl::GetBackingTextureId() {
  DCHECK(context_);
  return ggl::GetParentTextureId(context_);
}

gpu::gles2::GLES2Implementation*
    PlatformContext3DImpl::GetGLES2Implementation() {
  DCHECK(context_);
  return ggl::GetImplementation(context_);
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
  params.params.samples_per_packet = sample_count;

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

PepperPluginDelegateImpl::~PepperPluginDelegateImpl() {
}

scoped_refptr<pepper::PluginModule>
PepperPluginDelegateImpl::CreateOutOfProcessPepperPlugin(
    const FilePath& path) {
  IPC::ChannelHandle channel_handle;
  render_view_->Send(new ViewHostMsg_OpenChannelToPepperPlugin(
      path, &channel_handle));
  if (channel_handle.name.empty())
    return scoped_refptr<pepper::PluginModule>();  // Couldn't be initialized.
  return pepper::PluginModule::CreateOutOfProcessModule(
      ChildProcess::current()->io_message_loop(),
      channel_handle,
      ChildProcess::current()->GetShutDownEvent());
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
  return new PlatformContext3DImpl(render_view_->webview());
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

bool PepperPluginDelegateImpl::OpenFileSystem(
    const GURL& url,
    fileapi::FileSystemType type,
    long long size,
    fileapi::FileSystemCallbackDispatcher* dispatcher) {
  FileSystemDispatcher* file_system_dispatcher =
      ChildThread::current()->file_system_dispatcher();
  return file_system_dispatcher->OpenFileSystem(
      url, type, size, true /* create */, dispatcher);
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
  return file_system_dispatcher->Remove(path, false /* recursive */,
                                        dispatcher);
}

bool PepperPluginDelegateImpl::Rename(
    const FilePath& file_path,
    const FilePath& new_file_path,
    fileapi::FileSystemCallbackDispatcher* dispatcher) {
  FileSystemDispatcher* file_system_dispatcher =
      ChildThread::current()->file_system_dispatcher();
  return file_system_dispatcher->Move(file_path, new_file_path, dispatcher);
}

bool PepperPluginDelegateImpl::ReadDirectory(
    const FilePath& directory_path,
    fileapi::FileSystemCallbackDispatcher* dispatcher) {
  FileSystemDispatcher* file_system_dispatcher =
      ChildThread::current()->file_system_dispatcher();
  return file_system_dispatcher->ReadDirectory(directory_path, dispatcher);
}

FilePath GetModuleLocalFilePath(const std::string& module_name,
                                const FilePath& path) {
#if defined(OS_WIN)
  FilePath full_path(UTF8ToUTF16(module_name));
#else
  FilePath full_path(module_name);
#endif
  full_path = full_path.Append(path);
  return full_path;
}

base::PlatformFileError PepperPluginDelegateImpl::OpenModuleLocalFile(
    const std::string& module_name,
    const FilePath& path,
    int flags,
    base::PlatformFile* file) {
  FilePath full_path = GetModuleLocalFilePath(module_name, path);
  if (full_path.empty()) {
    *file = base::kInvalidPlatformFileValue;
    return base::PLATFORM_FILE_ERROR_ACCESS_DENIED;
  }
  IPC::PlatformFileForTransit transit_file;
  base::PlatformFileError error;
  IPC::Message* msg = new ViewHostMsg_PepperOpenFile(
      full_path, flags, &error, &transit_file);
  if (!render_view_->Send(msg)) {
    *file = base::kInvalidPlatformFileValue;
    return base::PLATFORM_FILE_ERROR_FAILED;
  }
  *file = IPC::PlatformFileForTransitToPlatformFile(transit_file);
  return error;
}

base::PlatformFileError PepperPluginDelegateImpl::RenameModuleLocalFile(
    const std::string& module_name,
    const FilePath& path_from,
    const FilePath& path_to) {
  FilePath full_path_from = GetModuleLocalFilePath(module_name, path_from);
  FilePath full_path_to = GetModuleLocalFilePath(module_name, path_to);
  if (full_path_from.empty() || full_path_to.empty()) {
    return base::PLATFORM_FILE_ERROR_ACCESS_DENIED;
  }
  base::PlatformFileError error;
  IPC::Message* msg = new ViewHostMsg_PepperRenameFile(
      full_path_from, full_path_to, &error);
  if (!render_view_->Send(msg)) {
    return base::PLATFORM_FILE_ERROR_FAILED;
  }
  return error;
}

base::PlatformFileError PepperPluginDelegateImpl::DeleteModuleLocalFileOrDir(
    const std::string& module_name,
    const FilePath& path,
    bool recursive) {
  FilePath full_path = GetModuleLocalFilePath(module_name, path);
  if (full_path.empty()) {
    return base::PLATFORM_FILE_ERROR_ACCESS_DENIED;
  }
  base::PlatformFileError error;
  IPC::Message* msg = new ViewHostMsg_PepperDeleteFileOrDir(
      full_path, recursive, &error);
  if (!render_view_->Send(msg)) {
    return base::PLATFORM_FILE_ERROR_FAILED;
  }
  return error;
}

base::PlatformFileError PepperPluginDelegateImpl::CreateModuleLocalDir(
    const std::string& module_name,
    const FilePath& path) {
  FilePath full_path = GetModuleLocalFilePath(module_name, path);
  if (full_path.empty()) {
    return base::PLATFORM_FILE_ERROR_ACCESS_DENIED;
  }
  base::PlatformFileError error;
  IPC::Message* msg = new ViewHostMsg_PepperCreateDir(full_path, &error);
  if (!render_view_->Send(msg)) {
    return base::PLATFORM_FILE_ERROR_FAILED;
  }
  return error;
}

base::PlatformFileError PepperPluginDelegateImpl::QueryModuleLocalFile(
    const std::string& module_name,
    const FilePath& path,
    base::PlatformFileInfo* info) {
  FilePath full_path = GetModuleLocalFilePath(module_name, path);
  if (full_path.empty()) {
    return base::PLATFORM_FILE_ERROR_ACCESS_DENIED;
  }
  base::PlatformFileError error;
  IPC::Message* msg = new ViewHostMsg_PepperQueryFile(full_path, info, &error);
  if (!render_view_->Send(msg)) {
    return base::PLATFORM_FILE_ERROR_FAILED;
  }
  return error;
}
base::PlatformFileError PepperPluginDelegateImpl::GetModuleLocalDirContents(
      const std::string& module_name,
      const FilePath& path,
      PepperDirContents* contents) {
  FilePath full_path = GetModuleLocalFilePath(module_name, path);
  if (full_path.empty()) {
    return base::PLATFORM_FILE_ERROR_ACCESS_DENIED;
  }
  base::PlatformFileError error;
  IPC::Message* msg =
      new ViewHostMsg_PepperGetDirContents(full_path, contents, &error);
  if (!render_view_->Send(msg)) {
    return base::PLATFORM_FILE_ERROR_FAILED;
  }
  return error;
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

void PepperPluginDelegateImpl::ZoomLimitsChanged(double minimum_factor,
                                                 double maximum_factor) {
  double minimum_level = WebView::zoomFactorToZoomLevel(minimum_factor);
  double maximum_level = WebView::zoomFactorToZoomLevel(maximum_factor);
  render_view_->webview()->zoomLimitsChanged(minimum_level, maximum_level);
}

std::string PepperPluginDelegateImpl::ResolveProxy(const GURL& url) {
  int net_error;
  std::string proxy_result;
  IPC::Message* msg =
      new ViewHostMsg_ResolveProxy(url, &net_error, &proxy_result);
  RenderThread::current()->Send(msg);
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
