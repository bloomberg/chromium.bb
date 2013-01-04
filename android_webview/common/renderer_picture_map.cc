// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/common/renderer_picture_map.h"

using base::AutoLock;

namespace android_webview {

static RendererPictureMap* g_renderer_picture_map = NULL;

// static
void RendererPictureMap::CreateInstance() {
  if (!g_renderer_picture_map)
    g_renderer_picture_map = new RendererPictureMap();
}

// static
RendererPictureMap* RendererPictureMap::GetInstance() {
  DCHECK(g_renderer_picture_map);
  return g_renderer_picture_map;
}

RendererPictureMap::RendererPictureMap() {
}

RendererPictureMap::~RendererPictureMap() {
}

scoped_refptr<cc::PicturePileImpl> RendererPictureMap::GetRendererPicture(
    int id) {
  AutoLock lock(lock_);
  return picture_map_[id];
}

void RendererPictureMap::SetRendererPicture(int id,
    scoped_refptr<cc::PicturePileImpl> picture) {
  AutoLock lock(lock_);
  picture_map_[id] = picture;
}

void RendererPictureMap::ClearRendererPicture(int id) {
  AutoLock lock(lock_);
  picture_map_.erase(id);
}

}  // android_webview
