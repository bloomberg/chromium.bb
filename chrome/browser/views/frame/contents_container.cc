// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/frame/contents_container.h"

#include "app/resource_bundle.h"
#include "chrome/browser/location_bar.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "grit/theme_resources.h"
#include "views/controls/image_view.h"
#include "views/widget/root_view.h"

#if defined(OS_WIN)
#include "views/widget/widget_win.h"
#elif defined(OS_LINUX)
#include "chrome/browser/gtk/gtk_util.h"
#include "views/window/window_gtk.h"
#endif

#if defined(OS_WIN)

class ContentsContainer::TearWindow : public views::WidgetWin {
 public:
  explicit TearWindow(ContentsContainer* contents_container)
      : contents_container_(contents_container) {
    set_window_style(WS_POPUP | WS_CLIPCHILDREN);
    set_window_ex_style(WS_EX_TOOLWINDOW | WS_EX_LAYERED);
  }

  virtual ~TearWindow() {
    // On windows it's possible for us to be deleted before the
    // ContentsContainer. If this happens make sure contents_container_ doesn't
    // attempt to delete us too.
    if (contents_container_)
      contents_container_->TearWindowDestroyed();
  }

  void set_contents_container(ContentsContainer* contents_container) {
    contents_container_ = contents_container;
  }

  virtual LRESULT OnMouseActivate(HWND window,
                                  UINT hit_test,
                                  UINT mouse_message) {
    // Don't activate the window when the user clicks it.
    contents_container_->browser_view_->GetLocationBar()->Revert();
    return MA_NOACTIVATE;
  }

 private:
  ContentsContainer* contents_container_;

  DISALLOW_COPY_AND_ASSIGN(TearWindow);
};

#endif

ContentsContainer::ContentsContainer(BrowserView* browser_view,
                                     views::View* active)
    : browser_view_(browser_view),
      active_(active),
      preview_(NULL),
      preview_tab_contents_(NULL),
      tear_window_(NULL),
      active_top_margin_(0) {
  AddChildView(active_);
}

ContentsContainer::~ContentsContainer() {
  DeleteTearWindow();
}

void ContentsContainer::MakePreviewContentsActiveContents() {
  active_ = preview_;
  preview_ = NULL;
  DeleteTearWindow();
  Layout();
}

void ContentsContainer::SetPreview(views::View* preview,
                                   TabContents* preview_tab_contents) {
  if (preview == preview_)
    return;

  if (preview_) {
    RemoveChildView(preview_);
    DeleteTearWindow();
  }
  preview_ = preview;
  preview_tab_contents_ = preview_tab_contents;
  if (preview_) {
    AddChildView(preview_);
    CreateTearWindow();
  }

  Layout();

  if (preview_)
    tear_window_->Show();  // Show after we'ved positioned it in Layout.
}

void ContentsContainer::SetActiveTopMargin(int margin) {
  if (active_top_margin_ == margin)
    return;

  active_top_margin_ = margin;
  // Make sure we layout next time around. We need this in case our bounds
  // haven't changed.
  InvalidateLayout();
}

void ContentsContainer::Layout() {
  // The active view always gets the full bounds.
  active_->SetBounds(0, active_top_margin_, width(),
                     std::max(0, height() - active_top_margin_));

  if (preview_) {
    preview_->SetBounds(0, 0, width(), height());
    PositionTearWindow();
  }

  // Need to invoke views::View in case any views whose bounds didn't change
  // still need a layout.
  views::View::Layout();
}

void ContentsContainer::CreateTearWindow() {
  DCHECK(preview_);
  tear_window_ = CreateTearWindowImpl();

  views::ImageView* image_view = new views::ImageView();
  image_view->SetImage(ResourceBundle::GetSharedInstance().GetBitmapNamed(
                           IDR_MATCH_PREVIEW_TEAR));
  tear_window_->SetContentsView(image_view);
}

#if defined(OS_WIN)

ContentsContainer::TearWindow* ContentsContainer::CreateTearWindowImpl() {
  TearWindow* widget = new TearWindow(this);
  widget->Init(browser_view_->GetNativeHandle(), gfx::Rect());
  return widget;
}

#elif defined(OS_LINUX)

ContentsContainer::TearWindow* ContentsContainer::CreateTearWindowImpl() {
  views::WidgetGtk* widget = new views::WidgetGtk(views::WidgetGtk::TYPE_POPUP);
  widget->MakeTransparent();
  widget->Init(NULL, gfx::Rect());
  gtk_util::StackPopupWindow(widget->GetNativeView(),
                             GTK_WIDGET(browser_view_->GetNativeHandle()));
  return widget;
}

#endif

void ContentsContainer::PositionTearWindow() {
  if (!tear_window_)
    return;

  gfx::Rect vis_bounds = GetVisibleBounds();

  gfx::Size pref = tear_window_->GetRootView()->GetPreferredSize();
  // Constrain to the the visible bounds as we may be given a different size
  // than is actually visible.
  pref.SetSize(std::min(pref.width(), vis_bounds.width()),
               std::min(pref.height(), vis_bounds.height()));

  gfx::Rect bounds(0, 0, pref.width(), pref.height());
  bounds.set_x(MirroredLeftPointForRect(bounds));

  gfx::Point origin(bounds.origin());
  views::View::ConvertPointToScreen(this, &origin);

  tear_window_->SetBounds(gfx::Rect(origin, pref));
}

void ContentsContainer::DeleteTearWindow() {
  if (!tear_window_)
    return;

  tear_window_->Close();
#if defined(OS_WIN)
  tear_window_->set_contents_container(NULL);
#endif
  // Close deletes the tear window.
  tear_window_ = NULL;
}

void ContentsContainer::TearWindowDestroyed() {
  tear_window_ = NULL;
}
