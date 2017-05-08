// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_TEXTURES_UI_TEXTURE_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_TEXTURES_UI_TEXTURE_H_

#include <memory>

#include "base/strings/string16.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"

class SkCanvas;

namespace gfx {
class FontList;
}  // namespace gfx

namespace vr_shell {

class UiTexture {
 public:
  UiTexture();
  virtual ~UiTexture();

  void DrawAndLayout(SkCanvas* canvas, const gfx::Size& texture_size);
  virtual gfx::Size GetPreferredTextureSize(int maximum_width) const = 0;
  virtual gfx::SizeF GetDrawnSize() const = 0;

 protected:
  virtual void Draw(SkCanvas* canvas, const gfx::Size& texture_size) = 0;

  static bool IsRTL();
  static gfx::FontList GetDefaultFontList(int size);
  static gfx::FontList GetFontList(int size, base::string16 text);
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_TEXTURES_UI_TEXTURE_H_
