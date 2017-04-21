// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_TEXTURES_UI_TEXTURE_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_TEXTURES_UI_TEXTURE_H_

#include <memory>

#include "base/strings/string16.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {
class Canvas;
class FontList;
}  // namespace gfx

namespace vr_shell {

class UITexture {
 public:
  UITexture(int texture_handle, int texture_size);
  virtual ~UITexture();

  void DrawAndLayout();
  void Flush();
  gfx::Size size() const { return size_; }

 protected:
  virtual void Draw(gfx::Canvas* canvas) = 0;
  virtual void SetSize() = 0;
  static bool IsRTL();
  static gfx::FontList GetFontList(int size, base::string16 text);

  int texture_handle_;
  int texture_size_;
  gfx::Size size_;
  sk_sp<SkSurface> surface_;
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_TEXTURES_UI_TEXTURE_H_
