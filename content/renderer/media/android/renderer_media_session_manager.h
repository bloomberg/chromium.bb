// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_ANDROID_RENDERER_MEDIA_SESSION_MANAGER_H_
#define CONTENT_RENDERER_MEDIA_ANDROID_RENDERER_MEDIA_SESSION_MANAGER_H_

#include <map>

#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/public/renderer/render_frame_observer.h"

namespace content {

class WebMediaSessionAndroid;

class CONTENT_EXPORT RendererMediaSessionManager : public RenderFrameObserver {
 public:
  RendererMediaSessionManager(RenderFrame* render_frame);
  ~RendererMediaSessionManager() override;

  int RegisterMediaSession(WebMediaSessionAndroid* session);
  void UnregisterMediaSession(int session_id);

 private:
  friend class WebMediaSessionTest;

  std::map<int, WebMediaSessionAndroid*> sessions_;
  int next_session_id_;

  DISALLOW_COPY_AND_ASSIGN(RendererMediaSessionManager);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_ANDROID_RENDERER_MEDIA_SESSION_MANAGER_H_
