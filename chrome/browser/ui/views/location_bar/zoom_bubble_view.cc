// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/zoom_bubble_view.h"

#include "base/i18n/rtl.h"
#include "chrome/browser/chrome_page_zoom.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/zoom_view.h"
#include "chrome/browser/ui/zoom/zoom_controller.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents_view.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"

namespace {

// The number of milliseconds the bubble should stay on the screen if it will
// close automatically.
const int kBubbleCloseDelay = 1500;

// The bubble's padding from the screen edge, used in fullscreen.
const int kFullscreenPaddingEnd = 20;

}  // namespace

// static
ZoomBubbleView* ZoomBubbleView::zoom_bubble_ = NULL;

// static
void ZoomBubbleView::ShowBubble(content::WebContents* web_contents,
                                bool auto_close) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  DCHECK(browser && browser->window() && browser->fullscreen_controller());

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  bool is_fullscreen = browser->window()->IsFullscreen();
  views::View* anchor_view = is_fullscreen ?
      NULL : browser_view->GetLocationBarView()->zoom_view();

  // If the bubble is already showing in this window and its |auto_close_| value
  // is equal to |auto_close|, the bubble can be reused and only the label text
  // needs to be updated.
  if (zoom_bubble_ &&
      zoom_bubble_->anchor_view() == anchor_view &&
      zoom_bubble_->auto_close_ == auto_close) {
    zoom_bubble_->Refresh();
  } else {
    // If the bubble is already showing but its |auto_close_| value is not equal
    // to |auto_close|, the bubble's focus properties must change, so the
    // current bubble must be closed and a new one created.
    CloseBubble();

    zoom_bubble_ = new ZoomBubbleView(anchor_view,
                                      web_contents,
                                      auto_close,
                                      browser->fullscreen_controller());

    // If we're fullscreen, there is no anchor view, so parent the bubble to
    // the content area.
    if (is_fullscreen) {
      zoom_bubble_->set_parent_window(
          web_contents->GetView()->GetTopLevelNativeWindow());
    }

    views::BubbleDelegateView::CreateBubble(zoom_bubble_);

    // Adjust for fullscreen after creation as it relies on the content size.
    if (is_fullscreen)
      zoom_bubble_->AdjustForFullscreen(browser_view->GetBoundsInScreen());

    zoom_bubble_->GetWidget()->Show();
  }
}

// static
void ZoomBubbleView::CloseBubble() {
  if (zoom_bubble_)
    zoom_bubble_->Close();
}

// static
bool ZoomBubbleView::IsShowing() {
  return zoom_bubble_ != NULL;
}

ZoomBubbleView::ZoomBubbleView(views::View* anchor_view,
                               content::WebContents* web_contents,
                               bool auto_close,
                               FullscreenController* fullscreen_controller)
    : BubbleDelegateView(anchor_view, anchor_view ?
          views::BubbleBorder::TOP_RIGHT : views::BubbleBorder::NONE),
      label_(NULL),
      web_contents_(web_contents),
      auto_close_(auto_close) {
  // Compensate for built-in vertical padding in the anchor view's image.
  set_anchor_view_insets(gfx::Insets(5, 0, 5, 0));
  set_use_focusless(auto_close);
  set_notify_enter_exit_on_child(true);

  registrar_.Add(this,
                 chrome::NOTIFICATION_FULLSCREEN_CHANGED,
                 content::Source<FullscreenController>(fullscreen_controller));
}

ZoomBubbleView::~ZoomBubbleView() {
}

void ZoomBubbleView::AdjustForFullscreen(const gfx::Rect& screen_bounds) {
  DCHECK(!anchor_view());

  // TODO(dbeam): should RTL logic be done in views::BubbleDelegateView?
  const size_t bubble_half_width = width() / 2;
  const int x_pos = base::i18n::IsRTL() ?
      screen_bounds.x() + bubble_half_width + kFullscreenPaddingEnd :
      screen_bounds.right() - bubble_half_width - kFullscreenPaddingEnd;
  set_anchor_rect(gfx::Rect(x_pos, screen_bounds.y(), 0, 0));

  // Used to update |views::BubbleDelegate::anchor_rect_| in a semi-hacky way.
  // TODO(dbeam): update only the bounds of this view or its border or frame.
  SizeToContents();
}

void ZoomBubbleView::Refresh() {
  ZoomController* zoom_controller =
      ZoomController::FromWebContents(web_contents_);
  int zoom_percent = zoom_controller->zoom_percent();
  label_->SetText(
      l10n_util::GetStringFUTF16Int(IDS_TOOLTIP_ZOOM, zoom_percent));
  StartTimerIfNecessary();
}

void ZoomBubbleView::Close() {
  GetWidget()->Close();
}

void ZoomBubbleView::StartTimerIfNecessary() {
  if (auto_close_) {
    if (timer_.IsRunning()) {
      timer_.Reset();
    } else {
      timer_.Start(
          FROM_HERE,
          base::TimeDelta::FromMilliseconds(kBubbleCloseDelay),
          this,
          &ZoomBubbleView::Close);
    }
  }
}

void ZoomBubbleView::StopTimer() {
  timer_.Stop();
}

void ZoomBubbleView::OnMouseEntered(const ui::MouseEvent& event) {
  StopTimer();
}

void ZoomBubbleView::OnMouseExited(const ui::MouseEvent& event) {
  StartTimerIfNecessary();
}

void ZoomBubbleView::OnGestureEvent(ui::GestureEvent* event) {
  if (!zoom_bubble_ || !zoom_bubble_->auto_close_ ||
      event->type() != ui::ET_GESTURE_TAP) {
    return;
  }

  // If an auto-closing bubble was tapped, show a non-auto-closing bubble in
  // its place.
  ShowBubble(zoom_bubble_->web_contents_, false);
  event->SetHandled();
}

void ZoomBubbleView::ButtonPressed(views::Button* sender,
                                   const ui::Event& event) {
  chrome_page_zoom::Zoom(web_contents_, content::PAGE_ZOOM_RESET);
}

void ZoomBubbleView::Init() {
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical,
      0, 0, views::kRelatedControlVerticalSpacing));

  ZoomController* zoom_controller =
      ZoomController::FromWebContents(web_contents_);
  int zoom_percent = zoom_controller->zoom_percent();
  label_ = new views::Label(
      l10n_util::GetStringFUTF16Int(IDS_TOOLTIP_ZOOM, zoom_percent));
  label_->SetFont(
      ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::MediumFont));
  AddChildView(label_);

  views::NativeTextButton* set_default_button = new views::NativeTextButton(
      this, l10n_util::GetStringUTF16(IDS_ZOOM_SET_DEFAULT));
  set_default_button->set_alignment(views::TextButtonBase::ALIGN_CENTER);
  AddChildView(set_default_button);

  StartTimerIfNecessary();
}

void ZoomBubbleView::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  DCHECK_EQ(type, chrome::NOTIFICATION_FULLSCREEN_CHANGED);
  CloseBubble();
}

void ZoomBubbleView::WindowClosing() {
  // |zoom_bubble_| can be a new bubble by this point (as Close(); doesn't
  // call this right away). Only set to NULL when it's this bubble.
  if (zoom_bubble_ == this)
    zoom_bubble_ = NULL;
}
