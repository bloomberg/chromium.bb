// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/info_bubble.h"

#include <vector>

#include "chrome/browser/ui/window_sizer.h"
#include "chrome/common/notification_service.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/path.h"
#include "views/layout/fill_layout.h"
#include "views/widget/root_view.h"
#include "views/widget/widget.h"
#include "views/window/client_view.h"
#include "views/window/window.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/wm_ipc.h"
#include "third_party/cros/chromeos_wm_ipc_enums.h"
#endif

using std::vector;

// How long the fade should last for.
static const int kHideFadeDurationMS = 200;

// Background color of the bubble.
#if defined(OS_WIN)
const SkColor InfoBubble::kBackgroundColor =
    color_utils::GetSysSkColor(COLOR_WINDOW);
#else
// TODO(beng): source from theme provider.
const SkColor InfoBubble::kBackgroundColor = SK_ColorWHITE;
#endif

void BorderContents::Init() {
  // Default arrow location.
  BubbleBorder::ArrowLocation arrow_location = BubbleBorder::TOP_LEFT;
  if (base::i18n::IsRTL())
    arrow_location = BubbleBorder::horizontal_mirror(arrow_location);
  DCHECK(!bubble_border_);
  bubble_border_ = new BubbleBorder(arrow_location);
  set_border(bubble_border_);
  bubble_border_->set_background_color(InfoBubble::kBackgroundColor);
}

void BorderContents::SizeAndGetBounds(
    const gfx::Rect& position_relative_to,
    BubbleBorder::ArrowLocation arrow_location,
    bool allow_bubble_offscreen,
    const gfx::Size& contents_size,
    gfx::Rect* contents_bounds,
    gfx::Rect* window_bounds) {
  if (base::i18n::IsRTL())
    arrow_location = BubbleBorder::horizontal_mirror(arrow_location);
  bubble_border_->set_arrow_location(arrow_location);
  // Set the border.
  set_border(bubble_border_);
  bubble_border_->set_background_color(InfoBubble::kBackgroundColor);

  // Give the contents a margin.
  gfx::Size local_contents_size(contents_size);
  local_contents_size.Enlarge(kLeftMargin + kRightMargin,
                              kTopMargin + kBottomMargin);

  // Try putting the arrow in its initial location, and calculating the bounds.
  *window_bounds =
      bubble_border_->GetBounds(position_relative_to, local_contents_size);
  if (!allow_bubble_offscreen) {
    gfx::Rect monitor_bounds = GetMonitorBounds(position_relative_to);
    if (!monitor_bounds.IsEmpty()) {
      // Try to resize vertically if this does not fit on the screen.
      MirrorArrowIfOffScreen(true,  // |vertical|.
                             position_relative_to, monitor_bounds,
                             local_contents_size, &arrow_location,
                             window_bounds);
      // Then try to resize horizontally if it still does not fit on the screen.
      MirrorArrowIfOffScreen(false,  // |vertical|.
                             position_relative_to, monitor_bounds,
                             local_contents_size, &arrow_location,
                             window_bounds);
    }
  }

  // Calculate the bounds of the contained contents (in window coordinates) by
  // subtracting the border dimensions and margin amounts.
  *contents_bounds = gfx::Rect(gfx::Point(), window_bounds->size());
  gfx::Insets insets;
  bubble_border_->GetInsets(&insets);
  contents_bounds->Inset(insets.left() + kLeftMargin, insets.top() + kTopMargin,
      insets.right() + kRightMargin, insets.bottom() + kBottomMargin);
}

gfx::Rect BorderContents::GetMonitorBounds(const gfx::Rect& rect) {
  scoped_ptr<WindowSizer::MonitorInfoProvider> monitor_provider(
      WindowSizer::CreateDefaultMonitorInfoProvider());
  return monitor_provider->GetMonitorWorkAreaMatching(rect);
}

void BorderContents::OnPaint(gfx::Canvas* canvas) {
  // The border of this view creates an anti-aliased round-rect region for the
  // contents, which we need to fill with the background color.
  // NOTE: This doesn't handle an arrow location of "NONE", which has square top
  // corners.
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(InfoBubble::kBackgroundColor);
  gfx::Path path;
  gfx::Rect bounds(GetContentsBounds());
  SkRect rect;
  rect.set(SkIntToScalar(bounds.x()), SkIntToScalar(bounds.y()),
           SkIntToScalar(bounds.right()), SkIntToScalar(bounds.bottom()));
  SkScalar radius = SkIntToScalar(BubbleBorder::GetCornerRadius());
  path.addRoundRect(rect, radius, radius);
  canvas->AsCanvasSkia()->drawPath(path, paint);

  // Now we paint the border, so it will be alpha-blended atop the contents.
  // This looks slightly better in the corners than drawing the contents atop
  // the border.
  OnPaintBorder(canvas);
}

void BorderContents::MirrorArrowIfOffScreen(
    bool vertical,
    const gfx::Rect& position_relative_to,
    const gfx::Rect& monitor_bounds,
    const gfx::Size& local_contents_size,
    BubbleBorder::ArrowLocation* arrow_location,
    gfx::Rect* window_bounds) {
  // If the bounds don't fit, move the arrow to its mirrored position to see if
  // it improves things.
  gfx::Insets offscreen_insets;
  if (ComputeOffScreenInsets(monitor_bounds, *window_bounds,
                             &offscreen_insets) &&
      GetInsetsLength(offscreen_insets, vertical) > 0) {
    BubbleBorder::ArrowLocation original_arrow_location = *arrow_location;
    *arrow_location =
        vertical ? BubbleBorder::vertical_mirror(*arrow_location) :
                   BubbleBorder::horizontal_mirror(*arrow_location);

    // Change the arrow and get the new bounds.
    bubble_border_->set_arrow_location(*arrow_location);
    *window_bounds = bubble_border_->GetBounds(position_relative_to,
                                               local_contents_size);
    gfx::Insets new_offscreen_insets;
    // If there is more of the window offscreen, we'll keep the old arrow.
    if (ComputeOffScreenInsets(monitor_bounds, *window_bounds,
                               &new_offscreen_insets) &&
        GetInsetsLength(new_offscreen_insets, vertical) >=
            GetInsetsLength(offscreen_insets, vertical)) {
      *arrow_location = original_arrow_location;
      bubble_border_->set_arrow_location(*arrow_location);
      *window_bounds = bubble_border_->GetBounds(position_relative_to,
                                                 local_contents_size);
    }
  }
}

// static
bool BorderContents::ComputeOffScreenInsets(const gfx::Rect& monitor_bounds,
                                            const gfx::Rect& window_bounds,
                                            gfx::Insets* offscreen_insets) {
  if (monitor_bounds.Contains(window_bounds))
    return false;

  if (!offscreen_insets)
    return true;

  int top = 0;
  int left = 0;
  int bottom = 0;
  int right = 0;

  if (window_bounds.y() < monitor_bounds.y())
    top = monitor_bounds.y() - window_bounds.y();
  if (window_bounds.x() < monitor_bounds.x())
    left = monitor_bounds.x() - window_bounds.x();
  if (window_bounds.bottom() > monitor_bounds.bottom())
    bottom = window_bounds.bottom() - monitor_bounds.bottom();
  if (window_bounds.right() > monitor_bounds.right())
    right = window_bounds.right() - monitor_bounds.right();

  offscreen_insets->Set(top, left, bottom, right);
  return true;
}

// static
int BorderContents::GetInsetsLength(const gfx::Insets& insets, bool vertical) {
  return vertical ? insets.height() : insets.width();
}

#if defined(OS_WIN)
// BorderWidget ---------------------------------------------------------------

BorderWidget::BorderWidget() : border_contents_(NULL) {
  set_window_style(WS_POPUP);
  set_window_ex_style(WS_EX_TOOLWINDOW | WS_EX_LAYERED);
}

void BorderWidget::Init(BorderContents* border_contents, HWND owner) {
  DCHECK(!border_contents_);
  border_contents_ = border_contents;
  border_contents_->Init();
  WidgetWin::Init(owner, gfx::Rect());
  SetContentsView(border_contents_);
  SetWindowPos(owner, 0, 0, 0, 0,
               SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOREDRAW);
}

gfx::Rect BorderWidget::SizeAndGetBounds(
    const gfx::Rect& position_relative_to,
    BubbleBorder::ArrowLocation arrow_location,
    const gfx::Size& contents_size) {
  // Ask the border view to calculate our bounds (and our contents').
  gfx::Rect contents_bounds;
  gfx::Rect window_bounds;
  border_contents_->SizeAndGetBounds(position_relative_to, arrow_location,
                                     false, contents_size, &contents_bounds,
                                     &window_bounds);
  SetBounds(window_bounds);
  border_contents_->SchedulePaint();

  // Return |contents_bounds| in screen coordinates.
  contents_bounds.Offset(window_bounds.origin());
  return contents_bounds;
}

LRESULT BorderWidget::OnMouseActivate(HWND window,
                                      UINT hit_test,
                                      UINT mouse_message) {
  // Never activate.
  return MA_NOACTIVATE;
}
#endif

// InfoBubble -----------------------------------------------------------------

// static
InfoBubble* InfoBubble::Show(views::Widget* parent,
                             const gfx::Rect& position_relative_to,
                             BubbleBorder::ArrowLocation arrow_location,
                             views::View* contents,
                             InfoBubbleDelegate* delegate) {
  InfoBubble* window = new InfoBubble;
  window->Init(parent, position_relative_to, arrow_location,
               contents, delegate);
  return window;
}

#if defined(OS_CHROMEOS)
// static
InfoBubble* InfoBubble::ShowFocusless(
    views::Widget* parent,
    const gfx::Rect& position_relative_to,
    BubbleBorder::ArrowLocation arrow_location,
    views::View* contents,
    InfoBubbleDelegate* delegate,
    bool show_while_screen_is_locked) {
  InfoBubble* window = new InfoBubble(views::WidgetGtk::TYPE_POPUP,
                                      show_while_screen_is_locked);
  window->Init(parent, position_relative_to, arrow_location,
               contents, delegate);
  return window;
}
#endif

void InfoBubble::Close() {
  if (show_status_ != kOpen)
    return;

  show_status_ = kClosing;

  if (fade_away_on_close_)
    FadeOut();
  else
    DoClose(false);
}

void InfoBubble::AnimationEnded(const ui::Animation* animation) {
  if (static_cast<int>(animation_->GetCurrentValue()) == 0) {
    // When fading out we just need to close the bubble at the end
    DoClose(false);
  } else {
#if defined(OS_WIN)
    // When fading in we need to remove the layered window style flag, since
    // that style prevents some bubble content from working properly.
    SetWindowLong(GWL_EXSTYLE, GetWindowLong(GWL_EXSTYLE) & ~WS_EX_LAYERED);
#endif
  }
}

void InfoBubble::AnimationProgressed(const ui::Animation* animation) {
#if defined(OS_WIN)
  // Set the opacity for the main contents window.
  unsigned char opacity = static_cast<unsigned char>(
      animation_->GetCurrentValue() * 255);
  SetLayeredWindowAttributes(GetNativeView(), 0,
      static_cast<byte>(opacity), LWA_ALPHA);
  contents_->SchedulePaint();

  // Also fade in/out the bubble border window.
  border_->SetOpacity(opacity);
  border_->border_contents()->SchedulePaint();
#else
  NOTIMPLEMENTED();
#endif
}

InfoBubble::InfoBubble()
    :
#if defined(OS_LINUX)
      WidgetGtk(TYPE_WINDOW),
      border_contents_(NULL),
#elif defined(OS_WIN)
      border_(NULL),
#endif
      delegate_(NULL),
      show_status_(kOpen),
      fade_away_on_close_(false),
#if defined(OS_CHROMEOS)
      show_while_screen_is_locked_(false),
#endif
      arrow_location_(BubbleBorder::NONE),
      contents_(NULL) {
}

#if defined(OS_CHROMEOS)
InfoBubble::InfoBubble(views::WidgetGtk::Type type,
                       bool show_while_screen_is_locked)
    : WidgetGtk(type),
      border_contents_(NULL),
      delegate_(NULL),
      show_status_(kOpen),
      fade_away_on_close_(false),
      show_while_screen_is_locked_(show_while_screen_is_locked),
      arrow_location_(BubbleBorder::NONE),
      contents_(NULL) {
}
#endif

InfoBubble::~InfoBubble() {
}

void InfoBubble::Init(views::Widget* parent,
                      const gfx::Rect& position_relative_to,
                      BubbleBorder::ArrowLocation arrow_location,
                      views::View* contents,
                      InfoBubbleDelegate* delegate) {
  delegate_ = delegate;
  position_relative_to_ = position_relative_to;
  arrow_location_ = arrow_location;
  contents_ = contents;

  // Create the main window.
#if defined(OS_WIN)
  views::Window* parent_window = parent->GetWindow();
  if (parent_window)
    parent_window->DisableInactiveRendering();
  set_window_style(WS_POPUP | WS_CLIPCHILDREN);
  int extended_style = WS_EX_TOOLWINDOW;
  // During FadeIn we need to turn on the layered window style to deal with
  // transparency. This flag needs to be reset after fading in is complete.
  bool fade_in = delegate_ && delegate_->FadeInOnShow();
  if (fade_in)
    extended_style |= WS_EX_LAYERED;
  set_window_ex_style(extended_style);

  DCHECK(!border_);
  border_ = new BorderWidget();

  if (fade_in) {
    border_->SetOpacity(0);
    SetOpacity(0);
  }

  border_->Init(CreateBorderContents(), parent->GetNativeView());

  // We make the BorderWidget the owner of the InfoBubble HWND, so that the
  // latter is displayed on top of the former.
  WidgetWin::Init(border_->GetNativeView(), gfx::Rect());

  SetWindowText(GetNativeView(), delegate_->accessible_name().c_str());
#elif defined(OS_LINUX)
  MakeTransparent();
  make_transient_to_parent();
  WidgetGtk::InitWithWidget(parent, gfx::Rect());
#if defined(OS_CHROMEOS)
  {
    vector<int> params;
    params.push_back(show_while_screen_is_locked_ ? 1 : 0);
    chromeos::WmIpc::instance()->SetWindowType(
        GetNativeView(),
        chromeos::WM_IPC_WINDOW_CHROME_INFO_BUBBLE,
        &params);
  }
#endif
#endif

  // Create a View to hold the contents of the main window.
  views::View* contents_view = new views::View;
  // We add |contents_view| to ourselves before the AddChildView() call below so
  // that when |contents| gets added, it will already have a widget, and thus
  // any NativeButtons it creates in ViewHierarchyChanged() will be functional
  // (e.g. calling SetChecked() on checkboxes is safe).
  SetContentsView(contents_view);
  // Adding |contents| as a child has to be done before we call
  // contents->GetPreferredSize() below, since some supplied views don't
  // actually initialize themselves until they're added to a hierarchy.
  contents_view->AddChildView(contents);

  // Calculate and set the bounds for all windows and views.
  gfx::Rect window_bounds;

#if defined(OS_WIN)
  // Initialize and position the border window.
  window_bounds = border_->SizeAndGetBounds(position_relative_to,
                                            arrow_location,
                                            contents->GetPreferredSize());

  // Make |contents| take up the entire contents view.
  contents_view->SetLayoutManager(new views::FillLayout);

  // Paint the background color behind the contents.
  contents_view->set_background(
      views::Background::CreateSolidBackground(kBackgroundColor));
#else
  // Create a view to paint the border and background.
  border_contents_ = CreateBorderContents();
  border_contents_->Init();
  gfx::Rect contents_bounds;
  border_contents_->SizeAndGetBounds(position_relative_to,
      arrow_location, false, contents->GetPreferredSize(),
      &contents_bounds, &window_bounds);
  // This new view must be added before |contents| so it will paint under it.
  contents_view->AddChildViewAt(border_contents_, 0);

  // |contents_view| has no layout manager, so we have to explicitly position
  // its children.
  border_contents_->SetBoundsRect(
      gfx::Rect(gfx::Point(), window_bounds.size()));
  contents->SetBoundsRect(contents_bounds);
#endif
  SetBounds(window_bounds);

  // Register the Escape accelerator for closing.
  GetFocusManager()->RegisterAccelerator(
      views::Accelerator(ui::VKEY_ESCAPE, false, false, false), this);

  // Done creating the bubble.
  NotificationService::current()->Notify(NotificationType::INFO_BUBBLE_CREATED,
                                         Source<InfoBubble>(this),
                                         NotificationService::NoDetails());

  // Show the window.
#if defined(OS_WIN)
  border_->ShowWindow(SW_SHOW);
  ShowWindow(SW_SHOW);
  if (fade_in)
    FadeIn();
#elif defined(OS_LINUX)
  views::WidgetGtk::Show();
#endif
}

BorderContents* InfoBubble::CreateBorderContents() {
  return new BorderContents();
}

void InfoBubble::SizeToContents() {
  gfx::Rect window_bounds;

#if defined(OS_WIN)
  // Initialize and position the border window.
  window_bounds = border_->SizeAndGetBounds(position_relative_to_,
                                            arrow_location_,
                                            contents_->GetPreferredSize());
#else
  gfx::Rect contents_bounds;
  border_contents_->SizeAndGetBounds(position_relative_to_,
      arrow_location_, false, contents_->GetPreferredSize(),
      &contents_bounds, &window_bounds);
  // |contents_view| has no layout manager, so we have to explicitly position
  // its children.
  border_contents_->SetBoundsRect(
      gfx::Rect(gfx::Point(), window_bounds.size()));
  contents_->SetBoundsRect(contents_bounds);
#endif
  SetBounds(window_bounds);
}

#if defined(OS_WIN)
void InfoBubble::OnActivate(UINT action, BOOL minimized, HWND window) {
  // The popup should close when it is deactivated.
  if (action == WA_INACTIVE) {
    Close();
  } else if (action == WA_ACTIVE) {
    DCHECK(GetRootView()->has_children());
    GetRootView()->GetChildViewAt(0)->RequestFocus();
  }
}
#elif defined(OS_LINUX)
void InfoBubble::IsActiveChanged() {
  if (!IsActive())
    Close();
}
#endif

void InfoBubble::DoClose(bool closed_by_escape) {
  if (show_status_ == kClosed)
    return;

  GetFocusManager()->UnregisterAccelerator(
      views::Accelerator(ui::VKEY_ESCAPE, false, false, false), this);
  if (delegate_)
    delegate_->InfoBubbleClosing(this, closed_by_escape);
  show_status_ = kClosed;
#if defined(OS_WIN)
  border_->Close();
  WidgetWin::Close();
#elif defined(OS_LINUX)
  WidgetGtk::Close();
#endif
}

void InfoBubble::FadeIn() {
  Fade(true);  // |fade_in|.
}

void InfoBubble::FadeOut() {
#if defined(OS_WIN)
  // The contents window cannot have the layered flag on by default, since its
  // content doesn't always work inside a layered window, but when animating it
  // is ok to set that style on the window for the purpose of fading it out.
  SetWindowLong(GWL_EXSTYLE, GetWindowLong(GWL_EXSTYLE) | WS_EX_LAYERED);
  // This must be the very next call, otherwise we can get flicker on close.
  SetLayeredWindowAttributes(GetNativeView(), 0,
      static_cast<byte>(255), LWA_ALPHA);
#endif

  Fade(false);  // |fade_in|.
}

void InfoBubble::Fade(bool fade_in) {
  animation_.reset(new ui::SlideAnimation(this));
  animation_->SetSlideDuration(kHideFadeDurationMS);
  animation_->SetTweenType(ui::Tween::LINEAR);

  animation_->Reset(fade_in ? 0.0 : 1.0);
  if (fade_in)
    animation_->Show();
  else
    animation_->Hide();
}

bool InfoBubble::AcceleratorPressed(const views::Accelerator& accelerator) {
  if (!delegate_ || delegate_->CloseOnEscape()) {
    DoClose(true);
    return true;
  }
  return false;
}
