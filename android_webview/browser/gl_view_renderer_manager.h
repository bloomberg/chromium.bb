// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GL_VIEW_RENDERER_MANAGER_H_
#define ANDROID_WEBVIEW_BROWSER_GL_VIEW_RENDERER_MANAGER_H_

#include <list>

#include "base/basictypes.h"
#include "base/synchronization/lock.h"
#include "base/threading/platform_thread.h"

namespace android_webview {

class BrowserViewRenderer;

class GLViewRendererManager {
  typedef std::list<BrowserViewRenderer*> ListType;
 public:
  typedef ListType::iterator Key;

  GLViewRendererManager();
  ~GLViewRendererManager();

  bool OnRenderThread() const;

  // If |key| is NullKey(), then |view| is inserted at the front and a new key
  // is returned. Otherwise |key| must point to |view| which is moved to the
  // front.
  Key DidDrawGL(Key key, BrowserViewRenderer* view);

  void NoLongerExpectsDrawGL(Key key);

  BrowserViewRenderer* GetMostRecentlyDrawn() const;

  Key NullKey();

 private:
  void MarkRenderThread();

  mutable base::Lock lock_;
  base::PlatformThreadHandle render_thread_;
  ListType mru_list_;

  DISALLOW_COPY_AND_ASSIGN(GLViewRendererManager);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GL_VIEW_RENDERER_MANAGER_H_
