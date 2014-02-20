// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gl_view_renderer_manager.h"

#include "base/logging.h"
#include "base/threading/platform_thread.h"

namespace android_webview {

using base::AutoLock;

GLViewRendererManager::GLViewRendererManager() {}

GLViewRendererManager::~GLViewRendererManager() {}

bool GLViewRendererManager::OnRenderThread() const {
  AutoLock auto_lock(lock_);
  return render_thread_.is_equal(base::PlatformThread::CurrentHandle());
}

void GLViewRendererManager::MarkRenderThread() {
  lock_.AssertAcquired();
  if (render_thread_.is_null())
    render_thread_ = base::PlatformThread::CurrentHandle();
  DCHECK(render_thread_.is_equal(base::PlatformThread::CurrentHandle()));
}

GLViewRendererManager::Key GLViewRendererManager::DidDrawGL(
    Key key,
    BrowserViewRenderer* view) {
  AutoLock auto_lock(lock_);
  MarkRenderThread();

  if (key == mru_list_.end()) {
    DCHECK(mru_list_.end() ==
           std::find(mru_list_.begin(), mru_list_.end(), view));
    mru_list_.push_front(view);
    return mru_list_.begin();
  } else {
    DCHECK(*key == view);
    mru_list_.splice(mru_list_.begin(), mru_list_, key);
    return key;
  }
}

void GLViewRendererManager::NoLongerExpectsDrawGL(Key key) {
  AutoLock auto_lock(lock_);
  DCHECK(mru_list_.end() != key);
  mru_list_.erase(key);
}

BrowserViewRenderer* GLViewRendererManager::GetMostRecentlyDrawn() const {
  AutoLock auto_lock(lock_);
  if (mru_list_.begin() == mru_list_.end())
    return NULL;
  return *mru_list_.begin();
}

GLViewRendererManager::Key GLViewRendererManager::NullKey() {
  AutoLock auto_lock(lock_);
  return mru_list_.end();
}

}  // namespace android_webview
