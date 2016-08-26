// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_FRAME_DETACHED_TITLE_AREA_RENDERER_H_
#define ASH_MUS_FRAME_DETACHED_TITLE_AREA_RENDERER_H_

#include "base/macros.h"
#include "ui/views/widget/widget_delegate.h"

namespace ui {
class Window;
}

namespace ash {
namespace mus {

class DetachedTitleAreaRendererHost;

// DetachedTitleAreaRenderer contains a HeaderView in a widget. It's intended to
// be used when the title area needs to be rendered in a window different from
// the normal window the title area renders to (such as in immersive mode).
class DetachedTitleAreaRenderer : public views::WidgetDelegate {
 public:
  // Used to indicate why this is being created.
  enum class Source {
    // This is being created at the request of a client, specifically because
    // of kRendererParentTitleArea_Property set on a client owned window.
    CLIENT,

    // Mash is creating this class to host an immersive reveal. Note that CLIENT
    // is also likely used for an immersive reveal, but for CLIENT the client
    // is completely controlling the reveal and not mash. In other words for
    // CLIENT ImmersiveFullscreenController is not running in mash.
    MASH,
  };

  // Creates a widget to render the title area and shows it. |window| is the
  // window to render to and |widget| the widget whose frame state is rendered
  // to |window|. This object is deleted either when |window| is destroyed, or
  // Destroy() is called.
  DetachedTitleAreaRenderer(DetachedTitleAreaRendererHost* host,
                            views::Widget* frame,
                            ui::Window* window,
                            Source source);

  void Destroy();

  views::Widget* widget() { return widget_; }

  // views::WidgetDelegate:
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;
  void DeleteDelegate() override;

 private:
  ~DetachedTitleAreaRenderer() override;

  DetachedTitleAreaRendererHost* host_;
  views::Widget* frame_;
  views::Widget* widget_;

  DISALLOW_COPY_AND_ASSIGN(DetachedTitleAreaRenderer);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_FRAME_DETACHED_TITLE_AREA_RENDERER_H_
