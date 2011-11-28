// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_aura.h"

#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "grit/generated_resources.h"  // Accessibility names
#include "grit/ui_resources.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/aura/cursor.h"
#include "ui/base/animation/throb_animation.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace {
// Our window is larger than it appears, as it includes space around the edges
// where resize handles can appear.
const int kResizeBorderThickness = 8;
// The top edge is a little thinner, as it is not draggable for resize.
const int kTopBorderThickness = 4;
// Offset between top of non-client frame and top edge of opaque frame
// background at start of slide-in animation.
const int kFrameBackgroundTopOffset = 25;

// Width of a persistent border that we show around the window (using
// FrameBackground) even when the resize border isn't visible.
const int kPersistentBorderThickness = 2;

// The color used to fill the frame.  Opacity is handled in the layer.
const SkColor kFrameColor = SK_ColorBLACK;
// Radius of rounded rectangle corners.
const int kRoundedRectRadius = 3;
// Frame border fades in over this range of opacity.
const double kFrameBorderStartOpacity = 0.2;
const double kFrameBorderEndOpacity = 0.3;
// How long the hover animation takes if uninterrupted.
const int kHoverFadeDurationMs = 250;

// Color shown when window control is hovered.
const SkColor kMaximizeButtonBackgroundColor = SkColorSetRGB(0, 255, 0);
const SkColor kCloseButtonBackgroundColor = SkColorSetRGB(255, 0, 0);

bool HitVisibleView(views::View* view, gfx::Point point) {
  return view->IsVisible() && view->GetMirroredBounds().Contains(point);
}

}  // namespace

// Buttons for window controls - close, zoom, etc.
// Note that views::CustomButton is already a ui::AnimationDelegate.
class WindowControlButton : public views::CustomButton {
 public:
  WindowControlButton(BrowserNonClientFrameViewAura* owner,
                      SkColor color,
                      const SkBitmap& icon)
      : views::CustomButton(owner),
        owner_(owner),
        color_(color),
        icon_(icon),
        ALLOW_THIS_IN_INITIALIZER_LIST(
            show_animation_(new ui::SlideAnimation(this))) {
    show_animation_->SetSlideDuration(kHoverFadeDurationMs);
    SetPaintToLayer(true);
    layer()->SetFillsBoundsOpaquely(false);
    layer()->SetOpacity(0.f);
  }
  virtual ~WindowControlButton() {}

  void Show() {
    show_animation_->Show();
  }
  void Hide() {
    show_animation_->Hide();
  }

  // Overridden from views::View:
  virtual void OnMouseEntered(const views::MouseEvent& event) OVERRIDE {
    // Ensure the caption/frame background shows when we hover this button.
    owner_->ShowFrameBackground();
    views::CustomButton::OnMouseEntered(event);
  }
  virtual void OnMouseExited(const views::MouseEvent& event) OVERRIDE {
    owner_->HideFrameBackground();
    views::CustomButton::OnMouseExited(event);
  }
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    canvas->FillRect(GetBackgroundColor(), GetLocalBounds());
    canvas->DrawBitmapInt(icon_, 0, 0);
  }
  virtual gfx::Size GetPreferredSize() OVERRIDE {
    return gfx::Size(icon_.width(), icon_.height());
  }

  // Overridden from ui::AnimationDelegate:
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE {
    if (animation == show_animation_.get()) {
      double opacity = show_animation_->GetCurrentValue();
      layer()->SetOpacity(static_cast<float>(opacity));
      return;
    }
    views::CustomButton::AnimationProgressed(animation);
  }

 private:
  SkColor GetBackgroundColor() {
    // Background animates in separately, so handle opacity manually.
    return SkColorSetARGB(hover_animation_->CurrentValueBetween(0, 150),
                          SkColorGetR(color_),
                          SkColorGetG(color_),
                          SkColorGetB(color_));
  }

  BrowserNonClientFrameViewAura* owner_;
  SkColor color_;
  SkBitmap icon_;
  scoped_ptr<ui::SlideAnimation> show_animation_;

  DISALLOW_COPY_AND_ASSIGN(WindowControlButton);
};

// Layer that visually sits "behind" the window contents and expands out to
// provide visual resize handles on the sides.  Hit testing and resize handling
// is in the parent NonClientFrameView.
class FrameBackground : public views::View,
                        public ui::AnimationDelegate {
 public:
  FrameBackground()
      : ALLOW_THIS_IN_INITIALIZER_LIST(
            size_animation_(new ui::SlideAnimation(this))),
        ALLOW_THIS_IN_INITIALIZER_LIST(
            color_animation_(new ui::SlideAnimation(this))) {
    size_animation_->SetSlideDuration(kHoverFadeDurationMs);
    color_animation_->SetSlideDuration(kHoverFadeDurationMs);
    SetPaintToLayer(true);
    UpdateOpacity();
  }
  virtual ~FrameBackground() {
  }

  void Configure(const gfx::Rect& start_bounds, const gfx::Rect& end_bounds) {
    start_bounds_ = start_bounds;
    end_bounds_ = end_bounds;
    UpdateBounds();
  }
  void SetEndBounds(const gfx::Rect& end_bounds) {
    end_bounds_ = end_bounds;
    UpdateBounds();
  }
  void Show() {
    size_animation_->Show();
    color_animation_->Show();
  }
  void Hide() {
    size_animation_->Hide();
    color_animation_->Hide();
  }

 protected:
  // Overridden from views::View:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    SkRect rect = { SkIntToScalar(0), SkIntToScalar(0),
                    SkIntToScalar(width()), SkIntToScalar(height()) };
    SkScalar radius = SkIntToScalar(kRoundedRectRadius);
    SkPaint paint;
    // Animation handles setting the opacity for the whole layer.
    paint.setColor(kFrameColor);
    paint.setStyle(SkPaint::kFill_Style);
    canvas->GetSkCanvas()->drawRoundRect(rect, radius, radius, paint);
  }

  // Overridden from ui::AnimationDelegate:
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE {
    if (animation == color_animation_.get()) {
      UpdateOpacity();
    } else if (animation == size_animation_.get()) {
      UpdateBounds();
    }
  }

 private:
  void UpdateOpacity() {
    double opacity = color_animation_->CurrentValueBetween(
        kFrameBorderStartOpacity, kFrameBorderEndOpacity);
    layer()->SetOpacity(static_cast<float>(opacity));
  }

  void UpdateBounds() {
    gfx::Rect current_bounds =
        size_animation_->CurrentValueBetween(start_bounds_, end_bounds_);
    SetBoundsRect(current_bounds);
    SchedulePaint();
  }

  scoped_ptr<ui::SlideAnimation> size_animation_;
  scoped_ptr<ui::SlideAnimation> color_animation_;
  // Default "hidden" rectangle.
  gfx::Rect default_bounds_;
  // When moving mouse from one target to another (e.g. from edge to corner)
  // the size animation start point may not be the default size.
  gfx::Rect start_bounds_;
  // Expanded bounds, with edges visible from behind the client area.
  gfx::Rect end_bounds_;

  DISALLOW_COPY_AND_ASSIGN(FrameBackground);
};

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewAura, public:

BrowserNonClientFrameViewAura::BrowserNonClientFrameViewAura(
    BrowserFrame* frame, BrowserView* browser_view)
    : BrowserNonClientFrameView(frame, browser_view),
      last_hittest_code_(HTNOWHERE) {
  frame_background_ = new FrameBackground();
  AddChildView(frame_background_);

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  maximize_button_ =
      new WindowControlButton(this,
                              kMaximizeButtonBackgroundColor,
                              *rb.GetBitmapNamed(IDR_AURA_WINDOW_ZOOM_ICON));
  maximize_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_MAXIMIZE));
  AddChildView(maximize_button_);

  close_button_ =
      new WindowControlButton(this,
                              kCloseButtonBackgroundColor,
                              *rb.GetBitmapNamed(IDR_AURA_WINDOW_CLOSE_ICON));
  close_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_CLOSE));
  AddChildView(close_button_);

  // The BrowserFrame will become our owning window/widget.
  frame->AsWidget()->AddObserver(this);
  // Associate our WindowFrame interface with our owning window/widget so
  // we get callbacks from aura_shell.
  frame->AsWidget()->SetNativeWindowProperty(
      aura_shell::kWindowFrameKey,
      static_cast<aura_shell::WindowFrame*>(this));
}

BrowserNonClientFrameViewAura::~BrowserNonClientFrameViewAura() {
  // Don't need to remove the Widget observer, the window is deleted before us.
}

void BrowserNonClientFrameViewAura::ShowFrameBackground() {
  UpdateFrameBackground(ShouldPaintAsActive());
  frame_background_->Show();
}

void BrowserNonClientFrameViewAura::HideFrameBackground() {
  frame_background_->Hide();
}

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewAura, private:

int BrowserNonClientFrameViewAura::NonClientHitTestImpl(
    const gfx::Point& point) {
  if (!GetLocalBounds().Contains(point))
    return HTNOWHERE;

  // Window controls get first try because they overlap the client area.
  if (HitVisibleView(maximize_button_, point))
    return HTMAXBUTTON;
  if (HitVisibleView(close_button_, point))
    return HTCLOSE;

  int frame_component = GetWidget()->client_view()->NonClientHitTest(point);
  if (frame_component != HTNOWHERE)
    return frame_component;

  // Test window resize components.
  bool can_resize = GetWidget()->widget_delegate()->CanResize();
  // TODO(derat): Disallow resizing via the top border in the Aura shell
  // instead of enforcing it here.  See http://crbug.com/101830.
  frame_component = GetHTComponentForFrame(point,
                                           0,
                                           kResizeBorderThickness,
                                           kResizeBorderThickness,
                                           kResizeBorderThickness,
                                           can_resize);
  if (frame_component != HTNOWHERE)
    return frame_component;
  // Use HTCAPTION as a final fallback.
  return HTCAPTION;
}

// Pass |active_window| explicitly because deactivating a window causes
// OnWidgetActivationChanged() to be called before GetWidget()->IsActive()
// changes state.
gfx::Rect BrowserNonClientFrameViewAura::GetFrameBackgroundBounds(
    int hittest_code,
    bool active_window) {
  bool show_left = false;
  bool show_top = false;
  bool show_right = false;
  bool show_bottom = false;
  switch (hittest_code) {
    case HTBOTTOM:
      show_bottom = true;
      break;
    case HTBOTTOMLEFT:
      show_bottom = true;
      show_left = true;
      break;
    case HTBOTTOMRIGHT:
      show_bottom = true;
      show_right = true;
      break;
    case HTCAPTION:
      show_top = true;
      break;
    case HTCLOSE:
      show_top = true;
      break;
    case HTLEFT:
      show_left = true;
      break;
    case HTMAXBUTTON:
      show_top = true;
      break;
    case HTRIGHT:
      show_right = true;
      break;
    case HTTOP:
      show_top = true;
      break;
    case HTTOPLEFT:
      show_top = true;
      show_left = true;
      break;
    case HTTOPRIGHT:
      show_top = true;
      show_right = true;
      break;
    default:
      break;
  }
  // Always show top edge for the active window so that you can tell which
  // window has focus.
  if (active_window)
    show_top = true;
  gfx::Rect target = bounds();
  // Inset the sides that are not showing.
  target.Inset(
      (show_left ? 0 : kResizeBorderThickness - kPersistentBorderThickness),
      (show_top ? 0 : kTopBorderThickness + kFrameBackgroundTopOffset),
      (show_right ? 0 : kResizeBorderThickness - kPersistentBorderThickness),
      (show_bottom ? 0 : kResizeBorderThickness - kPersistentBorderThickness));
  return target;
}

void BrowserNonClientFrameViewAura::UpdateFrameBackground(bool active_window) {
  gfx::Rect start_bounds = GetFrameBackgroundBounds(HTNOWHERE, active_window);
  gfx::Rect end_bounds =
      GetFrameBackgroundBounds(last_hittest_code_, active_window);
  frame_background_->Configure(start_bounds, end_bounds);
}

void BrowserNonClientFrameViewAura::ActiveStateChanged() {
  bool active = ShouldPaintAsActive();
  // Active windows have different background bounds.
  UpdateFrameBackground(active);
  if (active)
    frame_background_->Show();
  else
    frame_background_->Hide();
}

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameView overrides:

gfx::Rect BrowserNonClientFrameViewAura::GetBoundsForTabStrip(
    views::View* tabstrip) const {
  if (!tabstrip)
    return gfx::Rect();
  // TODO(jamescook): Avatar icon support.
  // Reserve space on the right for close/maximize buttons.
  int tabstrip_x = kResizeBorderThickness;
  int tabstrip_width = maximize_button_->x() - tabstrip_x;
  return gfx::Rect(tabstrip_x,
                   GetHorizontalTabStripVerticalOffset(false),
                   tabstrip_width,
                   tabstrip->GetPreferredSize().height());

}

int BrowserNonClientFrameViewAura::GetHorizontalTabStripVerticalOffset(
    bool restored) const {
  return kTopBorderThickness;
}

void BrowserNonClientFrameViewAura::UpdateThrobber(bool running) {
  // TODO(jamescook): Do we need this?
}

///////////////////////////////////////////////////////////////////////////////
// views::NonClientFrameView overrides:

gfx::Rect BrowserNonClientFrameViewAura::GetBoundsForClientView() const {
  gfx::Rect bounds = GetLocalBounds();
  bounds.Inset(kResizeBorderThickness,
               kTopBorderThickness,
               kResizeBorderThickness,
               kResizeBorderThickness);
  return bounds;
}

gfx::Rect BrowserNonClientFrameViewAura::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  gfx::Rect bounds = client_bounds;
  bounds.Inset(-kResizeBorderThickness,
               -kTopBorderThickness,
               -kResizeBorderThickness,
               -kResizeBorderThickness);
  return bounds;
}

int BrowserNonClientFrameViewAura::NonClientHitTest(const gfx::Point& point) {
  last_hittest_code_ = NonClientHitTestImpl(point);
  return last_hittest_code_;
}

void BrowserNonClientFrameViewAura::GetWindowMask(const gfx::Size& size,
                                                  gfx::Path* window_mask) {
  // Nothing.
}

void BrowserNonClientFrameViewAura::ResetWindowControls() {
  maximize_button_->SetState(views::CustomButton::BS_NORMAL);
  // The close button isn't affected by this constraint.
}

void BrowserNonClientFrameViewAura::UpdateWindowIcon() {
  // TODO(jamescook): We will need this for app frames.
}

void BrowserNonClientFrameViewAura::ShouldPaintAsActiveChanged() {
  ActiveStateChanged();
}

///////////////////////////////////////////////////////////////////////////////
// views::View overrides:

void BrowserNonClientFrameViewAura::Layout() {
  // Layout window buttons from right to left.
  int right = width() - kResizeBorderThickness;
  gfx::Size preferred = close_button_->GetPreferredSize();
  close_button_->SetBounds(right - preferred.width(), kTopBorderThickness,
                           preferred.width(), preferred.height());
  right -= preferred.width();  // No padding.
  preferred = maximize_button_->GetPreferredSize();
  maximize_button_->SetBounds(right - preferred.width(), kTopBorderThickness,
                              preferred.width(), preferred.height());
  UpdateFrameBackground(ShouldPaintAsActive());
}

views::View* BrowserNonClientFrameViewAura::GetEventHandlerForPoint(
    const gfx::Point& point) {
  // Mouse hovers near the resizing edges result in the animation starting and
  // stopping as the frame background changes size.  Just ignore events
  // destined for the frame background and handle them at this level.
  views::View* view = View::GetEventHandlerForPoint(point);
  if (view == frame_background_)
    return this;
  return view;
}

bool BrowserNonClientFrameViewAura::HitTest(const gfx::Point& p) const {
  // Claim all events outside the client area.
  bool in_client = GetWidget()->client_view()->bounds().Contains(p);
  if (!in_client)
    return true;
  // Window controls overlap the client area, so explicitly check for points
  // inside of them.
  if (close_button_->bounds().Contains(p) ||
      maximize_button_->bounds().Contains(p))
    return true;
  // Otherwise claim it only if it's in a non-tab portion of the tabstrip.
  if (!browser_view()->tabstrip())
    return false;
  gfx::Rect tabstrip_bounds(browser_view()->tabstrip()->bounds());
  gfx::Point tabstrip_origin(tabstrip_bounds.origin());
  View::ConvertPointToView(
      frame()->client_view(), this, &tabstrip_origin);
  tabstrip_bounds.set_origin(tabstrip_origin);
  if (p.y() > tabstrip_bounds.bottom())
    return false;

  // We convert from our parent's coordinates since we assume we fill its bounds
  // completely. We need to do this since we're not a parent of the tabstrip,
  // meaning ConvertPointToView would otherwise return something bogus.
  gfx::Point browser_view_point(p);
  View::ConvertPointToView(parent(), browser_view(), &browser_view_point);
  return browser_view()->IsPositionInWindowCaption(browser_view_point);
}

void BrowserNonClientFrameViewAura::OnMouseMoved(
    const views::MouseEvent& event) {
  // We may be hovering over the resize edge.
  ShowFrameBackground();
}

void BrowserNonClientFrameViewAura::OnMouseExited(
    const views::MouseEvent& event) {
  // We hovered away from the resize edge.
  HideFrameBackground();
}

gfx::NativeCursor BrowserNonClientFrameViewAura::GetCursor(
    const views::MouseEvent& event) {
  switch (last_hittest_code_) {
    case HTBOTTOM:
      return aura::kCursorSouthResize;
    case HTBOTTOMLEFT:
      return aura::kCursorSouthWestResize;
    case HTBOTTOMRIGHT:
      return aura::kCursorSouthEastResize;
    case HTLEFT:
      return aura::kCursorWestResize;
    case HTRIGHT:
      return aura::kCursorEastResize;
    case HTTOP:
      // Resizing from the top edge is not allowed.
      return aura::kCursorNull;
    case HTTOPLEFT:
      return aura::kCursorWestResize;
    case HTTOPRIGHT:
      return aura::kCursorEastResize;
    default:
      return aura::kCursorNull;
  }
}

///////////////////////////////////////////////////////////////////////////////
// views::ButtonListener overrides:

void BrowserNonClientFrameViewAura::ButtonPressed(views::Button* sender,
                                                  const views::Event& event) {
  if (sender == close_button_) {
    frame()->Close();
  } else if (sender == maximize_button_) {
    if (frame()->IsMaximized())
      frame()->Restore();
    else
      frame()->Maximize();
  }
}

///////////////////////////////////////////////////////////////////////////////
// views::Widget::Observer overrides:

void BrowserNonClientFrameViewAura::OnWidgetActivationChanged(
    views::Widget* widget,
    bool active) {
  ActiveStateChanged();
}

///////////////////////////////////////////////////////////////////////////////
// aura_shell::WindowFrame overrides:

void BrowserNonClientFrameViewAura::OnWindowHoverChanged(bool hovered) {
  if (hovered) {
    maximize_button_->Show();
    close_button_->Show();
  } else {
    maximize_button_->Hide();
    close_button_->Hide();
  }
}
