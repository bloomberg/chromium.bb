// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_COMMON_RENDERER_PICTURE_MAP_H_
#define ANDROID_WEBVIEW_COMMON_RENDERER_PICTURE_MAP_H_

#include <map>

#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "cc/picture_pile_impl.h"

namespace android_webview {

// Stores picture piles received from diferent renderers and associates them by
// renderer id. Will only work in single process mode.
class RendererPictureMap {
 public:
  static void CreateInstance();
  static RendererPictureMap* GetInstance();

  scoped_refptr<cc::PicturePileImpl> GetRendererPicture(int id);
  void SetRendererPicture(int id, scoped_refptr<cc::PicturePileImpl> picture);
  void ClearRendererPicture(int id);

 private:
  RendererPictureMap();
  ~RendererPictureMap();

  typedef std::map<int, scoped_refptr<cc::PicturePileImpl> > PictureMap;
  PictureMap picture_map_;
  base::Lock lock_;
};

}  // android_webview

#endif  // ANDROID_WEBVIEW_COMMON_RENDERER_PICTURE_MAP_H_
