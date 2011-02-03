// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/constrained_window_win.h"

#include <algorithm>

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/browser/ui/toolbar/toolbar_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/window_sizer.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/notification_service.h"
#include "gfx/canvas.h"
#include "gfx/font.h"
#include "gfx/path.h"
#include "gfx/rect.h"
#include "grit/app_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "net/base/net_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "views/controls/button/image_button.h"
#include "views/focus/focus_manager.h"
#include "views/widget/widget_win.h"
#include "views/window/client_view.h"
#include "views/window/non_client_view.h"
#include "views/window/window_resources.h"
#include "views/window/window_shape.h"
#include "views/window/window_win.h"

using base::TimeDelta;

namespace views {
class ClientView;
}

// An enumeration of bitmap resources used by this window.
enum {
  FRAME_PART_BITMAP_FIRST = 0,  // Must be first.

  // Window Frame Border.
  FRAME_BOTTOM_EDGE,
  FRAME_BOTTOM_LEFT_CORNER,
  FRAME_BOTTOM_RIGHT_CORNER,
  FRAME_LEFT_EDGE,
  FRAME_RIGHT_EDGE,
  FRAME_TOP_EDGE,
  FRAME_TOP_LEFT_CORNER,
  FRAME_TOP_RIGHT_CORNER,

  FRAME_PART_BITMAP_COUNT  // Must be last.
};

static const int kXPFramePartIDs[] = {
    0,
    IDR_WINDOW_BOTTOM_CENTER, IDR_WINDOW_BOTTOM_LEFT_CORNER,
    IDR_WINDOW_BOTTOM_RIGHT_CORNER, IDR_WINDOW_LEFT_SIDE,
    IDR_WINDOW_RIGHT_SIDE, IDR_WINDOW_TOP_CENTER,
    IDR_WINDOW_TOP_LEFT_CORNER, IDR_WINDOW_TOP_RIGHT_CORNER,
    0 };
static const int kVistaFramePartIDs[] = {
    0,
    IDR_CONSTRAINED_BOTTOM_CENTER_V, IDR_CONSTRAINED_BOTTOM_LEFT_CORNER_V,
    IDR_CONSTRAINED_BOTTOM_RIGHT_CORNER_V, IDR_CONSTRAINED_LEFT_SIDE_V,
    IDR_CONSTRAINED_RIGHT_SIDE_V, IDR_CONSTRAINED_TOP_CENTER_V,
    IDR_CONSTRAINED_TOP_LEFT_CORNER_V, IDR_CONSTRAINED_TOP_RIGHT_CORNER_V,
    0 };

class XPWindowResources : public views::WindowResources {
 public:
  XPWindowResources() {
    InitClass();
  }
  virtual ~XPWindowResources() {}

  virtual SkBitmap* GetPartBitmap(views::FramePartBitmap part_id) const {
    return bitmaps_[part_id];
  }

 private:
  static void InitClass() {
    static bool initialized = false;
    if (!initialized) {
      ResourceBundle& rb = ResourceBundle::GetSharedInstance();
      for (int i = 0; i < FRAME_PART_BITMAP_COUNT; ++i) {
        int id = kXPFramePartIDs[i];
        if (id != 0)
          bitmaps_[i] = rb.GetBitmapNamed(id);
      }
      initialized = true;
    }
  }

  static SkBitmap* bitmaps_[FRAME_PART_BITMAP_COUNT];

  DISALLOW_COPY_AND_ASSIGN(XPWindowResources);
};

class VistaWindowResources : public views::WindowResources {
 public:
  VistaWindowResources() {
    InitClass();
  }
  virtual ~VistaWindowResources() {}

  virtual SkBitmap* GetPartBitmap(views::FramePartBitmap part_id) const {
    return bitmaps_[part_id];
  }

 private:
  static void InitClass() {
    static bool initialized = false;
    if (!initialized) {
      ResourceBundle& rb = ResourceBundle::GetSharedInstance();
      for (int i = 0; i < FRAME_PART_BITMAP_COUNT; ++i) {
        int id = kVistaFramePartIDs[i];
        if (id != 0)
          bitmaps_[i] = rb.GetBitmapNamed(id);
      }
      initialized = true;
    }
  }

  static SkBitmap* bitmaps_[FRAME_PART_BITMAP_COUNT];

  DISALLOW_COPY_AND_ASSIGN(VistaWindowResources);
};

SkBitmap* XPWindowResources::bitmaps_[];
SkBitmap* VistaWindowResources::bitmaps_[];

////////////////////////////////////////////////////////////////////////////////
// ConstrainedWindowFrameView

class ConstrainedWindowFrameView
    : public views::NonClientFrameView,
      public views::ButtonListener {
 public:
  explicit ConstrainedWindowFrameView(ConstrainedWindowWin* container);
  virtual ~ConstrainedWindowFrameView();

  void UpdateWindowTitle();

  // Overridden from views::NonClientFrameView:
  virtual gfx::Rect GetBoundsForClientView() const;
  virtual bool AlwaysUseCustomFrame() const;
  virtual gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const;
  virtual int NonClientHitTest(const gfx::Point& point);
  virtual void GetWindowMask(const gfx::Size& size, gfx::Path* window_mask);
  virtual void EnableClose(bool enable);
  virtual void ResetWindowControls() { }

  // Overridden from views::View:
  virtual void Paint(gfx::Canvas* canvas);
  virtual void Layout();
  virtual void OnThemeChanged();

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

 private:
  // Returns the thickness of the entire nonclient left, right, and bottom
  // borders, including both the window frame and any client edge.
  int NonClientBorderThickness() const;

  // Returns the height of the entire nonclient top border, including the window
  // frame, any title area, and any connected client edge.
  int NonClientTopBorderHeight() const;

  // Returns the thickness of the nonclient portion of the 3D edge along the
  // bottom of the titlebar.
  int TitlebarBottomThickness() const;

  // Returns what the size of the titlebar icon would be if there was one.
  int IconSize() const;

  // Returns what the titlebar icon's bounds would be if there was one.
  gfx::Rect IconBounds() const;

  // Paints different parts of the window to the incoming canvas.
  void PaintFrameBorder(gfx::Canvas* canvas);
  void PaintTitleBar(gfx::Canvas* canvas);
  void PaintClientEdge(gfx::Canvas* canvas);

  // Layout various sub-components of this view.
  void LayoutWindowControls();
  void LayoutTitleBar();

  // Returns the bounds of the client area for the specified view size.
  gfx::Rect CalculateClientAreaBounds(int width, int height) const;

  SkColor GetTitleColor() const {
    return (container_->owner()->profile()->IsOffTheRecord() ||
            !views::WidgetWin::IsAeroGlassEnabled()) ? SK_ColorWHITE
                                                     : SK_ColorBLACK;
  }

  // Loads the appropriate set of WindowResources for the frame view.
  void InitWindowResources();

  ConstrainedWindowWin* container_;

  scoped_ptr<views::WindowResources> resources_;

  gfx::Rect title_bounds_;

  views::ImageButton* close_button_;

  // The bounds of the ClientView.
  gfx::Rect client_view_bounds_;

  static void InitClass();

  // The font to be used to render the titlebar text.
  static gfx::Font* title_font_;

  DISALLOW_COPY_AND_ASSIGN(ConstrainedWindowFrameView);
};

gfx::Font* ConstrainedWindowFrameView::title_font_ = NULL;

namespace {
// The frame border is only visible in restored mode and is hardcoded to 4 px on
// each side regardless of the system window border size.
const int kFrameBorderThickness = 4;
// Various edges of the frame border have a 1 px shadow along their edges; in a
// few cases we shift elements based on this amount for visual appeal.
const int kFrameShadowThickness = 1;
// In the window corners, the resize areas don't actually expand bigger, but the
// 16 px at the end of each edge triggers diagonal resizing.
const int kResizeAreaCornerSize = 16;
// The titlebar never shrinks too short to show the caption button plus some
// padding below it.
const int kCaptionButtonHeightWithPadding = 19;
// The titlebar has a 2 px 3D edge along the top and bottom.
const int kTitlebarTopAndBottomEdgeThickness = 2;
// The icon would never shrink below 16 px on a side, if there was one.
const int kIconMinimumSize = 16;
// The title text starts 2 px from the right edge of the left frame border.
const int kTitleLeftSpacing = 2;
// There is a 5 px gap between the title text and the caption buttons.
const int kTitleCaptionSpacing = 5;

const SkColor kContentsBorderShadow = SkColorSetARGB(51, 0, 0, 0);
}

////////////////////////////////////////////////////////////////////////////////
// ConstrainedWindowFrameView, public:

ConstrainedWindowFrameView::ConstrainedWindowFrameView(
    ConstrainedWindowWin* container)
        : NonClientFrameView(),
          container_(container),
          close_button_(new views::ImageButton(this)) {
  InitClass();
  InitWindowResources();

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  close_button_->SetImage(views::CustomButton::BS_NORMAL,
                          rb.GetBitmapNamed(IDR_CLOSE_SA));
  close_button_->SetImage(views::CustomButton::BS_HOT,
                          rb.GetBitmapNamed(IDR_CLOSE_SA_H));
  close_button_->SetImage(views::CustomButton::BS_PUSHED,
                          rb.GetBitmapNamed(IDR_CLOSE_SA_P));
  close_button_->SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                                   views::ImageButton::ALIGN_MIDDLE);
  AddChildView(close_button_);
}

ConstrainedWindowFrameView::~ConstrainedWindowFrameView() {
}

void ConstrainedWindowFrameView::UpdateWindowTitle() {
  SchedulePaint(title_bounds_, false);
}

////////////////////////////////////////////////////////////////////////////////
// ConstrainedWindowFrameView, views::NonClientFrameView implementation:

gfx::Rect ConstrainedWindowFrameView::GetBoundsForClientView() const {
  return client_view_bounds_;
}

bool ConstrainedWindowFrameView::AlwaysUseCustomFrame() const {
  // Constrained windows always use the custom frame - they just have a
  // different set of bitmaps.
  return true;
}

gfx::Rect ConstrainedWindowFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  int top_height = NonClientTopBorderHeight();
  int border_thickness = NonClientBorderThickness();
  return gfx::Rect(std::max(0, client_bounds.x() - border_thickness),
                   std::max(0, client_bounds.y() - top_height),
                   client_bounds.width() + (2 * border_thickness),
                   client_bounds.height() + top_height + border_thickness);
}

int ConstrainedWindowFrameView::NonClientHitTest(const gfx::Point& point) {
  if (!bounds().Contains(point))
    return HTNOWHERE;

  int frame_component = container_->GetClientView()->NonClientHitTest(point);

  // See if we're in the sysmenu region.  (We check the ClientView first to be
  // consistent with OpaqueBrowserFrameView; it's not really necessary here.)
  gfx::Rect sysmenu_rect(IconBounds());
  sysmenu_rect.set_x(MirroredLeftPointForRect(sysmenu_rect));
  if (sysmenu_rect.Contains(point))
    return (frame_component == HTCLIENT) ? HTCLIENT : HTSYSMENU;

  if (frame_component != HTNOWHERE)
    return frame_component;

  // Then see if the point is within any of the window controls.
  if (close_button_->GetBounds(APPLY_MIRRORING_TRANSFORMATION).Contains(point))
    return HTCLOSE;

  int window_component = GetHTComponentForFrame(point, kFrameBorderThickness,
      NonClientBorderThickness(), kResizeAreaCornerSize, kResizeAreaCornerSize,
      container_->GetDelegate()->CanResize());
  // Fall back to the caption if no other component matches.
  return (window_component == HTNOWHERE) ? HTCAPTION : window_component;
}

void ConstrainedWindowFrameView::GetWindowMask(const gfx::Size& size,
                                               gfx::Path* window_mask) {
  DCHECK(window_mask);
  views::GetDefaultWindowMask(size, window_mask);
}

void ConstrainedWindowFrameView::EnableClose(bool enable) {
  close_button_->SetEnabled(enable);
}

////////////////////////////////////////////////////////////////////////////////
// ConstrainedWindowFrameView, views::View implementation:

void ConstrainedWindowFrameView::Paint(gfx::Canvas* canvas) {
  PaintFrameBorder(canvas);
  PaintTitleBar(canvas);
  PaintClientEdge(canvas);
}

void ConstrainedWindowFrameView::Layout() {
  LayoutWindowControls();
  LayoutTitleBar();
  client_view_bounds_ = CalculateClientAreaBounds(width(), height());
}

void ConstrainedWindowFrameView::OnThemeChanged() {
  InitWindowResources();
}

////////////////////////////////////////////////////////////////////////////////
// ConstrainedWindowFrameView, views::ButtonListener implementation:

void ConstrainedWindowFrameView::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  if (sender == close_button_)
    container_->ExecuteSystemMenuCommand(SC_CLOSE);
}

////////////////////////////////////////////////////////////////////////////////
// ConstrainedWindowFrameView, private:

int ConstrainedWindowFrameView::NonClientBorderThickness() const {
  return kFrameBorderThickness + kClientEdgeThickness;
}

int ConstrainedWindowFrameView::NonClientTopBorderHeight() const {
  return std::max(kFrameBorderThickness + IconSize(),
                  kFrameShadowThickness + kCaptionButtonHeightWithPadding) +
      TitlebarBottomThickness();
}

int ConstrainedWindowFrameView::TitlebarBottomThickness() const {
  return kTitlebarTopAndBottomEdgeThickness + kClientEdgeThickness;
}

int ConstrainedWindowFrameView::IconSize() const {
#if defined(OS_WIN)
  // This metric scales up if either the titlebar height or the titlebar font
  // size are increased.
  return GetSystemMetrics(SM_CYSMICON);
#else
  return std::max(title_font_->height(), kIconMinimumSize);
#endif
}

gfx::Rect ConstrainedWindowFrameView::IconBounds() const {
  int size = IconSize();
  // Our frame border has a different "3D look" than Windows'.  Theirs has a
  // more complex gradient on the top that they push their icon/title below;
  // then the maximized window cuts this off and the icon/title are centered
  // in the remaining space.  Because the apparent shape of our border is
  // simpler, using the same positioning makes things look slightly uncentered
  // with restored windows, so instead of calculating the remaining space from
  // below the frame border, we calculate from below the 3D edge.
  int unavailable_px_at_top = kTitlebarTopAndBottomEdgeThickness;
  // When the icon is shorter than the minimum space we reserve for the caption
  // button, we vertically center it.  We want to bias rounding to put extra
  // space above the icon, since the 3D edge + client edge below looks (to the
  // eye) more like additional space than does the 3D edge above; hence the +1.
  int y = unavailable_px_at_top + (NonClientTopBorderHeight() -
      unavailable_px_at_top - size - TitlebarBottomThickness() + 1) / 2;
  return gfx::Rect(kFrameBorderThickness + kTitleLeftSpacing, y, size, size);
}

void ConstrainedWindowFrameView::PaintFrameBorder(gfx::Canvas* canvas) {
  SkBitmap* top_left_corner = resources_->GetPartBitmap(FRAME_TOP_LEFT_CORNER);
  SkBitmap* top_right_corner =
      resources_->GetPartBitmap(FRAME_TOP_RIGHT_CORNER);
  SkBitmap* top_edge = resources_->GetPartBitmap(FRAME_TOP_EDGE);
  SkBitmap* right_edge = resources_->GetPartBitmap(FRAME_RIGHT_EDGE);
  SkBitmap* left_edge = resources_->GetPartBitmap(FRAME_LEFT_EDGE);
  SkBitmap* bottom_left_corner =
      resources_->GetPartBitmap(FRAME_BOTTOM_LEFT_CORNER);
  SkBitmap* bottom_right_corner =
      resources_->GetPartBitmap(FRAME_BOTTOM_RIGHT_CORNER);
  SkBitmap* bottom_edge = resources_->GetPartBitmap(FRAME_BOTTOM_EDGE);

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  SkBitmap* theme_frame = rb.GetBitmapNamed(IDR_THEME_FRAME);
  SkColor frame_color = ResourceBundle::frame_color;

  // Fill with the frame color first so we have a constant background for
  // areas not covered by the theme image.
  canvas->FillRectInt(frame_color, 0, 0, width(), theme_frame->height());
  // Now fill down the sides.
  canvas->FillRectInt(frame_color, 0, theme_frame->height(), left_edge->width(),
                      height() - theme_frame->height());
  canvas->FillRectInt(frame_color, width() - right_edge->width(),
                      theme_frame->height(), right_edge->width(),
                      height() - theme_frame->height());
  // Now fill the bottom area.
  canvas->FillRectInt(frame_color,
      left_edge->width(), height() - bottom_edge->height(),
      width() - left_edge->width() - right_edge->width(),
      bottom_edge->height());

  // Draw the theme frame.
  canvas->TileImageInt(*theme_frame, 0, 0, width(), theme_frame->height());

  // Top.
  canvas->DrawBitmapInt(*top_left_corner, 0, 0);
  canvas->TileImageInt(*top_edge, top_left_corner->width(), 0,
                       width() - top_right_corner->width(), top_edge->height());
  canvas->DrawBitmapInt(*top_right_corner,
                        width() - top_right_corner->width(), 0);

  // Right.
  canvas->TileImageInt(*right_edge, width() - right_edge->width(),
      top_right_corner->height(), right_edge->width(),
      height() - top_right_corner->height() - bottom_right_corner->height());

  // Bottom.
  canvas->DrawBitmapInt(*bottom_right_corner,
                        width() - bottom_right_corner->width(),
                        height() - bottom_right_corner->height());
  canvas->TileImageInt(*bottom_edge, bottom_left_corner->width(),
      height() - bottom_edge->height(),
      width() - bottom_left_corner->width() - bottom_right_corner->width(),
      bottom_edge->height());
  canvas->DrawBitmapInt(*bottom_left_corner, 0,
                        height() - bottom_left_corner->height());

  // Left.
  canvas->TileImageInt(*left_edge, 0, top_left_corner->height(),
      left_edge->width(),
      height() - top_left_corner->height() - bottom_left_corner->height());
}

void ConstrainedWindowFrameView::PaintTitleBar(gfx::Canvas* canvas) {
  canvas->DrawStringInt(container_->GetWindowTitle(), *title_font_,
      GetTitleColor(), MirroredLeftPointForRect(title_bounds_),
      title_bounds_.y(), title_bounds_.width(), title_bounds_.height());
}

void ConstrainedWindowFrameView::PaintClientEdge(gfx::Canvas* canvas) {
  gfx::Rect client_edge_bounds(CalculateClientAreaBounds(width(), height()));
  client_edge_bounds.Inset(-kClientEdgeThickness, -kClientEdgeThickness);
  gfx::Rect frame_shadow_bounds(client_edge_bounds);
  frame_shadow_bounds.Inset(-kFrameShadowThickness, -kFrameShadowThickness);

  canvas->FillRectInt(kContentsBorderShadow, frame_shadow_bounds.x(),
                      frame_shadow_bounds.y(), frame_shadow_bounds.width(),
                      frame_shadow_bounds.height());

  canvas->FillRectInt(ResourceBundle::toolbar_color, client_edge_bounds.x(),
                      client_edge_bounds.y(), client_edge_bounds.width(),
                      client_edge_bounds.height());
}

void ConstrainedWindowFrameView::LayoutWindowControls() {
  gfx::Size close_button_size = close_button_->GetPreferredSize();
  close_button_->SetBounds(
      width() - kFrameBorderThickness - close_button_size.width(),
      kFrameShadowThickness, close_button_size.width(),
      close_button_size.height());
}

void ConstrainedWindowFrameView::LayoutTitleBar() {
  // The window title is based on the calculated icon position, even though'
  // there is no icon in constrained windows.
  gfx::Rect icon_bounds(IconBounds());
  int title_x = icon_bounds.x();
  int title_height = title_font_->GetHeight();
  // We bias the title position so that when the difference between the icon and
  // title heights is odd, the extra pixel of the title is above the vertical
  // midline rather than below.  This compensates for how the icon is already
  // biased downwards (see IconBounds()) and helps prevent descenders on the
  // title from overlapping the 3D edge at the bottom of the titlebar.
  title_bounds_.SetRect(title_x,
      icon_bounds.y() + ((icon_bounds.height() - title_height - 1) / 2),
      std::max(0, close_button_->x() - kTitleCaptionSpacing - title_x),
      title_height);
}

gfx::Rect ConstrainedWindowFrameView::CalculateClientAreaBounds(
    int width,
    int height) const {
  int top_height = NonClientTopBorderHeight();
  int border_thickness = NonClientBorderThickness();
  return gfx::Rect(border_thickness, top_height,
                   std::max(0, width - (2 * border_thickness)),
                   std::max(0, height - top_height - border_thickness));
}

void ConstrainedWindowFrameView::InitWindowResources() {
  resources_.reset(views::WidgetWin::IsAeroGlassEnabled() ?
    static_cast<views::WindowResources*>(new VistaWindowResources) :
    new XPWindowResources);
}

// static
void ConstrainedWindowFrameView::InitClass() {
  static bool initialized = false;
  if (!initialized) {
    title_font_ = new gfx::Font(views::WindowWin::GetWindowTitleFont());
    initialized = true;
  }
}

////////////////////////////////////////////////////////////////////////////////
// ConstrainedWindowWin, public:

ConstrainedWindowWin::~ConstrainedWindowWin() {
}

////////////////////////////////////////////////////////////////////////////////
// ConstrainedWindowWin, ConstrainedWindow implementation:

views::NonClientFrameView* ConstrainedWindowWin::CreateFrameViewForWindow() {
  return new ConstrainedWindowFrameView(this);
}

void ConstrainedWindowWin::FocusConstrainedWindow() {
  if ((!owner_->delegate() ||
       owner_->delegate()->ShouldFocusConstrainedWindow()) &&
      GetDelegate() && GetDelegate()->GetInitiallyFocusedView()) {
    GetDelegate()->GetInitiallyFocusedView()->RequestFocus();
  }
}

void ConstrainedWindowWin::ShowConstrainedWindow() {
  // We marked the view as hidden during construction.  Mark it as
  // visible now so FocusManager will let us receive focus.
  GetNonClientView()->SetVisible(true);
  if (owner_->delegate())
    owner_->delegate()->WillShowConstrainedWindow(owner_);
  ActivateConstrainedWindow();
  FocusConstrainedWindow();
}

void ConstrainedWindowWin::CloseConstrainedWindow() {
  // Broadcast to all observers of NOTIFY_CWINDOW_CLOSED.
  // One example of such an observer is AutomationCWindowTracker in the
  // automation component.
  NotificationService::current()->Notify(NotificationType::CWINDOW_CLOSED,
                                         Source<ConstrainedWindow>(this),
                                         NotificationService::NoDetails());

  Close();
}

std::wstring ConstrainedWindowWin::GetWindowTitle() const {
  if (GetDelegate())
    return GetDelegate()->GetWindowTitle();

  // TODO(pkasting): Shouldn't this be using a localized string, or else calling
  // NOTREACHED()?
  return std::wstring(L"Untitled");
}

const gfx::Rect& ConstrainedWindowWin::GetCurrentBounds() const {
  return current_bounds_;
}

////////////////////////////////////////////////////////////////////////////////
// ConstrainedWindowWin, private:

ConstrainedWindowWin::ConstrainedWindowWin(
    TabContents* owner,
    views::WindowDelegate* window_delegate)
    : WindowWin(window_delegate),
      owner_(owner) {
  GetNonClientView()->SetFrameView(CreateFrameViewForWindow());

  set_window_style(WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_CAPTION |
                   WS_THICKFRAME | WS_SYSMENU);
  set_focus_on_creation(false);
  // Views default to visible.  Since we are creating a window that is
  // not visible (no WS_VISIBLE), mark our View as hidden so that
  // FocusManager can deal with it properly.
  GetNonClientView()->SetVisible(false);

  WindowWin::Init(owner_->GetNativeView(), gfx::Rect());
}

void ConstrainedWindowWin::ActivateConstrainedWindow() {
  // Other pop-ups are simply moved to the front of the z-order.
  SetWindowPos(HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
}

////////////////////////////////////////////////////////////////////////////////
// ConstrainedWindowWin, views::WidgetWin overrides:

void ConstrainedWindowWin::OnDestroy() {
  // TODO(jcampan): figure out focus restoration

  // Make sure we call super so that it can do its cleanup.
  WindowWin::OnDestroy();
}

void ConstrainedWindowWin::OnFinalMessage(HWND window) {
  // Tell our constraining TabContents that we've gone so it can update its
  // list.
  owner_->WillClose(this);

  WindowWin::OnFinalMessage(window);
}

LRESULT ConstrainedWindowWin::OnMouseActivate(HWND window,
                                               UINT hittest_code,
                                               UINT message) {
  // We only detach the window if the user clicked on the title bar. That
  // way, users can click inside the contents of legitimate popups obtained
  // with a mouse gesture.
  if (hittest_code != HTCLIENT && hittest_code != HTNOWHERE &&
      hittest_code != HTCLOSE) {
    ActivateConstrainedWindow();
  }

  return MA_ACTIVATE;
}

void ConstrainedWindowWin::OnWindowPosChanged(WINDOWPOS* window_pos) {
  // If the window was moved or sized, tell the owner.
  if (!(window_pos->flags & SWP_NOMOVE) || !(window_pos->flags & SWP_NOSIZE))
    owner_->DidMoveOrResize(this);
  SetMsgHandled(FALSE);
}


// static
ConstrainedWindow* ConstrainedWindow::CreateConstrainedDialog(
    TabContents* parent,
    views::WindowDelegate* window_delegate) {
  ConstrainedWindowWin* window = new ConstrainedWindowWin(parent,
                                                          window_delegate);
  return window;
}
