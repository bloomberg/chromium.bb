// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/info_bubble.h"

#include "app/gfx/canvas.h"
#include "app/gfx/color_utils.h"
#include "app/gfx/path.h"
#include "base/keyboard_codes.h"
#include "chrome/browser/window_sizer.h"
#include "chrome/common/notification_service.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "views/fill_layout.h"
#include "views/widget/root_view.h"
#include "views/window/window.h"

// Background color of the bubble.
#if defined(OS_WIN)
const SkColor InfoBubble::kBackgroundColor =
    color_utils::GetSysSkColor(COLOR_WINDOW);
#else
// TODO(beng): source from theme provider.
const SkColor InfoBubble::kBackgroundColor = SK_ColorWHITE;
#endif

// BorderContents -------------------------------------------------------------

// This is used to paint the border of the InfoBubble.  Windows uses this via
// BorderWidget (see below), while others can use it directly in the bubble.
class BorderContents : public views::View {
 public:
  BorderContents() { }

  // Given the size of the contents and the rect to point at, initializes the
  // bubble and returns the bounds of both the border
  // and the contents inside the bubble.
  // |prefer_arrow_on_right| specifies the preferred location for the arrow
  // anchor. If the bubble does not fit on the monitor, the arrow location may
  // changed so it can.
  //
  // TODO(pkasting): Maybe this should use mirroring transformations instead,
  // which would hopefully simplify this code.
  void InitAndGetBounds(
      const gfx::Rect& position_relative_to,  // In screen coordinates
      const gfx::Size& contents_size,
      bool prefer_arrow_on_right,
      gfx::Rect* contents_bounds,             // Returned in window coordinates
      gfx::Rect* window_bounds);              // Returned in screen coordinates

 private:
  virtual ~BorderContents() { }

  // Overridden from View:
  virtual void Paint(gfx::Canvas* canvas);

  DISALLOW_COPY_AND_ASSIGN(BorderContents);
};

void BorderContents::InitAndGetBounds(
    const gfx::Rect& position_relative_to,
    const gfx::Size& contents_size,
    bool prefer_arrow_on_right,
    gfx::Rect* contents_bounds,
    gfx::Rect* window_bounds) {
  // Margins between the contents and the inside of the border, in pixels.
  const int kLeftMargin = 6;
  const int kTopMargin = 6;
  const int kRightMargin = 6;
  const int kBottomMargin = 9;

  // Set the border.
  BubbleBorder* bubble_border = new BubbleBorder;
  set_border(bubble_border);
  bubble_border->set_background_color(InfoBubble::kBackgroundColor);

  // Give the contents a margin.
  gfx::Size local_contents_size(contents_size);
  local_contents_size.Enlarge(kLeftMargin + kRightMargin,
                              kTopMargin + kBottomMargin);

  // Try putting the arrow in its initial location, and calculating the
  // bounds.
  BubbleBorder::ArrowLocation arrow_location(prefer_arrow_on_right ?
      BubbleBorder::TOP_RIGHT : BubbleBorder::TOP_LEFT);
  bubble_border->set_arrow_location(arrow_location);
  *window_bounds =
      bubble_border->GetBounds(position_relative_to, local_contents_size);

  // See if those bounds will fit on the monitor.
  scoped_ptr<WindowSizer::MonitorInfoProvider> monitor_provider(
      WindowSizer::CreateDefaultMonitorInfoProvider());
  gfx::Rect monitor_bounds(
      monitor_provider->GetMonitorWorkAreaMatching(position_relative_to));
  if (!monitor_bounds.IsEmpty() && !monitor_bounds.Contains(*window_bounds)) {
    // The bounds don't fit.  Move the arrow to try and improve things.
    bool arrow_on_left = prefer_arrow_on_right ?
        (window_bounds->x() < monitor_bounds.x()) :
        (window_bounds->right() <= monitor_bounds.right());
    if (window_bounds->bottom() > monitor_bounds.bottom()) {
      arrow_location = arrow_on_left ?
          BubbleBorder::BOTTOM_LEFT : BubbleBorder::BOTTOM_RIGHT;
    } else {
      arrow_location = arrow_on_left ?
          BubbleBorder::TOP_LEFT : BubbleBorder::TOP_RIGHT;
    }
    bubble_border->set_arrow_location(arrow_location);

    // Now get the recalculated bounds.
    *window_bounds = bubble_border->GetBounds(position_relative_to,
                                              local_contents_size);
  }

  // Calculate the bounds of the contained contents (in window coordinates) by
  // subtracting the border dimensions and margin amounts.
  *contents_bounds = gfx::Rect(gfx::Point(), window_bounds->size());
  gfx::Insets insets;
  bubble_border->GetInsets(&insets);
  contents_bounds->Inset(insets.left() + kLeftMargin, insets.top() + kTopMargin,
      insets.right() + kRightMargin, insets.bottom() + kBottomMargin);
}

void BorderContents::Paint(gfx::Canvas* canvas) {
  // The border of this view creates an anti-aliased round-rect region for the
  // contents, which we need to fill with the background color.
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

BorderWidget::BorderWidget() {
  set_delete_on_destroy(false);  // Our owner will free us manually.
  set_window_style(WS_POPUP);
  set_window_ex_style(WS_EX_TOOLWINDOW | WS_EX_LAYERED);
}

gfx::Rect BorderWidget::InitAndGetBounds(
    HWND owner,
    const gfx::Rect& position_relative_to,
    const gfx::Size& contents_size,
    bool prefer_arrow_on_right) {
  // Set up the border view and ask it to calculate our bounds (and our
  // contents').
  BorderContents* border_contents = new BorderContents;
  gfx::Rect contents_bounds, window_bounds;
  border_contents->InitAndGetBounds(position_relative_to, contents_size,
                                    prefer_arrow_on_right, &contents_bounds,
                                    &window_bounds);

  // Initialize ourselves.
  WidgetWin::Init(GetAncestor(owner, GA_ROOT), window_bounds);
  SetContentsView(border_contents);
  SetWindowPos(owner, 0, 0, 0, 0,
               SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOREDRAW);

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
      WidgetGtk(TYPE_WINDOW),
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
  delegate_ = delegate;

  // Create the main window.
#if defined(OS_WIN)
  parent_->DisableInactiveRendering();
  set_window_style(WS_POPUP | WS_CLIPCHILDREN);
  set_window_ex_style(WS_EX_TOOLWINDOW);
  WidgetWin::Init(parent->GetNativeWindow(), gfx::Rect());
#elif defined(OS_LINUX)
  MakeTransparent();
  make_transient_to_parent();
  WidgetGtk::Init(GTK_WIDGET(parent->GetNativeWindow()), gfx::Rect());
#endif

  // Create a View to hold the contents of the main window.
  views::View* contents_view = new views::View;
  // Adding |contents| as a child has to be done before we call
  // contents->GetPreferredSize() below, since some supplied views don't
  // actually initialize themselves until they're added to a hierarchy.
  contents_view->AddChildView(contents);
  SetContentsView(contents_view);

  // Calculate and set the bounds for all windows and views.
  gfx::Rect window_bounds;

  bool prefer_arrow_on_right =
      (contents->UILayoutIsRightToLeft() == delegate->PreferOriginSideAnchor());

#if defined(OS_WIN)
  border_.reset(new BorderWidget);
  // Initialize and position the border window.
  window_bounds = border_->InitAndGetBounds(GetNativeView(),
      position_relative_to, contents->GetPreferredSize(),
      prefer_arrow_on_right);

  // Make |contents| take up the entire contents view.
  contents_view->SetLayoutManager(new views::FillLayout);

  // Paint the background color behind the contents.
  contents_view->set_background(
      views::Background::CreateSolidBackground(kBackgroundColor));
#else
  // Create a view to paint the border and background.
  BorderContents* border_contents = new BorderContents;
  gfx::Rect contents_bounds;
  border_contents->InitAndGetBounds(position_relative_to,
      contents->GetPreferredSize(), prefer_arrow_on_right,
      &contents_bounds, &window_bounds);
  // This new view must be added before |contents| so it will paint under it.
  contents_view->AddChildView(0, border_contents);

  // |contents_view| has no layout manager, so we have to explicitly position
  // its children.
  border_contents->SetBounds(gfx::Rect(gfx::Point(), window_bounds.size()));
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
