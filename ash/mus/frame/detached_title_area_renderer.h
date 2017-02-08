// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_FRAME_DETACHED_TITLE_AREA_RENDERER_H_
#define ASH_MUS_FRAME_DETACHED_TITLE_AREA_RENDERER_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "ui/views/widget/widget_delegate.h"

namespace aura {
class Window;
}

namespace ash {

namespace mus {

class WindowManager;

// DetachedTitleAreaRenderer contains a HeaderView in a widget. It's intended to
// be used when the title area needs to be rendered in a window different from
// the normal window the title area renders to (such as in immersive mode).
//
// DetachedTitleAreaRenderer comes in two variants.
// DetachedTitleAreaRendererForClient is used for clients that need to draw
// into the non-client area of the widget. For example, Chrome browser windows
// draw into the non-client area of tabbed browser widgets (the tab strip
// is in the non-client area). In such a case
// DetachedTitleAreaRendererForClient is used.
// If the client does not need to draw to the non-client area then
// DetachedTitleAreaRendererInternal is used (and ash controls the whole
// immersive experience). Which is used is determined by
// |kRenderParentTitleArea_Property|. If |kRenderParentTitleArea_Property| is
// false DetachedTitleAreaRendererInternal is used.

// DetachedTitleAreaRendererInternal owns the widget it creates.
class DetachedTitleAreaRendererForInternal {
 public:
  // |frame| is the Widget the decorations are configured from.
  explicit DetachedTitleAreaRendererForInternal(views::Widget* frame);
  ~DetachedTitleAreaRendererForInternal();

  views::Widget* widget() { return widget_.get(); }

 private:
  std::unique_ptr<views::Widget> widget_;

  DISALLOW_COPY_AND_ASSIGN(DetachedTitleAreaRendererForInternal);
};

// Used when the client wants to control, and possibly render to, the widget
// hosting the frame decorations. In this mode the client owns the window
// backing the widget and controls the lifetime of the window.
class DetachedTitleAreaRendererForClient : public views::WidgetDelegate {
 public:
  DetachedTitleAreaRendererForClient(
      aura::Window* parent,
      std::map<std::string, std::vector<uint8_t>>* properties,
      WindowManager* window_manager);

  static DetachedTitleAreaRendererForClient* ForWindow(aura::Window* window);

  void Attach(views::Widget* frame);
  void Detach();

  bool is_attached() const { return is_attached_; }

  views::Widget* widget() { return widget_; }

  // views::WidgetDelegate:
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;
  void DeleteDelegate() override;

 private:
  ~DetachedTitleAreaRendererForClient() override;

  views::Widget* widget_;

  // Has Attach() been called?
  bool is_attached_ = false;

  DISALLOW_COPY_AND_ASSIGN(DetachedTitleAreaRendererForClient);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_FRAME_DETACHED_TITLE_AREA_RENDERER_H_
