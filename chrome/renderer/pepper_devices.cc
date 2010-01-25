// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/pepper_devices.h"
#include "skia/ext/platform_canvas.h"

namespace {
  const uint32 kBytesPerPixel = 4;  // Only 8888 RGBA for now.
}  // namespace

int Graphics2DDeviceContext::next_buffer_id_ = 0;

NPError Graphics2DDeviceContext::Initialize(
        gfx::Rect window_rect, const NPDeviceContext2DConfig* config,
        NPDeviceContext2D* context) {
  int width = window_rect.width();
  int height = window_rect.height();
  uint32 buffer_size = width * height * kBytesPerPixel;

  // Allocate the transport DIB and the PlatformCanvas pointing to it.
  transport_dib_.reset(TransportDIB::Create(buffer_size, ++next_buffer_id_));
  if (!transport_dib_.get())
    return NPERR_OUT_OF_MEMORY_ERROR;
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
      &src_rect, dest_rect);

  committed_bitmap->setIsOpaque(false);

  // Invoke the callback to inform the caller the work was done.
  // TODO(brettw) this is not how we want this to work, this should
  // happen when the frame is painted so the plugin knows when it can draw
  // the next frame.
  //
  // This should also be called in the failure cases as well.
  if (callback != NULL)
    (*callback)(id, context, NPERR_NO_ERROR, user_data);

  return NPERR_NO_ERROR;
}


AudioDeviceContext::~AudioDeviceContext() {
  if (stream_id_) {
    OnDestroy();
  }
}

NPError AudioDeviceContext::Initialize(
        AudioMessageFilter* filter, const NPDeviceContextAudioConfig* config,
        NPDeviceContextAudio* context) {
  filter_ = filter;
  context_= context;
  // Make sure we don't call init more than once.
  DCHECK_EQ(0, stream_id_);
  stream_id_ = filter_->AddDelegate(this);

  ViewHostMsg_Audio_CreateStream params;
  params.format = AudioManager::AUDIO_PCM_LINEAR;
  params.channels = config->outputChannelMap;
  params.sample_rate = config->sampleRate;
  switch (config->sampleType) {
    case NPAudioSampleTypeInt16:
      params.bits_per_sample = 16;
      break;
    case NPAudioSampleTypeFloat32:
      params.bits_per_sample = 32;
      break;
    default:
      return NPERR_INVALID_PARAM;
  }

  context->config = *config;
  params.packet_size = config->sampleFrameCount * config->outputChannelMap
      * (params.bits_per_sample >> 3);

  // TODO(neb): figure out if this number is grounded in reality
  params.buffer_capacity = params.packet_size * 3;

  LOG(INFO) << "Initializing Pepper Audio Context (" <<
      config->sampleFrameCount << "Hz, " << params.bits_per_sample <<
      " bits, " << config->outputChannelMap << "channels";

  filter->Send(new ViewHostMsg_CreateAudioStream(0, stream_id_, params));
  return NPERR_NO_ERROR;
}

void AudioDeviceContext::OnDestroy() {
  // Make sure we don't call destroy more than once.
  DCHECK_NE(0, stream_id_);
  filter_->RemoveDelegate(stream_id_);
  filter_->Send(new ViewHostMsg_CloseAudioStream(0, stream_id_));
  stream_id_ = 0;
}

void AudioDeviceContext::OnRequestPacket(
    size_t bytes_in_buffer, const base::Time& message_timestamp) {
  context_->config.callback(context_);
  filter_->Send(new ViewHostMsg_NotifyAudioPacketReady(0, stream_id_,
                                                        shared_memory_size_));
}

void AudioDeviceContext::OnStateChanged(
    ViewMsg_AudioStreamState state) {
}

void AudioDeviceContext::OnCreated(
    base::SharedMemoryHandle handle, size_t length) {
  shared_memory_.reset(new base::SharedMemory(handle, false));
  shared_memory_->Map(length);
  shared_memory_size_ = length;

  context_->outBuffer = shared_memory_->memory();
  // TODO(neb): call play after prefilling
  filter_->Send(new ViewHostMsg_PlayAudioStream(0, stream_id_));
}

void AudioDeviceContext::OnVolume(double volume) {
}

