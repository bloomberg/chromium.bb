// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/info_bubble.h"

#include "base/keyboard_codes.h"
#include "chrome/browser/window_sizer.h"
#include "chrome/common/notification_service.h"
#include "gfx/canvas.h"
#include "gfx/color_utils.h"
#include "gfx/path.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "views/fill_layout.h"
#include "views/widget/root_view.h"
#include "views/widget/widget.h"
#include "views/window/client_view.h"
#include "views/window/window.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/wm_ipc.h"
#include "third_party/cros/chromeos_wm_ipc_enums.h"
#endif

// Background color of the bubble.
#if defined(OS_WIN)
const SkColor InfoBubble::kBackgroundColor =
    color_utils::GetSysSkColor(COLOR_WINDOW);
#else
// TODO(beng): source from theme provider.
const SkColor InfoBubble::kBackgroundColor = SK_ColorWHITE;
#endif

void BorderContents::Init() {
  DCHECK(!bubble_border_);
  bubble_border_ = new BubbleBorder(BubbleBorder::LEFT_TOP);
  set_border(bubble_border_);
  bubble_border_->set_background_color(InfoBubble::kBackgroundColor);
}

void BorderContents::SizeAndGetBounds(
    const gfx::Rect& position_relative_to,
    BubbleBorder::ArrowLocation arrow_location,
    const gfx::Size& contents_size,
    gfx::Rect* contents_bounds,
    gfx::Rect* window_bounds) {
  if (UILayoutIsRightToLeft())
    arrow_location = BubbleBorder::rtl_mirror(arrow_location);
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

  // See if those bounds will fit on the monitor.
  scoped_ptr<WindowSizer::MonitorInfoProvider> monitor_provider(
      WindowSizer::CreateDefaultMonitorInfoProvider());
  gfx::Rect monitor_bounds(
      monitor_provider->GetMonitorWorkAreaMatching(position_relative_to));
  if (!monitor_bounds.IsEmpty() && !monitor_bounds.Contains(*window_bounds)) {
    // The bounds don't fit.  Move the arrow to try and improve things.
    if (window_bounds->bottom() > monitor_bounds.bottom())
      arrow_location = BubbleBorder::horizontal_mirror(arrow_location);
    else if (BubbleBorder::is_arrow_on_left(arrow_location) ?
             (window_bounds->right() > monitor_bounds.right()) :
             (window_bounds->x() < monitor_bounds.x())) {
      arrow_location = BubbleBorder::rtl_mirror(arrow_location);
    }

    bubble_border_->set_arrow_location(arrow_location);

    // Now get the recalculated bounds.
    *window_bounds = bubble_border_->GetBounds(position_relative_to,
                                              local_contents_size);
  }

  // Calculate the bounds of the contained contents (in window coordinates) by
  // subtracting the border dimensions and margin amounts.
  *contents_bounds = gfx::Rect(gfx::Point(), window_bounds->size());
  gfx::Insets insets;
  bubble_border_->GetInsets(&insets);
  contents_bounds->Inset(insets.left() + kLeftMargin, insets.top() + kTopMargin,
      insets.right() + kRightMargin, insets.bottom() + kBottomMargin);
}

void BorderContents::Paint(gfx::Canvas* canvas) {
  // The border of this view creates an anti-aliased round-rect region for the
  // contents, which we need to fill with the background color.
  // NOTE: This doesn't handle an arrow location of "NONE", which has square top
  // corners.
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(InfoBubble::kBackgroundColor);
  gfx::Path path;
  gfx::Rect bounds(GetLocalBounds(false));
  SkRect rect;
  rect.set(SkIntToScalar(bounds.x()), SkIntToScalar(bounds.y()),
           SkIntToScalar(bounds.right()), SkIntToScalar(bounds.bottom()));
  SkScalar radius = SkIntToScalar(BubbleBorder::GetCornerRadius());
  path.addRoundRect(rect, radius, radius);
  canvas->drawPath(path, paint);

  // Now we paint the border, so it will be alpha-blended atop the contents.
  // This looks slightly better in the corners than drawing the contents atop
  // the border.
  PaintBorder(canvas);
}

#if defined(OS_WIN)
// BorderWidget ---------------------------------------------------------------

BorderWidget::BorderWidget() : border_contents_(NULL) {
  set_delete_on_destroy(false);  // Our owner will free us manually.
  set_window_style(WS_POPUP);
  set_window_ex_style(WS_EX_TOOLWINDOW | WS_EX_LAYERED);
}


void BorderWidget::Init(HWND owner) {
  DCHECK(!border_contents_);
  border_contents_ = CreateBorderContents();
  border_contents_->Init();
  WidgetWin::Init(GetAncestor(owner, GA_ROOT), gfx::Rect());
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
                                     contents_size, &contents_bounds,
                                     &window_bounds);
  SetBounds(window_bounds);

  // Chop a hole out of our region to show the contents through.
  // CreateRectRgn() expects (left, top, right, bottom) in window coordinates.
  HRGN contents_region = CreateRectRgn(contents_bounds.x(), contents_bounds.y(),
      contents_bounds.right(), contents_bounds.bottom());
  HRGN window_region = CreateRectRgn(0, 0, window_bounds.width(),
                                     window_bounds.height());
  CombineRgn(window_region, window_region, contents_region, RGN_XOR);
  DeleteObject(contents_region);
  SetWindowRgn(window_region, true);

  // Return |contents_bounds| in screen coordinates.
  contents_bounds.Offset(window_bounds.origin());
  return contents_bounds;
}

BorderContents* BorderWidget::CreateBorderContents() {
  return new BorderContents();
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

void InfoBubble::Close() {
  Close(false);
}

InfoBubble::InfoBubble()
    :
#if defined(OS_LINUX)
      WidgetGtk(TYPE_WINDOW),
      border_contents_(NULL),
#endif
      delegate_(NULL),
      closed_(false) {
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
  set_window_ex_style(WS_EX_TOOLWINDOW);
  WidgetWin::Init(parent->GetNativeView(), gfx::Rect());
#elif defined(OS_LINUX)
  MakeTransparent();
  make_transient_to_parent();
  WidgetGtk::Init(
      GTK_WIDGET(static_cast<WidgetGtk*>(parent)->GetNativeView()),
      gfx::Rect());
#if defined(OS_CHROMEOS)
  chromeos::WmIpc::instance()->SetWindowType(
      GetNativeView(),
      chromeos::WM_IPC_WINDOW_CHROME_INFO_BUBBLE,
      NULL);
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
  DCHECK(!border_.get());
  border_.reset(CreateBorderWidget());
  border_->Init(GetNativeView());

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
  border_contents_ = new BorderContents;
  border_contents_->Init();
  gfx::Rect contents_bounds;
  border_contents_->SizeAndGetBounds(position_relative_to,
      arrow_location, contents->GetPreferredSize(),
      &contents_bounds, &window_bounds);
  // This new view must be added before |contents| so it will paint under it.
  contents_view->AddChildView(0, border_contents_);

  // |contents_view| has no layout manager, so we have to explicitly position
  // its children.
  border_contents_->SetBounds(gfx::Rect(gfx::Point(), window_bounds.size()));
  contents->SetBounds(contents_bounds);
#endif
  SetBounds(window_bounds);

  // Register the Escape accelerator for closing.
  GetFocusManager()->RegisterAccelerator(
      views::Accelerator(base::VKEY_ESCAPE, false, false, false), this);

  // Done creating the bubble.
  NotificationService::current()->Notify(NotificationType::INFO_BUBBLE_CREATED,
                                         Source<InfoBubble>(this),
                                         NotificationService::NoDetails());

  // Show the window.
#if defined(OS_WIN)
  border_->ShowWindow(SW_SHOW);
  ShowWindow(SW_SHOW);
#elif defined(OS_LINUX)
  views::WidgetGtk::Show();
#endif
}

#if defined(OS_WIN)
BorderWidget* InfoBubble::CreateBorderWidget() {
  return new BorderWidget;
}
#endif

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
      arrow_location_, contents_->GetPreferredSize(),
      &contents_bounds, &window_bounds);
  // |contents_view| has no layout manager, so we have to explicitly position
  // its children.
  border_contents_->SetBounds(gfx::Rect(gfx::Point(), window_bounds.size()));
  contents_->SetBounds(contents_bounds);
#endif
  SetBounds(window_bounds);
}

#if defined(OS_WIN)
void InfoBubble::OnActivate(UINT action, BOOL minimized, HWND window) {
  // The popup should close when it is deactivated.
  if (action == WA_INACTIVE && !closed_) {
    Close();
  } else if (action == WA_ACTIVE) {
    DCHECK(GetRootView()->GetChildViewCount() > 0);
    GetRootView()->GetChildViewAt(0)->RequestFocus();
  }
}
#elif defined(OS_LINUX)
void InfoBubble::IsActiveChanged() {
  if (!IsActive())
    Close();
}
#endif

void InfoBubble::Close(bool closed_by_escape) {
  if (closed_)
    return;

  if (delegate_)
    delegate_->InfoBubbleClosing(this, closed_by_escape);
  closed_ = true;
#if defined(OS_WIN)
  border_->Close();
  WidgetWin::Close();
#elif defined(OS_LINUX)
  WidgetGtk::Close();
#endif
}

bool InfoBubble::AcceleratorPressed(const views::Accelerator& accelerator) {
  if (!delegate_ || delegate_->CloseOnEscape()) {
    Close(true);
    return true;
  }
  return false;
}
