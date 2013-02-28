// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/constrained_window_views.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/toolbar/toolbar_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/theme_image_mapper.h"
#include "chrome/browser/ui/web_contents_modal_dialog_manager.h"
#include "chrome/common/chrome_constants.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/ui_resources.h"
#include "net/base/net_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/hit_test.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/path.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/screen.h"
#include "ui/views/color_constants.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/client_view.h"
#include "ui/views/window/dialog_client_view.h"
#include "ui/views/window/dialog_delegate.h"
#include "ui/views/window/frame_background.h"
#include "ui/views/window/non_client_view.h"
#include "ui/views/window/window_resources.h"
#include "ui/views/window/window_shape.h"

#if defined(OS_WIN) && !defined(USE_AURA)
#include "ui/base/win/shell.h"
#include "ui/views/widget/native_widget_win.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/window.h"
#include "ui/views/corewm/visibility_controller.h"
#include "ui/views/corewm/window_animations.h"
#include "ui/views/corewm/window_modality_controller.h"
#endif

#if defined(USE_ASH)
#include "ash/ash_constants.h"
#include "ash/shell.h"
#include "ash/wm/custom_frame_view_ash.h"
#endif

using base::TimeDelta;

namespace views {
class ClientView;
}

// An enumeration of image resources used by this window.
enum {
  FRAME_PART_IMAGE_FIRST = 0,  // Must be first.

  // Window Frame Border.
  FRAME_BOTTOM_EDGE,
  FRAME_BOTTOM_LEFT_CORNER,
  FRAME_BOTTOM_RIGHT_CORNER,
  FRAME_LEFT_EDGE,
  FRAME_RIGHT_EDGE,
  FRAME_TOP_EDGE,
  FRAME_TOP_LEFT_CORNER,
  FRAME_TOP_RIGHT_CORNER,

  FRAME_PART_IMAGE_COUNT  // Must be last.
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

  virtual gfx::ImageSkia* GetPartImage(
      views::FramePartImage part_id) const OVERRIDE {
    return images_[part_id];
  }

 private:
  static void InitClass() {
    static bool initialized = false;
    if (!initialized) {
      ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
      for (int i = 0; i < FRAME_PART_IMAGE_COUNT; ++i) {
        int id = kXPFramePartIDs[i];
        if (id != 0)
          images_[i] = rb.GetImageSkiaNamed(id);
      }
      initialized = true;
    }
  }

  static gfx::ImageSkia* images_[FRAME_PART_IMAGE_COUNT];

  DISALLOW_COPY_AND_ASSIGN(XPWindowResources);
};

class VistaWindowResources : public views::WindowResources {
 public:
  VistaWindowResources() {
    InitClass();
  }
  virtual ~VistaWindowResources() {}

  virtual gfx::ImageSkia* GetPartImage(
      views::FramePartImage part_id) const OVERRIDE {
    return images_[part_id];
  }

 private:
  static void InitClass() {
    static bool initialized = false;
    if (!initialized) {
      ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
      for (int i = 0; i < FRAME_PART_IMAGE_COUNT; ++i) {
        int id = kVistaFramePartIDs[i];
        if (id != 0)
          images_[i] = rb.GetImageSkiaNamed(id);
      }
      initialized = true;
    }
  }

  static gfx::ImageSkia* images_[FRAME_PART_IMAGE_COUNT];

  DISALLOW_COPY_AND_ASSIGN(VistaWindowResources);
};

gfx::ImageSkia* XPWindowResources::images_[];
gfx::ImageSkia* VistaWindowResources::images_[];

class ConstrainedWindowFrameView : public views::NonClientFrameView,
                                   public views::ButtonListener {
 public:
  ConstrainedWindowFrameView(views::Widget* container,
                             bool browser_is_off_the_record);
  virtual ~ConstrainedWindowFrameView();

  virtual void UpdateWindowTitle() OVERRIDE;

  // Overridden from views::NonClientFrameView:
  virtual gfx::Rect GetBoundsForClientView() const OVERRIDE;
  virtual gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const OVERRIDE;
  virtual int NonClientHitTest(const gfx::Point& point) OVERRIDE;
  virtual void GetWindowMask(const gfx::Size& size,
                             gfx::Path* window_mask) OVERRIDE;
  virtual void ResetWindowControls() OVERRIDE {}
  virtual void UpdateWindowIcon() OVERRIDE {}

  // Overridden from views::View:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual void OnThemeChanged() OVERRIDE;

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

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
    return browser_is_off_the_record_
#if defined(OS_WIN) && !defined(USE_AURA)
            || !ui::win::IsAeroGlassEnabled()
#endif
            ? SK_ColorWHITE : SK_ColorBLACK;
  }

  // Loads the appropriate set of WindowResources for the frame view.
  void InitWindowResources();

  views::Widget* container_;

  bool browser_is_off_the_record_;

  scoped_ptr<views::WindowResources> resources_;

  gfx::Rect title_bounds_;

  views::ImageButton* close_button_;

  // The bounds of the ClientView.
  gfx::Rect client_view_bounds_;

  // Background painter for the frame.
  scoped_ptr<views::FrameBackground> frame_background_;

  static void InitClass();

  // The font to be used to render the titlebar text.
  static const gfx::Font* title_font_;

  DISALLOW_COPY_AND_ASSIGN(ConstrainedWindowFrameView);
};

const gfx::Font* ConstrainedWindowFrameView::title_font_ = NULL;

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

}  // namespace

ConstrainedWindowFrameView::ConstrainedWindowFrameView(
    views::Widget* container, bool browser_is_off_the_record)
    : NonClientFrameView(),
      container_(container),
      browser_is_off_the_record_(browser_is_off_the_record),
      close_button_(new views::ImageButton(this)),
      frame_background_(new views::FrameBackground()) {
  InitClass();
  InitWindowResources();

  // Constrained windows always use the custom frame - they just have a
  // different set of images.
  container->set_frame_type(views::Widget::FRAME_TYPE_FORCE_CUSTOM);

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  close_button_->SetImage(views::CustomButton::STATE_NORMAL,
                          rb.GetImageSkiaNamed(IDR_CLOSE_SA));
  close_button_->SetImage(views::CustomButton::STATE_HOVERED,
                          rb.GetImageSkiaNamed(IDR_CLOSE_SA_H));
  close_button_->SetImage(views::CustomButton::STATE_PRESSED,
                          rb.GetImageSkiaNamed(IDR_CLOSE_SA_P));
  close_button_->SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                                   views::ImageButton::ALIGN_MIDDLE);
  AddChildView(close_button_);
}

ConstrainedWindowFrameView::~ConstrainedWindowFrameView() {
}

void ConstrainedWindowFrameView::UpdateWindowTitle() {
  SchedulePaintInRect(title_bounds_);
}

gfx::Rect ConstrainedWindowFrameView::GetBoundsForClientView() const {
  return client_view_bounds_;
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

  int frame_component =
      container_->client_view()->NonClientHitTest(point);

  // See if we're in the sysmenu region.  (We check the ClientView first to be
  // consistent with OpaqueBrowserFrameView; it's not really necessary here.)
  gfx::Rect sysmenu_rect(IconBounds());
  sysmenu_rect.set_x(GetMirroredXForRect(sysmenu_rect));
  if (sysmenu_rect.Contains(point))
    return (frame_component == HTCLIENT) ? HTCLIENT : HTSYSMENU;

  if (frame_component != HTNOWHERE)
    return frame_component;

  // Then see if the point is within any of the window controls.
  if (close_button_->GetMirroredBounds().Contains(point))
    return HTCLOSE;

  int window_component = GetHTComponentForFrame(point, kFrameBorderThickness,
      NonClientBorderThickness(), kResizeAreaCornerSize, kResizeAreaCornerSize,
      container_->widget_delegate()->CanResize());
  // Fall back to the caption if no other component matches.
  return (window_component == HTNOWHERE) ? HTCAPTION : window_component;
}

void ConstrainedWindowFrameView::GetWindowMask(const gfx::Size& size,
                                               gfx::Path* window_mask) {
  DCHECK(window_mask);
  views::GetDefaultWindowMask(size, window_mask);
}

void ConstrainedWindowFrameView::OnPaint(gfx::Canvas* canvas) {
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

void ConstrainedWindowFrameView::ButtonPressed(
    views::Button* sender, const ui::Event& event) {
  if (sender == close_button_)
    container_->Close();
}

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
  return std::max(title_font_->GetHeight(), kIconMinimumSize);
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
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  frame_background_->set_frame_color(ThemeProperties::GetDefaultColor(
      ThemeProperties::COLOR_FRAME));
  chrome::HostDesktopType desktop_type =
      chrome::GetHostDesktopTypeForNativeView(GetWidget()->GetNativeView());
  gfx::ImageSkia* theme_frame = rb.GetImageSkiaNamed(
      chrome::MapThemeImage(desktop_type, IDR_THEME_FRAME));
  frame_background_->set_theme_image(theme_frame);
  frame_background_->set_theme_overlay_image(NULL);
  frame_background_->set_top_area_height(theme_frame->height());

  frame_background_->SetCornerImages(
      resources_->GetPartImage(FRAME_TOP_LEFT_CORNER),
      resources_->GetPartImage(FRAME_TOP_RIGHT_CORNER),
      resources_->GetPartImage(FRAME_BOTTOM_LEFT_CORNER),
      resources_->GetPartImage(FRAME_BOTTOM_RIGHT_CORNER));
  frame_background_->SetSideImages(
      resources_->GetPartImage(FRAME_LEFT_EDGE),
      resources_->GetPartImage(FRAME_TOP_EDGE),
      resources_->GetPartImage(FRAME_RIGHT_EDGE),
      resources_->GetPartImage(FRAME_BOTTOM_EDGE));
  frame_background_->PaintRestored(canvas, this);
}

void ConstrainedWindowFrameView::PaintTitleBar(gfx::Canvas* canvas) {
  canvas->DrawStringInt(
      container_->widget_delegate()->GetWindowTitle(),
      *title_font_, GetTitleColor(), GetMirroredXForRect(title_bounds_),
      title_bounds_.y(), title_bounds_.width(), title_bounds_.height());
}

void ConstrainedWindowFrameView::PaintClientEdge(gfx::Canvas* canvas) {
  gfx::Rect client_edge_bounds(CalculateClientAreaBounds(width(), height()));
  client_edge_bounds.Inset(-kClientEdgeThickness, -kClientEdgeThickness);
  gfx::Rect frame_shadow_bounds(client_edge_bounds);
  frame_shadow_bounds.Inset(-kFrameShadowThickness, -kFrameShadowThickness);

  canvas->FillRect(frame_shadow_bounds, kContentsBorderShadow);
  canvas->FillRect(client_edge_bounds, views::kClientEdgeColor);
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
#if defined(OS_WIN) && !defined(USE_AURA)
  resources_.reset(ui::win::IsAeroGlassEnabled() ?
      static_cast<views::WindowResources*>(new VistaWindowResources) :
      new XPWindowResources);
#else
  // TODO(oshima): Use aura frame decoration.
  resources_.reset(new XPWindowResources);
#endif
}

// static
void ConstrainedWindowFrameView::InitClass() {
  static bool initialized = false;
  if (!initialized) {
#if defined(OS_WIN) && !defined(USE_AURA)
    title_font_ = new gfx::Font(views::NativeWidgetWin::GetWindowTitleFont());
#else
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    title_font_ = &rb.GetFont(ui::ResourceBundle::MediumFont);
#endif
    initialized = true;
  }
}

#if defined(USE_ASH)
// Ash has its own window frames, but we need the special close semantics for
// constrained windows.
class ConstrainedWindowFrameViewAsh : public ash::CustomFrameViewAsh {
 public:
  explicit ConstrainedWindowFrameViewAsh()
      : ash::CustomFrameViewAsh(),
        container_(NULL) {
  }

  void Init(views::Widget* container) {
    container_ = container;
    ash::CustomFrameViewAsh::Init(container);
    // Always use "active" look.
    SetInactiveRenderingDisabled(true);
  }

  // views::ButtonListener overrides:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE {
    if (sender == close_button())
      container_->Close();
  }

 private:
  views::Widget* container_;  // not owned
  DISALLOW_COPY_AND_ASSIGN(ConstrainedWindowFrameViewAsh);
};
#endif  // defined(USE_ASH)

ConstrainedWindowViews::ConstrainedWindowViews(
    gfx::NativeView parent,
    bool off_the_record,
    views::WidgetDelegate* widget_delegate)
    : off_the_record_(off_the_record) {
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.delegate = widget_delegate;
  params.child = true;

  params.parent = parent;

#if defined(USE_ASH)
  // Ash window headers can be transparent.
  params.transparent = true;
#endif

  Init(params);
}

ConstrainedWindowViews::~ConstrainedWindowViews() {
}

views::Widget* ConstrainedWindowViews::Create(
    content::WebContents* web_contents,
    views::WidgetDelegate* widget_delegate) {
  WebContentsModalDialogManager* manager =
      WebContentsModalDialogManager::FromWebContents(web_contents);
  views::Widget* dialog = new ConstrainedWindowViews(
      web_contents->GetView()->GetNativeView(),
      web_contents->GetBrowserContext()->IsOffTheRecord(),
      widget_delegate);
  manager->ShowDialog(dialog->GetNativeView());
  return dialog;
}

views::NonClientFrameView* ConstrainedWindowViews::CreateNonClientFrameView() {
  if (views::DialogDelegate::UseNewStyle())
    return views::DialogDelegate::CreateNewStyleFrameView(this);
#if defined(USE_ASH)
  ConstrainedWindowFrameViewAsh* frame = new ConstrainedWindowFrameViewAsh;
  frame->Init(this);
  return frame;
#endif
  return new ConstrainedWindowFrameView(this, off_the_record_);
}
