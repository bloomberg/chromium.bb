// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/aura/aura_init.h"

#include "chrome/browser/ui/views/chrome_views_delegate.h"
#include "ui/aura/desktop.h"
#include "ui/aura/hit_test.h"
#include "ui/aura/window_delegate.h"
#include "ui/gfx/canvas_skia.h"
#include "views/view.h"
#include "views/widget/widget.h"

namespace browser {

namespace {

// Trivial WindowDelegate implementation that draws a colored background.
class DemoWindowDelegate : public aura::WindowDelegate {
 public:
  explicit DemoWindowDelegate(SkColor color) : color_(color) {}

  // Overridden from aura::WindowDelegate:
  virtual void OnBoundsChanged(const gfx::Rect& old_bounds,
                               const gfx::Rect& new_bounds) OVERRIDE {}
  virtual void OnFocus() OVERRIDE {}
  virtual void OnBlur() OVERRIDE {}
  virtual bool OnKeyEvent(aura::KeyEvent* event) OVERRIDE {
    return false;
  }
  virtual int GetNonClientComponent(const gfx::Point& point) const OVERRIDE {
    return HTCLIENT;
  }
  virtual bool OnMouseEvent(aura::MouseEvent* event) OVERRIDE {
    return true;
  }
  virtual bool ShouldActivate(aura::MouseEvent* event) OVERRIDE {
    return true;
  }
  virtual void OnActivated() OVERRIDE {
  }
  virtual void OnLostActive() OVERRIDE {
  }
  virtual void OnCaptureLost() OVERRIDE {
  }
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    canvas->AsCanvasSkia()->drawColor(color_, SkXfermode::kSrc_Mode);
  }
  virtual void OnWindowDestroying() OVERRIDE {
  }
  virtual void OnWindowDestroyed() OVERRIDE {
    delete this;
  }

 private:
  SkColor color_;

  DISALLOW_COPY_AND_ASSIGN(DemoWindowDelegate);
};


class TestView : public views::View {
 public:
  TestView() {}
  virtual ~TestView() {}

  virtual void OnPaint(gfx::Canvas* canvas) {
    canvas->FillRectInt(SK_ColorRED, 0, 0, width(), height());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestView);
};

}  // namespace

void InitAuraDesktop() {
  aura::Desktop::GetInstance();
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_CONTROL);
  params.bounds = gfx::Rect(0, 0, 1024, 768);
  widget->Init(params);
  widget->SetContentsView(new views::View);
  widget->Show();
  ChromeViewsDelegate* chrome_views_delegate =
      static_cast<ChromeViewsDelegate*>(views::ViewsDelegate::views_delegate);
  chrome_views_delegate->default_parent_view = widget->GetContentsView();
  aura::Desktop::GetInstance()->Show();

  views::Widget* widget2 = new views::Widget;
  views::Widget::InitParams params2(views::Widget::InitParams::TYPE_CONTROL);
  params2.bounds = gfx::Rect(75, 75, 80, 80);
  params2.parent = aura::Desktop::GetInstance()->window();
  widget2->Init(params2);
  widget2->SetContentsView(new TestView);
  widget2->Show();

  DemoWindowDelegate* window_delegate1 = new DemoWindowDelegate(SK_ColorBLUE);
  aura::Window* window1 = new aura::Window(window_delegate1);
  window1->set_id(1);
  window1->Init();
  window1->SetBounds(gfx::Rect(100, 100, 400, 400));
  window1->SetVisibility(aura::Window::VISIBILITY_SHOWN);
  window1->SetParent(NULL);
}

}  // namespace browser
