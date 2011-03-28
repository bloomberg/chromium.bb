// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/fullscreen_exit_bubble.h"

#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "grit/generated_resources.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas_skia.h"
#include "views/screen.h"
#include "views/widget/root_view.h"
#include "views/window/window.h"

#if defined(OS_WIN)
#include "ui/base/l10n/l10n_util_win.h"
#include "views/widget/widget_win.h"
#elif defined(OS_LINUX)
#include "views/widget/widget_gtk.h"
#endif

// FullscreenExitView ----------------------------------------------------------

class FullscreenExitBubble::FullscreenExitView : public views::View {
 public:
  FullscreenExitView(FullscreenExitBubble* bubble,
                     const std::wstring& accelerator);
  virtual ~FullscreenExitView();

  // views::View
  virtual gfx::Size GetPreferredSize();

 private:
  static const int kPaddingPixels;  // Number of pixels around all sides of link

  // views::View
  virtual void Layout();
  virtual void OnPaint(gfx::Canvas* canvas);

  // Clickable hint text to show in the bubble.
  views::Link link_;
};

const int FullscreenExitBubble::FullscreenExitView::kPaddingPixels = 8;

FullscreenExitBubble::FullscreenExitView::FullscreenExitView(
    FullscreenExitBubble* bubble,
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
  link_.SetController(bubble);
  link_.SetFont(ResourceBundle::GetSharedInstance().GetFont(
      ResourceBundle::LargeFont));
  link_.SetNormalColor(SK_ColorWHITE);
  link_.SetHighlightedColor(SK_ColorWHITE);
  AddChildView(&link_);
}

FullscreenExitBubble::FullscreenExitView::~FullscreenExitView() {
}

gfx::Size FullscreenExitBubble::FullscreenExitView::GetPreferredSize() {
  gfx::Size preferred_size(link_.GetPreferredSize());
  preferred_size.Enlarge(kPaddingPixels * 2, kPaddingPixels * 2);
  return preferred_size;
}

void FullscreenExitBubble::FullscreenExitView::Layout() {
  gfx::Size link_preferred_size(link_.GetPreferredSize());
  link_.SetBounds(kPaddingPixels,
                  height() - kPaddingPixels - link_preferred_size.height(),
                  link_preferred_size.width(), link_preferred_size.height());
}

void FullscreenExitBubble::FullscreenExitView::OnPaint(gfx::Canvas* canvas) {
  // Create a round-bottomed rect to fill the whole View.
  SkRect rect;
  SkScalar padding = SkIntToScalar(kPaddingPixels);
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

// FullscreenExitBubble --------------------------------------------------------

const double FullscreenExitBubble::kOpacity = 0.7;
const int FullscreenExitBubble::kInitialDelayMs = 2300;
const int FullscreenExitBubble::kIdleTimeMs = 2300;
const int FullscreenExitBubble::kPositionCheckHz = 10;
const int FullscreenExitBubble::kSlideInRegionHeightPx = 4;
const int FullscreenExitBubble::kSlideInDurationMs = 350;
const int FullscreenExitBubble::kSlideOutDurationMs = 700;

FullscreenExitBubble::FullscreenExitBubble(
    views::Widget* frame,
    CommandUpdater::CommandUpdaterDelegate* delegate)
    : root_view_(frame->GetRootView()),
      delegate_(delegate),
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
  views::Widget::CreateParams params(views::Widget::CreateParams::TYPE_POPUP);
  params.transparent = true;
  params.can_activate = false;
  params.delete_on_destroy = false;
  popup_ = views::Widget::CreateWidget(params);
  popup_->SetOpacity(static_cast<unsigned char>(0xff * kOpacity));
  popup_->Init(frame->GetNativeView(), GetPopupRect(false));
  popup_->SetContentsView(view_);
  popup_->Show();  // This does not activate the popup.

  // Start the initial delay timer and begin watching the mouse.
  initial_delay_.Start(base::TimeDelta::FromMilliseconds(kInitialDelayMs), this,
                       &FullscreenExitBubble::CheckMousePosition);
  gfx::Point cursor_pos = views::Screen::GetCursorScreenPoint();
  last_mouse_pos_ = cursor_pos;
  views::View::ConvertPointToView(NULL, root_view_, &last_mouse_pos_);
  mouse_position_checker_.Start(
      base::TimeDelta::FromMilliseconds(1000 / kPositionCheckHz), this,
      &FullscreenExitBubble::CheckMousePosition);
}

FullscreenExitBubble::~FullscreenExitBubble() {
  // This is tricky.  We may be in an ATL message handler stack, in which case
  // the popup cannot be deleted yet.  We also can't blindly use
  // set_delete_on_destroy(true) on the popup to delete it when it closes,
  // because if the user closed the last tab while in fullscreen mode, Windows
  // has already destroyed the popup HWND by the time we get here, and thus
  // either the popup will already have been deleted (if we set this in our
  // constructor) or the popup will never get another OnFinalMessage() call (if
  // not, as currently).  So instead, we tell the popup to synchronously hide,
  // and then asynchronously close and delete itself.
  popup_->Close();
  MessageLoop::current()->DeleteSoon(FROM_HERE, popup_);
}

void FullscreenExitBubble::LinkActivated(views::Link* source, int event_flags) {
  delegate_->ExecuteCommand(IDC_FULLSCREEN);
}

void FullscreenExitBubble::AnimationProgressed(
    const ui::Animation* animation) {
  gfx::Rect popup_rect(GetPopupRect(false));
  if (popup_rect.IsEmpty()) {
    popup_->Hide();
  } else {
    popup_->SetBounds(popup_rect);
    popup_->Show();
  }
}
void FullscreenExitBubble::AnimationEnded(
    const ui::Animation* animation) {
  AnimationProgressed(animation);
}

void FullscreenExitBubble::CheckMousePosition() {
  // Desired behavior:
  //
  // +------------+-----------------------------+------------+
  // | _  _  _  _ | Exit full screen mode (F11) | _  _  _  _ |  Slide-in region
  // | _  _  _  _ \_____________________________/ _  _  _  _ |  Neutral region
  // |                                                       |  Slide-out region
  // :                                                       :
  //
  // * If app is not active, we hide the popup.
  // * If the mouse is offscreen or in the slide-out region, we hide the popup.
  // * If the mouse goes idle, we hide the popup.
  // * If the mouse is in the slide-in-region and not idle, we show the popup.
  // * If the mouse is in the neutral region and not idle, and the popup is
  //   currently sliding out, we show it again.  This facilitates users
  //   correcting us if they try to mouse horizontally towards the popup and
  //   unintentionally drop too low.
  // * Otherwise, we do nothing, because the mouse is in the neutral region and
  //   either the popup is hidden or the mouse is not idle, so we don't want to
  //   change anything's state.

  gfx::Point cursor_pos = views::Screen::GetCursorScreenPoint();
  gfx::Point transformed_pos(cursor_pos);
  views::View::ConvertPointToView(NULL, root_view_, &transformed_pos);

  // Check to see whether the mouse is idle.
  if (transformed_pos != last_mouse_pos_) {
    // The mouse moved; reset the idle timer.
    idle_timeout_.Stop();  // If the timer isn't running, this is a no-op.
    idle_timeout_.Start(base::TimeDelta::FromMilliseconds(kIdleTimeMs), this,
                        &FullscreenExitBubble::CheckMousePosition);
  }
  last_mouse_pos_ = transformed_pos;

  if ((!root_view_->GetWidget()->IsActive()) ||
      !root_view_->HitTest(transformed_pos) ||
      (cursor_pos.y() >= GetPopupRect(true).bottom()) ||
      !idle_timeout_.IsRunning()) {
    // The cursor is offscreen, in the slide-out region, or idle.
    Hide();
  } else if ((cursor_pos.y() < kSlideInRegionHeightPx) ||
             (size_animation_->GetCurrentValue() != 0)) {
    // The cursor is not idle, and either it's in the slide-in region or it's in
    // the neutral region and we're sliding out.
    size_animation_->SetSlideDuration(kSlideInDurationMs);
    size_animation_->Show();
  }
}

void FullscreenExitBubble::Hide() {
  // Allow the bubble to hide if the window is deactivated or our initial delay
  // finishes.
  if ((!root_view_->GetWidget()->IsActive()) || !initial_delay_.IsRunning()) {
    size_animation_->SetSlideDuration(kSlideOutDurationMs);
    size_animation_->Hide();
  }
}

gfx::Rect FullscreenExitBubble::GetPopupRect(
    bool ignore_animation_state) const {
  gfx::Size size(view_->GetPreferredSize());
  if (!ignore_animation_state) {
    size.set_height(static_cast<int>(static_cast<double>(size.height()) *
        size_animation_->GetCurrentValue()));
  }
  // NOTE: don't use the bounds of the root_view_. On linux changing window
  // size is async. Instead we use the size of the screen.
  gfx::Rect screen_bounds = views::Screen::GetMonitorAreaNearestWindow(
      root_view_->GetWidget()->GetNativeView());
  gfx::Point origin(screen_bounds.x() +
                    (screen_bounds.width() - size.width()) / 2,
                    screen_bounds.y());
  return gfx::Rect(origin, size);
}
