// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gl_view_renderer_manager.h"

#include "base/logging.h"
#include "base/threading/platform_thread.h"

namespace android_webview {

using base::AutoLock;

namespace {
base::LazyInstance<GLViewRendererManager>::Leaky g_view_renderer_manager =
    LAZY_INSTANCE_INITIALIZER;
}  // namespace

// static
GLViewRendererManager* GLViewRendererManager::GetInstance() {
  return g_view_renderer_manager.Pointer();
}

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

GLViewRendererManager::Key GLViewRendererManager::PushBack(RendererType view) {
  MarkRenderThread();
  DCHECK(mru_list_.end() ==
         std::find(mru_list_.begin(), mru_list_.end(), view));
  mru_list_.push_back(view);
  Key back = mru_list_.end();
  back--;
  return back;
}

void GLViewRendererManager::DidDrawGL(Key key) {
  AutoLock auto_lock(lock_);
  DCHECK(mru_list_.end() != key);
  mru_list_.splice(mru_list_.begin(), mru_list_, key);
}

void GLViewRendererManager::Remove(Key key) {
  AutoLock auto_lock(lock_);
  DCHECK(mru_list_.end() != key);
  mru_list_.erase(key);
}

GLViewRendererManager::RendererType
GLViewRendererManager::GetMostRecentlyDrawn() const {
  AutoLock auto_lock(lock_);
  if (mru_list_.begin() == mru_list_.end())
    return NULL;
  return *mru_list_.begin();
}

}  // namespace android_webview
