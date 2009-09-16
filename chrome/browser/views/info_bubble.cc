// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/info_bubble.h"

#include "app/gfx/canvas.h"
#include "app/gfx/color_utils.h"
#include "app/gfx/path.h"
#include "chrome/browser/window_sizer.h"
#include "chrome/common/notification_service.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "views/fill_layout.h"
#include "views/widget/root_view.h"
#include "views/window/window.h"

namespace {

// Background color of the bubble.
#if defined(OS_WIN)
const SkColor kBackgroundColor = color_utils::GetSysSkColor(COLOR_WINDOW);
#else
// TODO(beng): source from theme provider.
const SkColor kBackgroundColor = SK_ColorWHITE;
#endif

}

#if defined(OS_WIN)
// BorderContents -------------------------------------------------------------

// This is used to paint the border; see comments on BorderWidget below.
class BorderContents : public views::View {
 public:
  BorderContents() { }

  // Given the size of the contents and the rect (in screen coordinates) to
  // point at, initializes the bubble and returns the bounds (in screen
  // coordinates) of both the border and the contents inside the bubble.
  // |is_rtl| is true if the UI is RTL and thus the arrow should default to the
  // right side of the bubble; otherwise it defaults to the left top corner, and
  // then is moved as necessary to try and fit the whole bubble on the same
  // monitor as the rect being pointed to.
  //
  // TODO(pkasting): Maybe this should use mirroring transformations instead,
  // which would hopefully simplify this code.
  void InitAndGetBounds(const gfx::Rect& position_relative_to,
                        const gfx::Size& contents_size,
                        bool is_rtl,
                        gfx::Rect* inner_bounds,
                        gfx::Rect* outer_bounds);

 private:
  virtual ~BorderContents() { }

  // Overridden from View:
  virtual void Paint(gfx::Canvas* canvas);

  DISALLOW_COPY_AND_ASSIGN(BorderContents);
};

void BorderContents::InitAndGetBounds(
    const gfx::Rect& position_relative_to,
    const gfx::Size& contents_size,
    bool is_rtl,
    gfx::Rect* inner_bounds,
    gfx::Rect* outer_bounds) {
  // Set the border.
  BubbleBorder* bubble_border = new BubbleBorder;
  set_border(bubble_border);
  bubble_border->set_background_color(kBackgroundColor);

  // Try putting the arrow in its default location, and calculating the bounds.
  BubbleBorder::ArrowLocation arrow_location(is_rtl ?
      BubbleBorder::TOP_RIGHT : BubbleBorder::TOP_LEFT);
  bubble_border->set_arrow_location(arrow_location);
  *outer_bounds = bubble_border->GetBounds(position_relative_to, contents_size);

  // See if those bounds will fit on the monitor.
  scoped_ptr<WindowSizer::MonitorInfoProvider> monitor_provider(
      WindowSizer::CreateDefaultMonitorInfoProvider());
  gfx::Rect monitor_bounds(
      monitor_provider->GetMonitorWorkAreaMatching(position_relative_to));
  if (!monitor_bounds.IsEmpty() && !monitor_bounds.Contains(*outer_bounds)) {
    // The bounds don't fit.  Move the arrow to try and improve things.
    bool arrow_on_left =
        (is_rtl ? (outer_bounds->x() < monitor_bounds.x()) :
                  (outer_bounds->right() <= monitor_bounds.right()));
    if (outer_bounds->bottom() > monitor_bounds.bottom()) {
      arrow_location = arrow_on_left ?
          BubbleBorder::BOTTOM_LEFT : BubbleBorder::BOTTOM_RIGHT;
    } else {
      arrow_location = arrow_on_left ?
          BubbleBorder::TOP_LEFT : BubbleBorder::TOP_RIGHT;
    }
    bubble_border->set_arrow_location(arrow_location);

    // Now get the recalculated bounds.
    *outer_bounds = bubble_border->GetBounds(position_relative_to,
                                             contents_size);
  }

  // Calculate the bounds of the contained contents by subtracting the border
  // dimensions.
  *inner_bounds = *outer_bounds;
  gfx::Insets insets;
  bubble_border->GetInsets(&insets);
  inner_bounds->Inset(insets.left(), insets.top(), insets.right(),
                      insets.bottom());
}

void BorderContents::Paint(gfx::Canvas* canvas) {
  // The border of this view creates an anti-aliased round-rect region for the
  // contents, which we need to fill with the background color.
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(kBackgroundColor);
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

// BorderWidget ---------------------------------------------------------------

BorderWidget::BorderWidget() {
  set_delete_on_destroy(false);  // Our owner will free us manually.
  set_window_style(WS_POPUP);
  set_window_ex_style(WS_EX_TOOLWINDOW | WS_EX_LAYERED);
}

gfx::Rect BorderWidget::InitAndGetBounds(
    HWND owner,
    const gfx::Rect& position_relative_to,
    const gfx::Size& contents_size,
    bool is_rtl) {
  // Margins around the contents.
  const int kLeftMargin = 6;
  const int kTopMargin = 6;
  const int kRightMargin = 6;
  const int kBottomMargin = 9;

  // Set up the border view and ask it to calculate our bounds (and our
  // contents').
  gfx::Size local_contents_size(contents_size);
  local_contents_size.Enlarge(kLeftMargin + kRightMargin,
                              kTopMargin + kBottomMargin);
  BorderContents* border_contents = new BorderContents;
  gfx::Rect inner_bounds, outer_bounds;
  border_contents->InitAndGetBounds(position_relative_to, local_contents_size,
                                    is_rtl, &inner_bounds, &outer_bounds);

  // Initialize ourselves.
  WidgetWin::Init(GetAncestor(owner, GA_ROOT), outer_bounds);
  SetContentsView(border_contents);
  SetWindowPos(owner, 0, 0, 0, 0,
               SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOREDRAW);

  // Chop a hole out of our region to show the contents through.
  // CreateRectRgn() expects (left, top, right, bottom) in window coordinates.
  inner_bounds.Inset(kLeftMargin, kTopMargin, kRightMargin, kBottomMargin);
  gfx::Rect region_bounds(inner_bounds);
  region_bounds.Offset(-outer_bounds.x(), -outer_bounds.y());
  HRGN inner_region = CreateRectRgn(region_bounds.x(), region_bounds.y(),
      region_bounds.right(), region_bounds.bottom());
  HRGN outer_region = CreateRectRgn(0, 0,
      outer_bounds.right(), outer_bounds.bottom());
  CombineRgn(outer_region, outer_region, inner_region, RGN_XOR);
  DeleteObject(inner_region);
  SetWindowRgn(outer_region, true);

  return inner_bounds;
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
InfoBubble* InfoBubble::Show(views::Window* parent,
                             const gfx::Rect& position_relative_to,
                             views::View* contents,
                             InfoBubbleDelegate* delegate) {
  InfoBubble* window = new InfoBubble;
  window->Init(parent, position_relative_to, contents, delegate);
  return window;
}

void InfoBubble::Close() {
  Close(false);
}

InfoBubble::InfoBubble()
    :
#if defined(OS_LINUX)
      WidgetGtk(TYPE_POPUP),
#endif
      delegate_(NULL),
      parent_(NULL),
      closed_(false) {
}

void InfoBubble::Init(views::Window* parent,
                      const gfx::Rect& position_relative_to,
                      views::View* contents,
                      InfoBubbleDelegate* delegate) {
  parent_ = parent;
  parent_->DisableInactiveRendering();

  delegate_ = delegate;

#if defined(OS_WIN)
  set_window_style(WS_POPUP | WS_CLIPCHILDREN);
  set_window_ex_style(WS_EX_TOOLWINDOW);
  border_.reset(new BorderWidget);
#endif

#if defined(OS_WIN)
  WidgetWin::Init(parent->GetNativeWindow(), gfx::Rect());
#else
  WidgetGtk::Init(GTK_WIDGET(parent->GetNativeWindow()), gfx::Rect());
#endif

  views::View* contents_view = new views::View;
  contents_view->set_background(
      views::Background::CreateSolidBackground(kBackgroundColor));
  contents_view->SetLayoutManager(new views::FillLayout);
  // Adding |contents| as a child has to be done before we call
  // contents->GetPreferredSize() below, since some supplied views don't
  // actually set themselves up until they're added to a hierarchy.
  contents_view->AddChildView(contents);
  SetContentsView(contents_view);

  gfx::Rect bounds;
#if defined(OS_WIN)
  bounds = border_->InitAndGetBounds(GetNativeView(),
                                     position_relative_to,
                                     contents_view->GetPreferredSize(),
                                     contents->UILayoutIsRightToLeft());

  // Register the Escape accelerator for closing.
  GetFocusManager()->RegisterAccelerator(
      views::Accelerator(VK_ESCAPE, false, false, false), this);
#endif
  SetBounds(bounds);

  NotificationService::current()->Notify(NotificationType::INFO_BUBBLE_CREATED,
                                         Source<InfoBubble>(this),
                                         NotificationService::NoDetails());

  // Show the window.
#if defined(OS_WIN)
  border_->ShowWindow(SW_SHOW);
  ShowWindow(SW_SHOW);
#else
  views::WidgetGtk::Show();
#endif
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
#else
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
