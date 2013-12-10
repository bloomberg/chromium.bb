// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_CHROME_RENDER_FRAME_OBSERVER_H_
#define CHROME_RENDERER_CHROME_RENDER_FRAME_OBSERVER_H_

#include "content/public/renderer/render_frame_observer.h"

// This class holds the Chrome specific parts of RenderFrame, and has the same
// lifetime.
class ChromeRenderFrameObserver : public content::RenderFrameObserver {
 public:
  explicit ChromeRenderFrameObserver(content::RenderFrame* render_frame);
  virtual ~ChromeRenderFrameObserver();

 private:
  // RenderFrameObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // IPC handlers
  void OnSetIsPrerendering(bool is_prerendering);

  DISALLOW_COPY_AND_ASSIGN(ChromeRenderFrameObserver);
};

#endif  // CHROME_RENDERER_CHROME_RENDER_FRAME_OBSERVER_H_
