// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/status_bubble_views.h"

#include <algorithm>

#include "ash/wm/window_state.h"
#include "base/bind.h"
#include "base/i18n/rtl.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/elide_url.h"
#include "net/base/net_util.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/aura/window.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/text_utils.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/scrollbar/native_scroll_bar.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

// The alpha and color of the bubble's shadow.
static const SkColor kShadowColor = SkColorSetARGB(30, 0, 0, 0);

// The roundedness of the edges of our bubble.
static const int kBubbleCornerRadius = 4;

// How close the mouse can get to the infobubble before it starts sliding
// off-screen.
static const int kMousePadding = 20;

// The horizontal offset of the text within the status bubble, not including the
// outer shadow ring.
static const int kTextPositionX = 3;

// The minimum horizontal space between the (right) end of the text and the edge
// of the status bubble, not including the outer shadow ring.
static const int kTextHorizPadding = 1;

// Delays before we start hiding or showing the bubble after we receive a
// show or hide request.
static const int kShowDelay = 80;
static const int kHideDelay = 250;

// How long each fade should last for.
static const int kShowFadeDurationMS = 120;
static const int kHideFadeDurationMS = 200;
static const int kFramerate = 25;

// How long each expansion step should take.
static const int kMinExpansionStepDurationMS = 20;
static const int kMaxExpansionStepDurationMS = 150;


// StatusBubbleViews::StatusViewAnimation --------------------------------------
class StatusBubbleViews::StatusViewAnimation : public gfx::LinearAnimation,
                                               public gfx::AnimationDelegate {
 public:
  StatusViewAnimation(StatusView* status_view,
                      double opacity_start,
                      double opacity_end);
  virtual ~StatusViewAnimation();

  double GetCurrentOpacity();

 private:
  // gfx::LinearAnimation:
  virtual void AnimateToState(double state) OVERRIDE;

  // gfx::AnimationDelegate:
  virtual void AnimationEnded(const Animation* animation) OVERRIDE;

  StatusView* status_view_;

  // Start and end opacities for the current transition - note that as a
  // fade-in can easily turn into a fade out, opacity_start_ is sometimes
  // a value between 0 and 1.
  double opacity_start_;
  double opacity_end_;

  DISALLOW_COPY_AND_ASSIGN(StatusViewAnimation);
};


// StatusBubbleViews::StatusView -----------------------------------------------
//
// StatusView manages the display of the bubble, applying text changes and
// fading in or out the bubble as required.
class StatusBubbleViews::StatusView : public views::View {
 public:
  // The bubble can be in one of many states:
  enum BubbleState {
    BUBBLE_HIDDEN,         // Entirely BUBBLE_HIDDEN.
    BUBBLE_HIDING_FADE,    // In a fade-out transition.
    BUBBLE_HIDING_TIMER,   // Waiting before a fade-out.
    BUBBLE_SHOWING_TIMER,  // Waiting before a fade-in.
    BUBBLE_SHOWING_FADE,   // In a fade-in transition.
    BUBBLE_SHOWN           // Fully visible.
  };

  enum BubbleStyle {
    STYLE_BOTTOM,
    STYLE_FLOATING,
    STYLE_STANDARD,
    STYLE_STANDARD_RIGHT
  };

  StatusView(views::Widget* popup,
             ui::ThemeProvider* theme_provider);
  virtual ~StatusView();

  // Set the bubble text to a certain value, hides the bubble if text is
  // an empty string.  Trigger animation sequence to display if
  // |should_animate_open|.
  void SetText(const base::string16& text, bool should_animate_open);

  BubbleState state() const { return state_; }
  BubbleStyle style() const { return style_; }
  void SetStyle(BubbleStyle style);

  // Show the bubble instantly.
  void Show();

  // Hide the bubble instantly.
  void Hide();

  // Resets any timers we have. Typically called when the user moves a
  // mouse.
  void ResetTimer();

  // This call backs the StatusView in order to fade the bubble in and out.
  void SetOpacity(double opacity);

  // Depending on the state of the bubble this will either hide the popup or
  // not.
  void OnAnimationEnded();

 private:
  class InitialTimer;

  // Manage the timers that control the delay before a fade begins or ends.
  void StartTimer(base::TimeDelta time);
  void OnTimer();
  void CancelTimer();
  void RestartTimer(base::TimeDelta delay);

  // Manage the fades and starting and stopping the animations correctly.
  void StartFade(double start, double end, int duration);
  void StartHiding();
  void StartShowing();

  // views::View:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

  BubbleState state_;
  BubbleStyle style_;

  base::WeakPtrFactory<StatusBubbleViews::StatusView> timer_factory_;

  scoped_ptr<StatusViewAnimation> animation_;

  // Handle to the widget that contains us.
  views::Widget* popup_;

  // The currently-displayed text.
  base::string16 text_;

  // Holds the theme provider of the frame that created us.
  ui::ThemeProvider* theme_service_;

  DISALLOW_COPY_AND_ASSIGN(StatusView);
};

StatusBubbleViews::StatusView::StatusView(views::Widget* popup,
                                          ui::ThemeProvider* theme_provider)
    : state_(BUBBLE_HIDDEN),
      style_(STYLE_STANDARD),
      timer_factory_(this),
      animation_(new StatusViewAnimation(this, 0, 0)),
      popup_(popup),
      theme_service_(theme_provider) {
}

StatusBubbleViews::StatusView::~StatusView() {
  animation_->Stop();
  CancelTimer();
}

void StatusBubbleViews::StatusView::SetText(const base::string16& text,
                                            bool should_animate_open) {
  if (text.empty()) {
    // The string was empty.
    StartHiding();
  } else {
    // We want to show the string.
    if (text != text_) {
      text_ = text;
      SchedulePaint();
    }
    if (should_animate_open)
      StartShowing();
  }
}

void StatusBubbleViews::StatusView::Show() {
  animation_->Stop();
  CancelTimer();
  SetOpacity(1.0);
  popup_->ShowInactive();
  state_ = BUBBLE_SHOWN;
}

void StatusBubbleViews::StatusView::Hide() {
  animation_->Stop();
  CancelTimer();
  SetOpacity(0.0);
  text_.clear();
  popup_->Hide();
  state_ = BUBBLE_HIDDEN;
}

void StatusBubbleViews::StatusView::StartTimer(base::TimeDelta time) {
  if (timer_factory_.HasWeakPtrs())
    timer_factory_.InvalidateWeakPtrs();

  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&StatusBubbleViews::StatusView::OnTimer,
                 timer_factory_.GetWeakPtr()),
      time);
}

void StatusBubbleViews::StatusView::OnTimer() {
  if (state_ == BUBBLE_HIDING_TIMER) {
    state_ = BUBBLE_HIDING_FADE;
    StartFade(1.0, 0.0, kHideFadeDurationMS);
  } else if (state_ == BUBBLE_SHOWING_TIMER) {
    state_ = BUBBLE_SHOWING_FADE;
    StartFade(0.0, 1.0, kShowFadeDurationMS);
  }
}

void StatusBubbleViews::StatusView::CancelTimer() {
  if (timer_factory_.HasWeakPtrs())
    timer_factory_.InvalidateWeakPtrs();
}

void StatusBubbleViews::StatusView::RestartTimer(base::TimeDelta delay) {
  CancelTimer();
  StartTimer(delay);
}

void StatusBubbleViews::StatusView::ResetTimer() {
  if (state_ == BUBBLE_SHOWING_TIMER) {
    // We hadn't yet begun showing anything when we received a new request
    // for something to show, so we start from scratch.
    RestartTimer(base::TimeDelta::FromMilliseconds(kShowDelay));
  }
}

void StatusBubbleViews::StatusView::StartFade(double start,
                                              double end,
                                              int duration) {
  animation_.reset(new StatusViewAnimation(this, start, end));

  // This will also reset the currently-occurring animation.
  animation_->SetDuration(duration);
  animation_->Start();
}

void StatusBubbleViews::StatusView::StartHiding() {
  if (state_ == BUBBLE_SHOWN) {
    state_ = BUBBLE_HIDING_TIMER;
    StartTimer(base::TimeDelta::FromMilliseconds(kHideDelay));
  } else if (state_ == BUBBLE_SHOWING_TIMER) {
    state_ = BUBBLE_HIDDEN;
    popup_->Hide();
    CancelTimer();
  } else if (state_ == BUBBLE_SHOWING_FADE) {
    state_ = BUBBLE_HIDING_FADE;
    // Figure out where we are in the current fade.
    double current_opacity = animation_->GetCurrentOpacity();

    // Start a fade in the opposite direction.
    StartFade(current_opacity, 0.0,
              static_cast<int>(kHideFadeDurationMS * current_opacity));
  }
}

void StatusBubbleViews::StatusView::StartShowing() {
  if (state_ == BUBBLE_HIDDEN) {
    popup_->ShowInactive();
    state_ = BUBBLE_SHOWING_TIMER;
    StartTimer(base::TimeDelta::FromMilliseconds(kShowDelay));
  } else if (state_ == BUBBLE_HIDING_TIMER) {
    state_ = BUBBLE_SHOWN;
    CancelTimer();
  } else if (state_ == BUBBLE_HIDING_FADE) {
    // We're partway through a fade.
    state_ = BUBBLE_SHOWING_FADE;

    // Figure out where we are in the current fade.
    double current_opacity = animation_->GetCurrentOpacity();

    // Start a fade in the opposite direction.
    StartFade(current_opacity, 1.0,
              static_cast<int>(kShowFadeDurationMS * current_opacity));
  } else if (state_ == BUBBLE_SHOWING_TIMER) {
    // We hadn't yet begun showing anything when we received a new request
    // for something to show, so we start from scratch.
    ResetTimer();
  }
}

void StatusBubbleViews::StatusView::SetOpacity(double opacity) {
  popup_->SetOpacity(static_cast<unsigned char>(opacity * 255));
}

void StatusBubbleViews::StatusView::SetStyle(BubbleStyle style) {
  if (style_ != style) {
    style_ = style;
    SchedulePaint();
  }
}

void StatusBubbleViews::StatusView::OnAnimationEnded() {
  if (state_ == BUBBLE_HIDING_FADE) {
    state_ = BUBBLE_HIDDEN;
    popup_->Hide();
  } else if (state_ == BUBBLE_SHOWING_FADE) {
    state_ = BUBBLE_SHOWN;
  }
}

void StatusBubbleViews::StatusView::OnPaint(gfx::Canvas* canvas) {
  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);
  paint.setAntiAlias(true);
  SkColor toolbar_color = theme_service_->GetColor(
      ThemeProperties::COLOR_TOOLBAR);
  paint.setColor(toolbar_color);

  gfx::Rect popup_bounds = popup_->GetWindowBoundsInScreen();

  SkScalar rad[8] = {};

  // Top Edges - if the bubble is in its bottom position (sticking downwards),
  // then we square the top edges. Otherwise, we square the edges based on the
  // position of the bubble within the window (the bubble is positioned in the
  // southeast corner in RTL and in the southwest corner in LTR).
  if (style_ != STYLE_BOTTOM) {
    if (base::i18n::IsRTL() != (style_ == STYLE_STANDARD_RIGHT)) {
      // The text is RtL or the bubble is on the right side (but not both).

      // Top Left corner.
      rad[0] = kBubbleCornerRadius;
      rad[1] = kBubbleCornerRadius;
    } else {
      // Top Right corner.
      rad[2] = kBubbleCornerRadius;
      rad[3] = kBubbleCornerRadius;
    }
  }

  // Bottom edges - Keep these squared off if the bubble is in its standard
  // position (sticking upward).
  if (style_ != STYLE_STANDARD && style_ != STYLE_STANDARD_RIGHT) {
    // Bottom Right Corner.
    rad[4] = kBubbleCornerRadius;
    rad[5] = kBubbleCornerRadius;

    // Bottom Left Corner.
    rad[6] = kBubbleCornerRadius;
    rad[7] = kBubbleCornerRadius;
  }

  // Draw the bubble's shadow.
  int width = popup_bounds.width();
  int height = popup_bounds.height();
  gfx::Rect rect(gfx::Rect(popup_bounds.size()));
  SkPaint shadow_paint;
  shadow_paint.setAntiAlias(true);
  shadow_paint.setColor(kShadowColor);

  SkRRect rrect;
  rrect.setRectRadii(RectToSkRect(rect), (const SkVector*)rad);
  canvas->sk_canvas()->drawRRect(rrect, paint);

  // Draw the bubble.
  rect.SetRect(SkIntToScalar(kShadowThickness),
               SkIntToScalar(kShadowThickness),
               SkIntToScalar(width),
               SkIntToScalar(height));
  rrect.setRectRadii(RectToSkRect(rect), (const SkVector*)rad);
  canvas->sk_canvas()->drawRRect(rrect, paint);

  // Draw highlight text and then the text body. In order to make sure the text
  // is aligned to the right on RTL UIs, we mirror the text bounds if the
  // locale is RTL.
  const gfx::FontList font_list;
  int text_width = std::min(
      gfx::GetStringWidth(text_, font_list),
      width - (kShadowThickness * 2) - kTextPositionX - kTextHorizPadding);
  int text_height = height - (kShadowThickness * 2);
  gfx::Rect body_bounds(kShadowThickness + kTextPositionX,
                        kShadowThickness,
                        std::max(0, text_width),
                        std::max(0, text_height));
  body_bounds.set_x(GetMirroredXForRect(body_bounds));
  SkColor text_color =
      theme_service_->GetColor(ThemeProperties::COLOR_STATUS_BAR_TEXT);
  canvas->DrawStringRect(text_, font_list, text_color, body_bounds);
}


// StatusBubbleViews::StatusViewAnimation --------------------------------------

StatusBubbleViews::StatusViewAnimation::StatusViewAnimation(
    StatusView* status_view,
    double opacity_start,
    double opacity_end)
    : gfx::LinearAnimation(kFramerate, this),
      status_view_(status_view),
      opacity_start_(opacity_start),
      opacity_end_(opacity_end) {
}

StatusBubbleViews::StatusViewAnimation::~StatusViewAnimation() {
  // Remove ourself as a delegate so that we don't get notified when
  // animations end as a result of destruction.
  set_delegate(NULL);
}

double StatusBubbleViews::StatusViewAnimation::GetCurrentOpacity() {
  return opacity_start_ + (opacity_end_ - opacity_start_) *
      gfx::LinearAnimation::GetCurrentValue();
}

void StatusBubbleViews::StatusViewAnimation::AnimateToState(double state) {
  status_view_->SetOpacity(GetCurrentOpacity());
}

void StatusBubbleViews::StatusViewAnimation::AnimationEnded(
    const gfx::Animation* animation) {
  status_view_->SetOpacity(opacity_end_);
  status_view_->OnAnimationEnded();
}

// StatusBubbleViews::StatusViewExpander ---------------------------------------
//
// Manages the expansion and contraction of the status bubble as it accommodates
// URLs too long to fit in the standard bubble. Changes are passed through the
// StatusView to paint.
class StatusBubbleViews::StatusViewExpander : public gfx::LinearAnimation,
                                              public gfx::AnimationDelegate {
 public:
  StatusViewExpander(StatusBubbleViews* status_bubble,
                     StatusView* status_view)
      : gfx::LinearAnimation(kFramerate, this),
        status_bubble_(status_bubble),
        status_view_(status_view),
        expansion_start_(0),
        expansion_end_(0) {
  }

  // Manage the expansion of the bubble.
  void StartExpansion(const base::string16& expanded_text,
                      int current_width,
                      int expansion_end);

  // Set width of fully expanded bubble.
  void SetExpandedWidth(int expanded_width);

 private:
  // Animation functions.
  int GetCurrentBubbleWidth();
  void SetBubbleWidth(int width);
  virtual void AnimateToState(double state) OVERRIDE;
  virtual void AnimationEnded(const gfx::Animation* animation) OVERRIDE;

  // Manager that owns us.
  StatusBubbleViews* status_bubble_;

  // Change the bounds and text of this view.
  StatusView* status_view_;

  // Text elided (if needed) to fit maximum status bar width.
  base::string16 expanded_text_;

  // Widths at expansion start and end.
  int expansion_start_;
  int expansion_end_;
};

void StatusBubbleViews::StatusViewExpander::AnimateToState(double state) {
  SetBubbleWidth(GetCurrentBubbleWidth());
}

void StatusBubbleViews::StatusViewExpander::AnimationEnded(
    const gfx::Animation* animation) {
  SetBubbleWidth(expansion_end_);
  status_view_->SetText(expanded_text_, false);
}

void StatusBubbleViews::StatusViewExpander::StartExpansion(
    const base::string16& expanded_text,
    int expansion_start,
    int expansion_end) {
  expanded_text_ = expanded_text;
  expansion_start_ = expansion_start;
  expansion_end_ = expansion_end;
  int min_duration = std::max(kMinExpansionStepDurationMS,
      static_cast<int>(kMaxExpansionStepDurationMS *
          (expansion_end - expansion_start) / 100.0));
  SetDuration(std::min(kMaxExpansionStepDurationMS, min_duration));
  Start();
}

int StatusBubbleViews::StatusViewExpander::GetCurrentBubbleWidth() {
  return static_cast<int>(expansion_start_ +
      (expansion_end_ - expansion_start_) *
          gfx::LinearAnimation::GetCurrentValue());
}

void StatusBubbleViews::StatusViewExpander::SetBubbleWidth(int width) {
  status_bubble_->SetBubbleWidth(width);
  status_view_->SchedulePaint();
}


// StatusBubbleViews -----------------------------------------------------------

const int StatusBubbleViews::kShadowThickness = 1;

StatusBubbleViews::StatusBubbleViews(views::View* base_view)
    : contains_mouse_(false),
      offset_(0),
      base_view_(base_view),
      view_(NULL),
      download_shelf_is_visible_(false),
      is_expanded_(false),
      expand_timer_factory_(this) {
  expand_view_.reset();
}

StatusBubbleViews::~StatusBubbleViews() {
  CancelExpandTimer();
  if (popup_.get())
    popup_->CloseNow();
}

void StatusBubbleViews::Init() {
  if (!popup_.get()) {
    popup_.reset(new views::Widget);
    views::Widget* frame = base_view_->GetWidget();
    if (!view_)
      view_ = new StatusView(popup_.get(), frame->GetThemeProvider());
    if (!expand_view_.get())
      expand_view_.reset(new StatusViewExpander(this, view_));
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
    params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
    params.accept_events = false;
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.parent = frame->GetNativeView();
    params.context = frame->GetNativeWindow();
    popup_->Init(params);
    popup_->GetNativeView()->SetName("StatusBubbleViews");
    // We do our own animation and don't want any from the system.
    popup_->SetVisibilityChangedAnimationsEnabled(false);
    popup_->SetOpacity(0x00);
    popup_->SetContentsView(view_);
    ash::wm::GetWindowState(popup_->GetNativeWindow())->
        set_ignored_by_shelf(true);
    RepositionPopup();
  }
}

void StatusBubbleViews::Reposition() {
  // In restored mode, the client area has a client edge between it and the
  // frame.
  int overlap = kShadowThickness;
  int height = GetPreferredSize().height();
  int base_view_height = base_view()->bounds().height();
  gfx::Point origin(-overlap, base_view_height - height + overlap);
  SetBounds(origin.x(), origin.y(), base_view()->bounds().width() / 3, height);
}

void StatusBubbleViews::RepositionPopup() {
  if (popup_.get()) {
    gfx::Point top_left;
    views::View::ConvertPointToScreen(base_view_, &top_left);

    popup_->SetBounds(gfx::Rect(top_left.x() + position_.x(),
                                top_left.y() + position_.y(),
                                size_.width(), size_.height()));
  }
}

gfx::Size StatusBubbleViews::GetPreferredSize() {
  return gfx::Size(0, gfx::FontList().GetHeight() + kTotalVerticalPadding);
}

void StatusBubbleViews::SetBounds(int x, int y, int w, int h) {
  original_position_.SetPoint(x, y);
  position_.SetPoint(base_view_->GetMirroredXWithWidthInView(x, w), y);
  size_.SetSize(w, h);
  RepositionPopup();
  if (popup_.get() && contains_mouse_)
    AvoidMouse(last_mouse_moved_location_);
}

void StatusBubbleViews::SetStatus(const base::string16& status_text) {
  if (size_.IsEmpty())
    return;  // We have no bounds, don't attempt to show the popup.

  if (status_text_ == status_text && !status_text.empty())
    return;

  if (!IsFrameVisible())
    return;  // Don't show anything if the parent isn't visible.

  Init();
  status_text_ = status_text;
  if (!status_text_.empty()) {
    view_->SetText(status_text, true);
    view_->Show();
  } else if (!url_text_.empty()) {
    view_->SetText(url_text_, true);
  } else {
    view_->SetText(base::string16(), true);
  }
}

void StatusBubbleViews::SetURL(const GURL& url, const std::string& languages) {
  url_ = url;
  languages_ = languages;
  if (size_.IsEmpty())
    return;  // We have no bounds, don't attempt to show the popup.

  Init();

  // If we want to clear a displayed URL but there is a status still to
  // display, display that status instead.
  if (url.is_empty() && !status_text_.empty()) {
    url_text_ = base::string16();
    if (IsFrameVisible())
      view_->SetText(status_text_, true);
    return;
  }

  // Reset expansion state only when bubble is completely hidden.
  if (view_->state() == StatusView::BUBBLE_HIDDEN) {
    is_expanded_ = false;
    SetBubbleWidth(GetStandardStatusBubbleWidth());
  }

  // Set Elided Text corresponding to the GURL object.
  gfx::Rect popup_bounds = popup_->GetWindowBoundsInScreen();
  int text_width = static_cast<int>(popup_bounds.width() -
      (kShadowThickness * 2) - kTextPositionX - kTextHorizPadding - 1);
  url_text_ = ElideUrl(url, gfx::FontList(), text_width, languages);

  // An URL is always treated as a left-to-right string. On right-to-left UIs
  // we need to explicitly mark the URL as LTR to make sure it is displayed
  // correctly.
  url_text_ = base::i18n::GetDisplayStringInLTRDirectionality(url_text_);

  if (IsFrameVisible()) {
    view_->SetText(url_text_, true);

    CancelExpandTimer();

    // If bubble is already in expanded state, shift to adjust to new text
    // size (shrinking or expanding). Otherwise delay.
    if (is_expanded_ && !url.is_empty()) {
      ExpandBubble();
    } else if (net::FormatUrl(url, languages).length() > url_text_.length()) {
      base::MessageLoop::current()->PostDelayedTask(
          FROM_HERE,
          base::Bind(&StatusBubbleViews::ExpandBubble,
                     expand_timer_factory_.GetWeakPtr()),
          base::TimeDelta::FromMilliseconds(kExpandHoverDelayMS));
    }
  }
}

void StatusBubbleViews::Hide() {
  status_text_ = base::string16();
  url_text_ = base::string16();
  if (view_)
    view_->Hide();
}

void StatusBubbleViews::MouseMoved(const gfx::Point& location,
                                   bool left_content) {
  contains_mouse_ = !left_content;
  if (left_content) {
    RepositionPopup();
    return;
  }
  last_mouse_moved_location_ = location;

  if (view_) {
    view_->ResetTimer();

    if (view_->state() != StatusView::BUBBLE_HIDDEN &&
        view_->state() != StatusView::BUBBLE_HIDING_FADE &&
        view_->state() != StatusView::BUBBLE_HIDING_TIMER) {
      AvoidMouse(location);
    }
  }
}

void StatusBubbleViews::UpdateDownloadShelfVisibility(bool visible) {
  download_shelf_is_visible_ = visible;
}

void StatusBubbleViews::AvoidMouse(const gfx::Point& location) {
  // Get the position of the frame.
  gfx::Point top_left;
  views::View::ConvertPointToScreen(base_view_, &top_left);
  // Border included.
  int window_width = base_view_->GetLocalBounds().width();

  // Get the cursor position relative to the popup.
  gfx::Point relative_location = location;
  if (base::i18n::IsRTL()) {
    int top_right_x = top_left.x() + window_width;
    relative_location.set_x(top_right_x - relative_location.x());
  } else {
    relative_location.set_x(
        relative_location.x() - (top_left.x() + position_.x()));
  }
  relative_location.set_y(
      relative_location.y() - (top_left.y() + position_.y()));

  // If the mouse is in a position where we think it would move the
  // status bubble, figure out where and how the bubble should be moved.
  if (relative_location.y() > -kMousePadding &&
      relative_location.x() < size_.width() + kMousePadding) {
    int offset = kMousePadding + relative_location.y();

    // Make the movement non-linear.
    offset = offset * offset / kMousePadding;

    // When the mouse is entering from the right, we want the offset to be
    // scaled by how horizontally far away the cursor is from the bubble.
    if (relative_location.x() > size_.width()) {
      offset = static_cast<int>(static_cast<float>(offset) * (
          static_cast<float>(kMousePadding -
              (relative_location.x() - size_.width())) /
          static_cast<float>(kMousePadding)));
    }

    // Cap the offset and change the visual presentation of the bubble
    // depending on where it ends up (so that rounded corners square off
    // and mate to the edges of the tab content).
    if (offset >= size_.height() - kShadowThickness * 2) {
      offset = size_.height() - kShadowThickness * 2;
      view_->SetStyle(StatusView::STYLE_BOTTOM);
    } else if (offset > kBubbleCornerRadius / 2 - kShadowThickness) {
      view_->SetStyle(StatusView::STYLE_FLOATING);
    } else {
      view_->SetStyle(StatusView::STYLE_STANDARD);
    }

    // Check if the bubble sticks out from the monitor or will obscure
    // download shelf.
    gfx::NativeView window = base_view_->GetWidget()->GetNativeView();
    gfx::Rect monitor_rect = gfx::Screen::GetScreenFor(window)->
        GetDisplayNearestWindow(window).work_area();
    const int bubble_bottom_y = top_left.y() + position_.y() + size_.height();

    if (bubble_bottom_y + offset > monitor_rect.height() ||
        (download_shelf_is_visible_ &&
         (view_->style() == StatusView::STYLE_FLOATING ||
          view_->style() == StatusView::STYLE_BOTTOM))) {
      // The offset is still too large. Move the bubble to the right and reset
      // Y offset_ to zero.
      view_->SetStyle(StatusView::STYLE_STANDARD_RIGHT);
      offset_ = 0;

      // Subtract border width + bubble width.
      int right_position_x = window_width - (position_.x() + size_.width());
      popup_->SetBounds(gfx::Rect(top_left.x() + right_position_x,
                                  top_left.y() + position_.y(),
                                  size_.width(), size_.height()));
    } else {
      offset_ = offset;
      popup_->SetBounds(gfx::Rect(top_left.x() + position_.x(),
                                  top_left.y() + position_.y() + offset_,
                                  size_.width(), size_.height()));
    }
  } else if (offset_ != 0 ||
      view_->style() == StatusView::STYLE_STANDARD_RIGHT) {
    offset_ = 0;
    view_->SetStyle(StatusView::STYLE_STANDARD);
    popup_->SetBounds(gfx::Rect(top_left.x() + position_.x(),
                                top_left.y() + position_.y(),
                                size_.width(), size_.height()));
  }
}

bool StatusBubbleViews::IsFrameVisible() {
  views::Widget* frame = base_view_->GetWidget();
  if (!frame->IsVisible())
    return false;

  views::Widget* window = frame->GetTopLevelWidget();
  return !window || !window->IsMinimized();
}

bool StatusBubbleViews::IsFrameMaximized() {
  views::Widget* frame = base_view_->GetWidget();
  views::Widget* window = frame->GetTopLevelWidget();
  return window && window->IsMaximized();
}

void StatusBubbleViews::ExpandBubble() {
  // Elide URL to maximum possible size, then check actual length (it may
  // still be too long to fit) before expanding bubble.
  gfx::Rect popup_bounds = popup_->GetWindowBoundsInScreen();
  int max_status_bubble_width = GetMaxStatusBubbleWidth();
  const gfx::FontList font_list;
  url_text_ = ElideUrl(url_, font_list, max_status_bubble_width, languages_);
  int expanded_bubble_width =
      std::max(GetStandardStatusBubbleWidth(),
               std::min(gfx::GetStringWidth(url_text_, font_list) +
                            (kShadowThickness * 2) + kTextPositionX +
                            kTextHorizPadding + 1,
                        max_status_bubble_width));
  is_expanded_ = true;
  expand_view_->StartExpansion(url_text_, popup_bounds.width(),
                               expanded_bubble_width);
}

int StatusBubbleViews::GetStandardStatusBubbleWidth() {
  return base_view_->bounds().width() / 3;
}

int StatusBubbleViews::GetMaxStatusBubbleWidth() {
  const ui::NativeTheme* theme = base_view_->GetNativeTheme();
  return static_cast<int>(std::max(0, base_view_->bounds().width() -
      (kShadowThickness * 2) - kTextPositionX - kTextHorizPadding - 1 -
      views::NativeScrollBar::GetVerticalScrollBarWidth(theme)));
}

void StatusBubbleViews::SetBubbleWidth(int width) {
  size_.set_width(width);
  SetBounds(original_position_.x(), original_position_.y(),
            size_.width(), size_.height());
}

void StatusBubbleViews::CancelExpandTimer() {
  if (expand_timer_factory_.HasWeakPtrs())
    expand_timer_factory_.InvalidateWeakPtrs();
}
