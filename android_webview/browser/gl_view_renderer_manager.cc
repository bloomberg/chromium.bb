// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gl_view_renderer_manager.h"

#include "base/logging.h"

namespace android_webview {

GLViewRendererManager::GLViewRendererManager() {}

GLViewRendererManager::~GLViewRendererManager() {}

GLViewRendererManager::Key GLViewRendererManager::DidDrawGL(
    Key key,
    BrowserViewRenderer* view) {
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
  DCHECK(mru_list_.end() != key);
  mru_list_.erase(key);
}

BrowserViewRenderer* GLViewRendererManager::GetMostRecentlyDrawn() const {
  if (mru_list_.begin() == mru_list_.end())
    return NULL;
  return *mru_list_.begin();
}

GLViewRendererManager::Key GLViewRendererManager::NullKey() {
  return mru_list_.end();
}

}  // namespace android_webview
