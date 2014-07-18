// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_WRENCH_ICON_PAINTER_H_
#define CHROME_BROWSER_UI_TOOLBAR_WRENCH_ICON_PAINTER_H_

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/image/image_skia.h"

namespace gfx {
class Canvas;
class MultiAnimation;
class Rect;
}
namespace ui {
class ThemeProvider;
}

// This class is used to draw the wrench icon. It can signify severity levels
// by changing the wrench icon to different colors.
class WrenchIconPainter : gfx::AnimationDelegate {
 public:
  enum BezelType {
    BEZEL_NONE,
    BEZEL_HOVER,
    BEZEL_PRESSED,
  };

  enum Severity {
    SEVERITY_NONE,
    SEVERITY_LOW,
    SEVERITY_MEDIUM,
    SEVERITY_HIGH,
  };

  class Delegate {
   public:
    virtual void ScheduleWrenchIconPaint() = 0;
   protected:
    virtual ~Delegate() {}
  };

  explicit WrenchIconPainter(Delegate* delegate);
  virtual ~WrenchIconPainter();

  // If |severity| is not |SEVERITY_NONE| then the wrench icon is colored to
  // match the severity level.
  void SetSeverity(Severity severity, bool animate);

  // A badge drawn on the top left.
  void set_badge(const gfx::ImageSkia& badge) { badge_ = badge; }

  void Paint(gfx::Canvas* canvas,
             ui::ThemeProvider* theme_provider,
             const gfx::Rect& rect,
             BezelType bezel_type);

 private:
  FRIEND_TEST_ALL_PREFIXES(WrenchIconPainterTest, PaintCallback);

  // AnimationDelegate:
  virtual void AnimationProgressed(const gfx::Animation* animation) OVERRIDE;

  // Gets the image ID used to draw bars for the current severity level.
  int GetCurrentSeverityImageID() const;

  Delegate* delegate_;
  Severity severity_;
  gfx::ImageSkia badge_;
  scoped_ptr<gfx::MultiAnimation> animation_;

  DISALLOW_COPY_AND_ASSIGN(WrenchIconPainter);
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_WRENCH_ICON_PAINTER_H_
