// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/media/ipc_video_renderer.h"

#include "chrome/common/render_messages.h"
#include "chrome/renderer/render_thread.h"
#include "media/base/video_frame.h"
#include "media/base/media_format.h"

IPCVideoRenderer::IPCVideoRenderer(
    webkit_glue::WebMediaPlayerImpl::Proxy* proxy,
    int routing_id)
    : proxy_(proxy),
      created_(false),
      routing_id_(routing_id),
      stopped_(false, false) {
  // TODO(hclam): decide whether to do the following line in this thread or
  // in the render thread.
  proxy->SetVideoRenderer(this);
}

IPCVideoRenderer::~IPCVideoRenderer() {
}

// static
media::FilterFactory* IPCVideoRenderer::CreateFactory(
    webkit_glue::WebMediaPlayerImpl::Proxy* proxy,
    int routing_id) {
  return new media::FilterFactoryImpl2<
      IPCVideoRenderer,
      webkit_glue::WebMediaPlayerImpl::Proxy*,
      int>(proxy, routing_id);
}

// static
bool IPCVideoRenderer::IsMediaFormatSupported(
    const media::MediaFormat& media_format) {
  return ParseMediaFormat(media_format, NULL, NULL, NULL, NULL);
}

bool IPCVideoRenderer::OnInitialize(media::VideoDecoder* decoder) {
  video_size_.SetSize(width(), height());

  // TODO(scherkus): we're assuming YV12 here.
  size_t size = (width() * height()) + ((width() * height()) >> 1);
  uint32 epoch = static_cast<uint32>(reinterpret_cast<size_t>(this));
  transport_dib_.reset(TransportDIB::Create(size, epoch));
  CHECK(transport_dib_.get());

  return true;
}

void IPCVideoRenderer::OnStop(media::FilterCallback* callback) {
  stopped_.Signal();

  proxy_->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(this, &IPCVideoRenderer::DoDestroyVideo, callback));
}

void IPCVideoRenderer::OnFrameAvailable() {
  proxy_->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(this, &IPCVideoRenderer::DoUpdateVideo));
}

void IPCVideoRenderer::SetRect(const gfx::Rect& rect) {
  DCHECK(MessageLoop::current() == proxy_->message_loop());

  // TODO(scherkus): this is actually a SetSize() call... there's a pending
  // WebKit bug to get this fixed up.  It would be nice if this was a real
  // SetRect() call so we could get the absolute coordinates instead of relying
  // on Paint().
}

void IPCVideoRenderer::Paint(skia::PlatformCanvas* canvas,
                             const gfx::Rect& dest_rect) {
  DCHECK(MessageLoop::current() == proxy_->message_loop());

  // Copy the rect for UpdateVideo messages.
  video_rect_ = dest_rect;

  if (!created_) {
    created_ = true;
    Send(new ViewHostMsg_CreateVideo(routing_id_, video_size_));

    // Force an update in case the first frame arrived before the first
    // Paint() call.
    DoUpdateVideo();
  }

  // TODO(scherkus): code to punch a hole through the backing store goes here.
  // We don't need it right away since we don't do a proper alpha composite
  // between the browser BackingStore and VideoLayer.
}

void IPCVideoRenderer::Send(IPC::Message* msg) {
  DCHECK(routing_id_ != MSG_ROUTING_NONE);
  DCHECK(routing_id_ == msg->routing_id());

  bool result = RenderThread::current()->Send(msg);
  LOG_IF(ERROR, !result) << "RenderThread::current()->Send(msg) failed";
}

void IPCVideoRenderer::DoUpdateVideo() {
  DCHECK(MessageLoop::current() == proxy_->message_loop());

  // Nothing to do if we don't know where we are positioned on the page.
  if (!created_ || video_rect_.IsEmpty() || stopped_.IsSignaled()) {
    return;
  }

  scoped_refptr<media::VideoFrame> frame;
  GetCurrentFrame(&frame);
  if (!frame) {
    PutCurrentFrame(frame);
    return;
  }

  CHECK(frame->width() == static_cast<size_t>(video_size_.width()));
  CHECK(frame->height() == static_cast<size_t>(video_size_.height()));
  CHECK(frame->format() == media::VideoFrame::YV12);
  CHECK(frame->planes() == 3);

  uint8* dest = reinterpret_cast<uint8*>(transport_dib_->memory());

  // Copy Y plane.
  const uint8* src = frame->data(media::VideoFrame::kYPlane);
  size_t stride = frame->stride(media::VideoFrame::kYPlane);
  for (size_t row = 0; row < frame->height(); ++row) {
    memcpy(dest, src, frame->width());
    dest += frame->width();
    src += stride;
  }

  // Copy U plane.
  src = frame->data(media::VideoFrame::kUPlane);
  stride = frame->stride(media::VideoFrame::kUPlane);
  for (size_t row = 0; row < frame->height() / 2; ++row) {
    memcpy(dest, src, frame->width() / 2);
    dest += frame->width() / 2;
    src += stride;
  }

  // Copy V plane.
  src = frame->data(media::VideoFrame::kVPlane);
  stride = frame->stride(media::VideoFrame::kVPlane);
  for (size_t row = 0; row < frame->height() / 2; ++row) {
    memcpy(dest, src, frame->width() / 2);
    dest += frame->width() / 2;
    src += stride;
  }

  PutCurrentFrame(frame);

  // Sanity check!
  uint8* expected = reinterpret_cast<uint8*>(transport_dib_->memory()) +
      transport_dib_->size();
  CHECK(dest == expected);

  Send(new ViewHostMsg_UpdateVideo(routing_id_,
                                   transport_dib_->id(),
                                   video_rect_));
}

void IPCVideoRenderer::DoDestroyVideo(media::FilterCallback* callback) {
  DCHECK(MessageLoop::current() == proxy_->message_loop());

  // We shouldn't receive any more messages after the browser receives this.
  Send(new ViewHostMsg_DestroyVideo(routing_id_));

  // Detach ourselves from the proxy.
  proxy_->SetVideoRenderer(NULL);
  proxy_ = NULL;
  if (callback) {
    callback->Run();
    delete callback;
  }
}
