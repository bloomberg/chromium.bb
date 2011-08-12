// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/fullscreen_exit_bubble_views.h"

#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "grit/generated_resources.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/screen.h"
#include "views/controls/link.h"
#include "views/widget/widget.h"

#if defined(OS_WIN)
#include "ui/base/l10n/l10n_util_win.h"
#endif

// FullscreenExitView ----------------------------------------------------------

class FullscreenExitBubbleViews::FullscreenExitView : public views::View {
 public:
  FullscreenExitView(FullscreenExitBubbleViews* bubble,
                     const std::wstring& accelerator);
  virtual ~FullscreenExitView();

  // views::View
  virtual gfx::Size GetPreferredSize();

 private:
  // views::View
  virtual void Layout();
  virtual void OnPaint(gfx::Canvas* canvas);

  // Clickable hint text to show in the bubble.
  views::Link link_;
};

FullscreenExitBubbleViews::FullscreenExitView::FullscreenExitView(
    FullscreenExitBubbleViews* bubble,
    const std::wstring& accelerator) {
  link_.set_parent_owned(false);
#if !defined(OS_CHROMEOS)
  link_.SetText(
      UTF16ToWide(l10n_util::GetStringFUTF16(IDS_EXIT_FULLSCREEN_MODE,
                                             WideToUTF16(accelerator))));
#else
  link_.SetText(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_EXIT_FULLSCREEN_MODE)));
#endif
  link_.set_listener(bubble);
  link_.SetFont(ResourceBundle::GetSharedInstance().GetFont(
      ResourceBundle::LargeFont));
  link_.SetNormalColor(SK_ColorWHITE);
  link_.SetHighlightedColor(SK_ColorWHITE);
  AddChildView(&link_);
}

FullscreenExitBubbleViews::FullscreenExitView::~FullscreenExitView() {
}

gfx::Size FullscreenExitBubbleViews::FullscreenExitView::GetPreferredSize() {
  gfx::Size preferred_size(link_.GetPreferredSize());
  preferred_size.Enlarge(kPaddingPx * 2, kPaddingPx * 2);
  return preferred_size;
}

void FullscreenExitBubbleViews::FullscreenExitView::Layout() {
  gfx::Size link_preferred_size(link_.GetPreferredSize());
  link_.SetBounds(kPaddingPx,
                  height() - kPaddingPx - link_preferred_size.height(),
                  link_preferred_size.width(), link_preferred_size.height());
}

void FullscreenExitBubbleViews::FullscreenExitView::OnPaint(
    gfx::Canvas* canvas) {
  // Create a round-bottomed rect to fill the whole View.
  SkRect rect;
  SkScalar padding = SkIntToScalar(kPaddingPx);
  // The "-padding" top coordinate ensures that the rect is always tall enough
  // to contain the complete rounded corner radius.  If we set this to 0, as the
  // popup slides offscreen (in reality, squishes to 0 height), the corners will
  // flatten out as the height becomes less than the corner radius.
  rect.set(0, -padding, SkIntToScalar(width()), SkIntToScalar(height()));
  SkScalar rad[8] = { 0, 0, 0, 0, padding, padding, padding, padding };
  SkPath path;
  path.addRoundRect(rect, rad, SkPath::kCW_Direction);

  // Fill it black.
  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);
  paint.setFlags(SkPaint::kAntiAlias_Flag);
  paint.setColor(SK_ColorBLACK);
  canvas->AsCanvasSkia()->drawPath(path, paint);
}

// FullscreenExitBubbleViews ---------------------------------------------------

FullscreenExitBubbleViews::FullscreenExitBubbleViews(
    views::Widget* frame,
    CommandUpdater::CommandUpdaterDelegate* delegate)
    : FullscreenExitBubble(delegate),
      root_view_(frame->GetRootView()),
      popup_(NULL),
      size_animation_(new ui::SlideAnimation(this)) {
  size_animation_->Reset(1);

  // Create the contents view.
  views::Accelerator accelerator(ui::VKEY_UNKNOWN, false, false, false);
  bool got_accelerator = frame->GetAccelerator(IDC_FULLSCREEN, &accelerator);
  DCHECK(got_accelerator);
  view_ = new FullscreenExitView(
      this, UTF16ToWideHack(accelerator.GetShortcutText()));

  // Initialize the popup.
  popup_ = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.transparent = true;
  params.can_activate = false;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = frame->GetNativeView();
  params.bounds = GetPopupRect(false);
  popup_->Init(params);
  popup_->SetContentsView(view_);
  popup_->SetOpacity(static_cast<unsigned char>(0xff * kOpacity));
  popup_->Show();  // This does not activate the popup.

  StartWatchingMouse();
}

FullscreenExitBubbleViews::~FullscreenExitBubbleViews() {
  // This is tricky.  We may be in an ATL message handler stack, in which case
  // the popup cannot be deleted yet.  We also can't set the popup's ownership
  // model to NATIVE_WIDGET_OWNS_WIDGET because if the user closed the last tab
  // while in fullscreen mode, Windows has already destroyed the popup HWND by
  // the time we get here, and thus either the popup will already have been
  // deleted (if we set this in our constructor) or the popup will never get
  // another OnFinalMessage() call (if not, as currently).  So instead, we tell
  // the popup to synchronously hide, and then asynchronously close and delete
  // itself.
  popup_->Close();
  MessageLoop::current()->DeleteSoon(FROM_HERE, popup_);
}

void FullscreenExitBubbleViews::LinkClicked(
    views::Link* source, int event_flags) {
  ToggleFullscreen();
}

void FullscreenExitBubbleViews::AnimationProgressed(
    const ui::Animation* animation) {
  gfx::Rect popup_rect(GetPopupRect(false));
  if (popup_rect.IsEmpty()) {
    popup_->Hide();
  } else {
    popup_->SetBounds(popup_rect);
    popup_->Show();
  }
}
void FullscreenExitBubbleViews::AnimationEnded(
    const ui::Animation* animation) {
  AnimationProgressed(animation);
}

void FullscreenExitBubbleViews::Hide() {
  size_animation_->SetSlideDuration(kSlideOutDurationMs);
  size_animation_->Hide();
}

void FullscreenExitBubbleViews::Show() {
  size_animation_->SetSlideDuration(kSlideInDurationMs);
  size_animation_->Show();
}

bool FullscreenExitBubbleViews::IsAnimating() {
  return size_animation_->GetCurrentValue() != 0;
}

bool FullscreenExitBubbleViews::IsWindowActive() {
  return root_view_->GetWidget()->IsActive();
}

bool FullscreenExitBubbleViews::WindowContainsPoint(gfx::Point pos) {
  return root_view_->HitTest(pos);
}

gfx::Point FullscreenExitBubbleViews::GetCursorScreenPoint() {
  gfx::Point cursor_pos = gfx::Screen::GetCursorScreenPoint();
  gfx::Point transformed_pos(cursor_pos);
  views::View::ConvertPointToView(NULL, root_view_, &transformed_pos);
  return transformed_pos;
}

gfx::Rect FullscreenExitBubbleViews::GetPopupRect(
    bool ignore_animation_state) const {
  gfx::Size size(view_->GetPreferredSize());
  if (!ignore_animation_state) {
    size.set_height(static_cast<int>(static_cast<double>(size.height()) *
        size_animation_->GetCurrentValue()));
  }
  // NOTE: don't use the bounds of the root_view_. On linux changing window
  // size is async. Instead we use the size of the screen.
  gfx::Rect screen_bounds = gfx::Screen::GetMonitorAreaNearestWindow(
      root_view_->GetWidget()->GetNativeView());
  gfx::Point origin(screen_bounds.x() +
                    (screen_bounds.width() - size.width()) / 2,
                    screen_bounds.y());
  return gfx::Rect(origin, size);
}
