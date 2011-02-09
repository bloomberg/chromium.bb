// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/pepper_devices.h"

#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "chrome/renderer/render_thread.h"
#include "chrome/renderer/webplugin_delegate_pepper.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "webkit/plugins/npapi/plugin_instance.h"
#include "webkit/plugins/npapi/webplugin.h"

namespace {

const uint32 kBytesPerPixel = 4;  // Only 8888 RGBA for now.

}  // namespace

int Graphics2DDeviceContext::next_buffer_id_ = 0;

struct Graphics2DDeviceContext::FlushCallbackData {
  FlushCallbackData(NPDeviceFlushContextCallbackPtr f,
                    NPP n,
                    NPDeviceContext2D* c,
                    NPUserData* u)
      : function(f),
        npp(n),
        context(c),
        user_data(u) {
  }

  NPDeviceFlushContextCallbackPtr function;
  NPP npp;
  NPDeviceContext2D* context;
  NPUserData* user_data;
};

Graphics2DDeviceContext::Graphics2DDeviceContext(
    WebPluginDelegatePepper* plugin_delegate)
    : plugin_delegate_(plugin_delegate) {
}

Graphics2DDeviceContext::~Graphics2DDeviceContext() {}

NPError Graphics2DDeviceContext::Initialize(
    gfx::Rect window_rect, const NPDeviceContext2DConfig* config,
    NPDeviceContext2D* context) {
  int width = window_rect.width();
  int height = window_rect.height();
  uint32 buffer_size = width * height * kBytesPerPixel;

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
    return NPERR_GENERIC_ERROR;
  if (!TransportDIB::is_valid(dib_handle))
    return NPERR_OUT_OF_MEMORY_ERROR;
  transport_dib_.reset(TransportDIB::Map(dib_handle));
#else
  transport_dib_.reset(TransportDIB::Create(buffer_size, ++next_buffer_id_));
  if (!transport_dib_.get())
    return NPERR_OUT_OF_MEMORY_ERROR;
#endif  // defined(OS_MACOSX)
  canvas_.reset(transport_dib_->GetPlatformCanvas(width, height));
  if (!canvas_.get())
    return NPERR_OUT_OF_MEMORY_ERROR;

  // Note that we need to get the address out of the bitmap rather than
  // using plugin_buffer_->memory(). The memory() is when the bitmap data
  // has had "Map" called on it. For Windows, this is separate than making a
  // bitmap using the shared section.
  const SkBitmap& plugin_bitmap =
      canvas_->getTopPlatformDevice().accessBitmap(true);
  SkAutoLockPixels locker(plugin_bitmap);

  // TODO(brettw) this theoretically shouldn't be necessary. But the
  // platform device on Windows will fill itself with green to help you
  // catch areas you didn't paint.
  plugin_bitmap.eraseARGB(0, 0, 0, 0);

  // Save the canvas to the output context structure and save the
  // OpenPaintContext for future reference.
  context->region = plugin_bitmap.getAddr32(0, 0);
  context->stride = width * kBytesPerPixel;
  context->dirty.left = 0;
  context->dirty.top = 0;
  context->dirty.right = width;
  context->dirty.bottom = height;
  return NPERR_NO_ERROR;
}

NPError Graphics2DDeviceContext::Flush(SkBitmap* committed_bitmap,
                                       NPDeviceContext2D* context,
                                       NPDeviceFlushContextCallbackPtr callback,
                                       NPP id, void* user_data) {
  // Draw the bitmap to the backing store.
  //
  // TODO(brettw) we can optimize this in the case where the entire canvas is
  // updated by actually taking ownership of the buffer and not telling the
  // plugin we're done using it. This wat we can avoid the copy when the entire
  // canvas has been updated.
  SkIRect src_rect = { context->dirty.left,
                       context->dirty.top,
                       context->dirty.right,
                       context->dirty.bottom };
  SkRect dest_rect = { SkIntToScalar(context->dirty.left),
                       SkIntToScalar(context->dirty.top),
                       SkIntToScalar(context->dirty.right),
                       SkIntToScalar(context->dirty.bottom) };
  SkCanvas committed_canvas(*committed_bitmap);

  // We want to replace the contents of the bitmap rather than blend.
  SkPaint paint;
  paint.setXfermodeMode(SkXfermode::kSrc_Mode);
  committed_canvas.drawBitmapRect(
      canvas_->getTopPlatformDevice().accessBitmap(false),
      &src_rect, dest_rect, &paint);

  committed_bitmap->setIsOpaque(false);

  // Cause the updated part of the screen to be repainted. This will happen
  // asynchronously.
  // TODO(brettw) is this the coorect coordinate system?
  gfx::Rect dest_gfx_rect(context->dirty.left, context->dirty.top,
                          context->dirty.right - context->dirty.left,
                          context->dirty.bottom - context->dirty.top);

  plugin_delegate_->instance()->webplugin()->InvalidateRect(dest_gfx_rect);

  // Save the callback to execute later. See |unpainted_flush_callbacks_| in
  // the header file.
  if (callback) {
    unpainted_flush_callbacks_.push_back(
        FlushCallbackData(callback, id, context, user_data));
  }

  return NPERR_NO_ERROR;
}

void Graphics2DDeviceContext::RenderViewInitiatedPaint() {
  // Move all "unpainted" callbacks to the painted state. See
  // |unpainted_flush_callbacks_| in the header for more.
  std::copy(unpainted_flush_callbacks_.begin(),
            unpainted_flush_callbacks_.end(),
            std::back_inserter(painted_flush_callbacks_));
  unpainted_flush_callbacks_.clear();
}

void Graphics2DDeviceContext::RenderViewFlushedPaint() {
  // Notify all "painted" callbacks. See |unpainted_flush_callbacks_| in the
  // header for more.
  for (size_t i = 0; i < painted_flush_callbacks_.size(); i++) {
    const FlushCallbackData& data = painted_flush_callbacks_[i];
    data.function(data.npp, data.context, NPERR_NO_ERROR, data.user_data);
  }
  painted_flush_callbacks_.clear();
}

AudioDeviceContext::AudioDeviceContext()
    : context_(NULL),
      stream_id_(0),
      shared_memory_size_(0) {
}

AudioDeviceContext::~AudioDeviceContext() {
  if (stream_id_) {
    OnDestroy();
  }
}

NPError AudioDeviceContext::Initialize(AudioMessageFilter* filter,
                                       const NPDeviceContextAudioConfig* config,
                                       NPDeviceContextAudio* context) {
  DCHECK(filter);
  // Make sure we don't call init more than once.
  DCHECK_EQ(0, stream_id_);

  if (!config || !context) {
    return NPERR_INVALID_PARAM;
  }
  filter_ = filter;
  context_= context;

  ViewHostMsg_Audio_CreateStream_Params params;
  params.params.format = AudioParameters::AUDIO_PCM_LINEAR;
  params.params.channels = config->outputChannelMap;
  params.params.sample_rate = config->sampleRate;
  switch (config->sampleType) {
    case NPAudioSampleTypeInt16:
      params.params.bits_per_sample = 16;
      break;
    case NPAudioSampleTypeFloat32:
      params.params.bits_per_sample = 32;
      break;
    default:
      return NPERR_INVALID_PARAM;
  }

  context->config = *config;
  params.params.samples_per_packet = config->sampleFrameCount;

  stream_id_ = filter_->AddDelegate(this);
  filter->Send(new ViewHostMsg_CreateAudioStream(0, stream_id_, params, true));
  return NPERR_NO_ERROR;
}

void AudioDeviceContext::OnDestroy() {
  // Make sure we don't call destroy more than once.
  DCHECK_NE(0, stream_id_);
  filter_->RemoveDelegate(stream_id_);
  filter_->Send(new ViewHostMsg_CloseAudioStream(0, stream_id_));
  stream_id_ = 0;
  if (audio_thread_.get()) {
    socket_->Close();
    audio_thread_->Join();
  }
}

void AudioDeviceContext::OnRequestPacket(AudioBuffersState buffers_state) {
  FireAudioCallback();
  filter_->Send(new ViewHostMsg_NotifyAudioPacketReady(0, stream_id_,
                                                       shared_memory_size_));
}

void AudioDeviceContext::OnStateChanged(
    const ViewMsg_AudioStreamState_Params& state) {
}

void AudioDeviceContext::OnCreated(
    base::SharedMemoryHandle handle, uint32 length) {
#if defined(OS_WIN)
  DCHECK(handle);
#else
  DCHECK_NE(-1, handle.fd);
#endif
  DCHECK(length);
  DCHECK(context_);

  shared_memory_.reset(new base::SharedMemory(handle, false));
  shared_memory_->Map(length);
  shared_memory_size_ = length;

  context_->outBuffer = shared_memory_->memory();
  FireAudioCallback();
  filter_->Send(new ViewHostMsg_PlayAudioStream(0, stream_id_));
}

void AudioDeviceContext::OnLowLatencyCreated(
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
  DCHECK(context_);
  DCHECK(!audio_thread_.get());
  shared_memory_.reset(new base::SharedMemory(handle, false));
  shared_memory_->Map(length);
  shared_memory_size_ = length;

  context_->outBuffer = shared_memory_->memory();
  socket_.reset(new base::SyncSocket(socket_handle));
  // Allow the client to pre-populate the buffer.
  FireAudioCallback();
  if (context_->config.startThread) {
    audio_thread_.reset(
        new base::DelegateSimpleThread(this, "plugin_audio_thread"));
    audio_thread_->Start();
  }
  filter_->Send(new ViewHostMsg_PlayAudioStream(0, stream_id_));
}

void AudioDeviceContext::OnVolume(double volume) {
}

void AudioDeviceContext::Run() {
  int pending_data;
  while (sizeof(pending_data) == socket_->Receive(&pending_data,
                                                  sizeof(pending_data)) &&
      pending_data >= 0) {
    FireAudioCallback();
  }
}
