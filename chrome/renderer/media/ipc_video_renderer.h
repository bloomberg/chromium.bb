// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This video renderer implementation uses IPC to signal the browser process to
// composite the video as a separate layer underneath the backing store.
//
// Extremely experimental.  Use at own risk!

#ifndef CHROME_RENDERER_MEDIA_IPC_VIDEO_RENDERER_H_
#define CHROME_RENDERER_MEDIA_IPC_VIDEO_RENDERER_H_
#pragma once

#include "app/surface/transport_dib.h"
#include "base/waitable_event.h"
#include "gfx/rect.h"
#include "gfx/size.h"
#include "ipc/ipc_message.h"
#include "media/filters/video_renderer_base.h"
#include "webkit/glue/media/web_video_renderer.h"
#include "webkit/glue/webmediaplayer_impl.h"

class IPCVideoRenderer : public webkit_glue::WebVideoRenderer {
 public:
  // WebVideoRenderer implementation.
  virtual void SetRect(const gfx::Rect& rect);
  virtual void Paint(skia::PlatformCanvas* canvas, const gfx::Rect& dest_rect);

  void OnUpdateVideo();
  void OnUpdateVideoAck();
  void OnDestroyVideo();

  // Static method for creating factory for this object.
  static media::FilterFactory* CreateFactory(
      webkit_glue::WebMediaPlayerImpl::Proxy* proxy,
      int routing_id);

  // FilterFactoryImpl2 implementation.
  static bool IsMediaFormatSupported(const media::MediaFormat& media_format);

  // TODO(scherkus): remove this mega-hack, see http://crbug.com/28207
  class FactoryFactory : public webkit_glue::WebVideoRendererFactoryFactory {
   public:
    explicit FactoryFactory(int routing_id)
        : webkit_glue::WebVideoRendererFactoryFactory(),
          routing_id_(routing_id) {
    }

    virtual media::FilterFactory* CreateFactory(
        webkit_glue::WebMediaPlayerImpl::Proxy* proxy) {
      return IPCVideoRenderer::CreateFactory(proxy, routing_id_);
    }

   private:
    int routing_id_;

    DISALLOW_COPY_AND_ASSIGN(FactoryFactory);
  };

 protected:
  // VideoRendererBase implementation.
  virtual bool OnInitialize(media::VideoDecoder* decoder);
  virtual void OnStop(media::FilterCallback* callback);
  virtual void OnFrameAvailable();

 private:
  // Only the filter factories can create instances.
  friend class media::FilterFactoryImpl2<
      IPCVideoRenderer,
      webkit_glue::WebMediaPlayerImpl::Proxy*,
      int>;
  IPCVideoRenderer(webkit_glue::WebMediaPlayerImpl::Proxy* proxy,
                   int routing_id);
  virtual ~IPCVideoRenderer();

  // Send an IPC message to the browser process.  The routing ID of the message
  // is assumed to match |routing_id_|.
  void Send(IPC::Message* msg);

  // Handles updating the video on the render thread.
  void DoUpdateVideo();

  // Handles destroying the video on the render thread.
  void DoDestroyVideo(media::FilterCallback* callback);

  // Pointer to our parent object that is called to request repaints.
  scoped_refptr<webkit_glue::WebMediaPlayerImpl::Proxy> proxy_;

  // The size of the video.
  gfx::Size video_size_;

  // The rect of the video.
  gfx::Rect video_rect_;

  // Whether we've created the video layer on the browser.
  bool created_;

  // Used to transporting YUV to the browser process.
  int routing_id_;
  scoped_ptr<TransportDIB> transport_dib_;

  // Used to determine whether we've been instructed to stop.
  // TODO(scherkus): work around because we don't have asynchronous stopping.
  // Refer to http://crbug.com/16059
  base::WaitableEvent stopped_;

  DISALLOW_COPY_AND_ASSIGN(IPCVideoRenderer);
};

#endif  // CHROME_RENDERER_MEDIA_IPC_VIDEO_RENDERER_H_
