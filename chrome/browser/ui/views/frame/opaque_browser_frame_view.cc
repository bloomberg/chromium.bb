// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/opaque_browser_frame_view.h"

#include <algorithm>
#include <string>

#include "base/compiler_specific.h"
#include "base/profiler/scoped_tracker.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/opaque_browser_frame_view_layout.h"
#include "chrome/browser/ui/views/frame/opaque_browser_frame_view_platform_specific.h"
#include "chrome/browser/ui/views/profiles/avatar_menu_button.h"
#include "chrome/browser/ui/views/tab_icon_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "grit/components_strings.h"
#include "grit/theme_resources.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/path.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/resources/grit/views_resources.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/window/frame_background.h"
#include "ui/views/window/window_shape.h"

#if defined(OS_LINUX)
#include "ui/views/controls/menu/menu_runner.h"
#endif

using content::WebContents;

namespace {

// In the window corners, the resize areas don't actually expand bigger, but the
// 16 px at the end of each edge triggers diagonal resizing.
const int kResizeAreaCornerSize = 16;

#if !defined(OS_WIN)
// The icon never shrinks below 16 px on a side.
const int kIconMinimumSize = 16;
#endif

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// OpaqueBrowserFrameView, public:

OpaqueBrowserFrameView::OpaqueBrowserFrameView(BrowserFrame* frame,
                                               BrowserView* browser_view)
    : BrowserNonClientFrameView(frame, browser_view),
      layout_(new OpaqueBrowserFrameViewLayout(this)),
      minimize_button_(nullptr),
      maximize_button_(nullptr),
      restore_button_(nullptr),
      close_button_(nullptr),
      window_icon_(nullptr),
      window_title_(nullptr),
      frame_background_(new views::FrameBackground()) {
  SetLayoutManager(layout_);

  minimize_button_ = InitWindowCaptionButton(IDR_MINIMIZE,
                                             IDR_MINIMIZE_H,
                                             IDR_MINIMIZE_P,
                                             IDR_MINIMIZE_BUTTON_MASK,
                                             IDS_ACCNAME_MINIMIZE,
                                             VIEW_ID_MINIMIZE_BUTTON);
  maximize_button_ = InitWindowCaptionButton(IDR_MAXIMIZE,
                                             IDR_MAXIMIZE_H,
                                             IDR_MAXIMIZE_P,
                                             IDR_MAXIMIZE_BUTTON_MASK,
                                             IDS_ACCNAME_MAXIMIZE,
                                             VIEW_ID_MAXIMIZE_BUTTON);
  restore_button_ = InitWindowCaptionButton(IDR_RESTORE,
                                            IDR_RESTORE_H,
                                            IDR_RESTORE_P,
                                            IDR_RESTORE_BUTTON_MASK,
                                            IDS_ACCNAME_RESTORE,
                                            VIEW_ID_RESTORE_BUTTON);
  close_button_ = InitWindowCaptionButton(IDR_CLOSE,
                                          IDR_CLOSE_H,
                                          IDR_CLOSE_P,
                                          IDR_CLOSE_BUTTON_MASK,
                                          IDS_ACCNAME_CLOSE,
                                          VIEW_ID_CLOSE_BUTTON);

  // Initializing the TabIconView is expensive, so only do it if we need to.
  if (browser_view->ShouldShowWindowIcon()) {
    window_icon_ = new TabIconView(this, this);
    window_icon_->set_is_light(true);
    window_icon_->set_id(VIEW_ID_WINDOW_ICON);
    AddChildView(window_icon_);
    window_icon_->Update();
  }

  window_title_ = new views::Label(
      browser_view->GetWindowTitle(),
      gfx::FontList(BrowserFrame::GetTitleFontList()));
  window_title_->SetVisible(browser_view->ShouldShowWindowTitle());
  window_title_->SetEnabledColor(SK_ColorWHITE);
  window_title_->SetSubpixelRenderingEnabled(false);
  window_title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  window_title_->set_id(VIEW_ID_WINDOW_TITLE);
  AddChildView(window_title_);

  UpdateAvatar();

  platform_observer_.reset(OpaqueBrowserFrameViewPlatformSpecific::Create(
      this, layout_,
      ThemeServiceFactory::GetForProfile(browser_view->browser()->profile())));
}

OpaqueBrowserFrameView::~OpaqueBrowserFrameView() {
}

///////////////////////////////////////////////////////////////////////////////
// OpaqueBrowserFrameView, BrowserNonClientFrameView implementation:

gfx::Rect OpaqueBrowserFrameView::GetBoundsForTabStrip(
    views::View* tabstrip) const {
  if (!tabstrip)
    return gfx::Rect();

  return layout_->GetBoundsForTabStrip(tabstrip->GetPreferredSize(), width());
}

int OpaqueBrowserFrameView::GetTopInset(bool restored) const {
  return browser_view()->IsTabStripVisible() ?
      layout_->GetTabStripInsetsTop(restored) :
      layout_->NonClientTopBorderHeight(restored);
}

int OpaqueBrowserFrameView::GetThemeBackgroundXInset() const {
  return 0;
}

void OpaqueBrowserFrameView::UpdateThrobber(bool running) {
  if (window_icon_)
    window_icon_->Update();
}

gfx::Size OpaqueBrowserFrameView::GetMinimumSize() const {
  return layout_->GetMinimumSize(width());
}

///////////////////////////////////////////////////////////////////////////////
// OpaqueBrowserFrameView, views::NonClientFrameView implementation:

gfx::Rect OpaqueBrowserFrameView::GetBoundsForClientView() const {
  return layout_->client_view_bounds();
}

gfx::Rect OpaqueBrowserFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  return layout_->GetWindowBoundsForClientBounds(client_bounds);
}

bool OpaqueBrowserFrameView::IsWithinAvatarMenuButtons(
    const gfx::Point& point) const {
  if (avatar_button() &&
     avatar_button()->GetMirroredBounds().Contains(point)) {
    return true;
  }
#if defined(FRAME_AVATAR_BUTTON)
  if (new_avatar_button() &&
     new_avatar_button()->GetMirroredBounds().Contains(point)) {
    return true;
  }
#endif

  return false;
}

int OpaqueBrowserFrameView::NonClientHitTest(const gfx::Point& point) {
  if (!bounds().Contains(point))
    return HTNOWHERE;

  // See if the point is within the avatar menu button.
  if (IsWithinAvatarMenuButtons(point))
    return HTCLIENT;

  int frame_component = frame()->client_view()->NonClientHitTest(point);

  // See if we're in the sysmenu region.  We still have to check the tabstrip
  // first so that clicks in a tab don't get treated as sysmenu clicks.
  gfx::Rect sysmenu_rect(IconBounds());
  // In maximized mode we extend the rect to the screen corner to take advantage
  // of Fitts' Law.
  if (layout_->IsTitleBarCondensed())
    sysmenu_rect.SetRect(0, 0, sysmenu_rect.right(), sysmenu_rect.bottom());
  sysmenu_rect.set_x(GetMirroredXForRect(sysmenu_rect));
  if (sysmenu_rect.Contains(point))
    return (frame_component == HTCLIENT) ? HTCLIENT : HTSYSMENU;

  if (frame_component != HTNOWHERE)
    return frame_component;

  // Then see if the point is within any of the window controls.
  if (close_button_ && close_button_->visible() &&
      close_button_->GetMirroredBounds().Contains(point))
    return HTCLOSE;
  if (restore_button_ && restore_button_->visible() &&
      restore_button_->GetMirroredBounds().Contains(point))
    return HTMAXBUTTON;
  if (maximize_button_ && maximize_button_->visible() &&
      maximize_button_->GetMirroredBounds().Contains(point))
    return HTMAXBUTTON;
  if (minimize_button_ && minimize_button_->visible() &&
      minimize_button_->GetMirroredBounds().Contains(point))
    return HTMINBUTTON;

  views::WidgetDelegate* delegate = frame()->widget_delegate();
  if (!delegate) {
    LOG(WARNING) << "delegate is null, returning safe default.";
    return HTCAPTION;
  }
  int window_component = GetHTComponentForFrame(
      point, FrameBorderThickness(false), NonClientBorderThickness(),
      kResizeAreaCornerSize, kResizeAreaCornerSize, delegate->CanResize());
  // Fall back to the caption if no other component matches.
  return (window_component == HTNOWHERE) ? HTCAPTION : window_component;
}

void OpaqueBrowserFrameView::GetWindowMask(const gfx::Size& size,
                                           gfx::Path* window_mask) {
  DCHECK(window_mask);

  if (layout_->IsTitleBarCondensed() || frame()->IsFullscreen())
    return;

  views::GetDefaultWindowMask(
      size, frame()->GetCompositor()->device_scale_factor(), window_mask);
}

void OpaqueBrowserFrameView::ResetWindowControls() {
  restore_button_->SetState(views::CustomButton::STATE_NORMAL);
  minimize_button_->SetState(views::CustomButton::STATE_NORMAL);
  maximize_button_->SetState(views::CustomButton::STATE_NORMAL);
  // The close button isn't affected by this constraint.
}

void OpaqueBrowserFrameView::UpdateWindowIcon() {
  if (window_icon_)
    window_icon_->SchedulePaint();
}

void OpaqueBrowserFrameView::UpdateWindowTitle() {
  if (!frame()->IsFullscreen())
    window_title_->SchedulePaint();
}

void OpaqueBrowserFrameView::SizeConstraintsChanged() {
}

///////////////////////////////////////////////////////////////////////////////
// OpaqueBrowserFrameView, views::View overrides:

void OpaqueBrowserFrameView::GetAccessibleState(
    ui::AXViewState* state) {
  state->role = ui::AX_ROLE_TITLE_BAR;
}

///////////////////////////////////////////////////////////////////////////////
// OpaqueBrowserFrameView, views::ButtonListener implementation:

void OpaqueBrowserFrameView::ButtonPressed(views::Button* sender,
                                           const ui::Event& event) {
  if (sender == minimize_button_) {
    frame()->Minimize();
  } else if (sender == maximize_button_) {
    frame()->Maximize();
  } else if (sender == restore_button_) {
    frame()->Restore();
  } else if (sender == close_button_) {
    frame()->Close();
  }
}

void OpaqueBrowserFrameView::OnMenuButtonClicked(views::View* source,
                                                 const gfx::Point& point) {
#if defined(OS_LINUX)
  views::MenuRunner menu_runner(frame()->GetSystemMenuModel(),
                                views::MenuRunner::HAS_MNEMONICS);
  ignore_result(menu_runner.RunMenuAt(browser_view()->GetWidget(),
                                      window_icon_,
                                      window_icon_->GetBoundsInScreen(),
                                      views::MENU_ANCHOR_TOPLEFT,
                                      ui::MENU_SOURCE_MOUSE));
#endif
}

///////////////////////////////////////////////////////////////////////////////
// OpaqueBrowserFrameView, TabIconView::TabContentsProvider implementation:

bool OpaqueBrowserFrameView::ShouldTabIconViewAnimate() const {
  // This function is queried during the creation of the window as the
  // TabIconView we host is initialized, so we need to null check the selected
  // WebContents because in this condition there is not yet a selected tab.
  WebContents* current_tab = browser_view()->GetActiveWebContents();
  return current_tab ? current_tab->IsLoading() : false;
}

gfx::ImageSkia OpaqueBrowserFrameView::GetFaviconForTabIconView() {
  views::WidgetDelegate* delegate = frame()->widget_delegate();
  if (!delegate) {
    LOG(WARNING) << "delegate is null, returning safe default.";
    return gfx::ImageSkia();
  }
  return delegate->GetWindowIcon();
}

///////////////////////////////////////////////////////////////////////////////
// OpaqueBrowserFrameView, OpaqueBrowserFrameViewLayoutDelegate implementation:

bool OpaqueBrowserFrameView::ShouldShowWindowIcon() const {
  views::WidgetDelegate* delegate = frame()->widget_delegate();
  return ShouldShowWindowTitleBar() && delegate &&
         delegate->ShouldShowWindowIcon();
}

bool OpaqueBrowserFrameView::ShouldShowWindowTitle() const {
  // |delegate| may be null if called from callback of InputMethodChanged while
  // a window is being destroyed.
  // See more discussion at http://crosbug.com/8958
  views::WidgetDelegate* delegate = frame()->widget_delegate();
  return ShouldShowWindowTitleBar() && delegate &&
         delegate->ShouldShowWindowTitle();
}

base::string16 OpaqueBrowserFrameView::GetWindowTitle() const {
  return frame()->widget_delegate()->GetWindowTitle();
}

int OpaqueBrowserFrameView::GetIconSize() const {
#if defined(OS_WIN)
  // This metric scales up if either the titlebar height or the titlebar font
  // size are increased.
  return GetSystemMetrics(SM_CYSMICON);
#else
  return std::max(BrowserFrame::GetTitleFontList().GetHeight(),
                  kIconMinimumSize);
#endif
}

gfx::Size OpaqueBrowserFrameView::GetBrowserViewMinimumSize() const {
  return browser_view()->GetMinimumSize();
}

bool OpaqueBrowserFrameView::ShouldShowCaptionButtons() const {
  return ShouldShowWindowTitleBar();
}

bool OpaqueBrowserFrameView::ShouldShowAvatar() const {
  return browser_view()->ShouldShowAvatar();
}

bool OpaqueBrowserFrameView::IsRegularOrGuestSession() const {
  return browser_view()->IsRegularOrGuestSession();
}

gfx::ImageSkia OpaqueBrowserFrameView::GetOTRAvatarIcon() const {
  return browser_view()->GetOTRAvatarIcon();
}

bool OpaqueBrowserFrameView::IsMaximized() const {
  return frame()->IsMaximized();
}

bool OpaqueBrowserFrameView::IsMinimized() const {
  return frame()->IsMinimized();
}

bool OpaqueBrowserFrameView::IsFullscreen() const {
  return frame()->IsFullscreen();
}

bool OpaqueBrowserFrameView::IsTabStripVisible() const {
  return browser_view()->IsTabStripVisible();
}

bool OpaqueBrowserFrameView::IsToolbarVisible() const {
  return browser_view()->IsToolbarVisible() &&
      !browser_view()->toolbar()->GetPreferredSize().IsEmpty();
}

int OpaqueBrowserFrameView::GetTabStripHeight() const {
  return browser_view()->GetTabStripHeight();
}

gfx::Size OpaqueBrowserFrameView::GetTabstripPreferredSize() const {
  gfx::Size s = browser_view()->tabstrip()->GetPreferredSize();
  return s;
}

int OpaqueBrowserFrameView::GetToolbarLeadingCornerClientWidth() const {
  return browser_view()->GetToolbarBounds().x() -
      OpaqueBrowserFrameViewLayout::kContentEdgeShadowThickness +
      GetThemeProvider()->GetImageSkiaNamed(
          IDR_CONTENT_TOP_LEFT_CORNER)->width();
}

///////////////////////////////////////////////////////////////////////////////
// OpaqueBrowserFrameView, protected:

// views::View:
void OpaqueBrowserFrameView::OnPaint(gfx::Canvas* canvas) {
  if (frame()->IsFullscreen())
    return;  // Nothing is visible, so don't bother to paint.

  if (layout_->IsTitleBarCondensed())
    PaintMaximizedFrameBorder(canvas);
  else
    PaintRestoredFrameBorder(canvas);

  // The window icon and title are painted by their respective views.
  /* TODO(pkasting):  If this window is active, we should also draw a drop
   * shadow on the title.  This is tricky, because we don't want to hardcode a
   * shadow color (since we want to work with various themes), but we can't
   * alpha-blend either (since the Windows text APIs don't really do this).
   * So we'd need to sample the background color at the right location and
   * synthesize a good shadow color. */

  if (IsToolbarVisible() && IsTabStripVisible())
    PaintToolbarBackground(canvas);
  PaintClientEdge(canvas);
}

// BrowserNonClientFrameView:
bool OpaqueBrowserFrameView::ShouldPaintAsThemed() const {
  // Theme app and popup windows if |platform_observer_| wants it.
  return browser_view()->IsBrowserTypeNormal() ||
         platform_observer_->IsUsingSystemTheme();
}

void OpaqueBrowserFrameView::UpdateNewAvatarButtonImpl() {
#if defined(FRAME_AVATAR_BUTTON)
  UpdateNewAvatarButton(AvatarButtonStyle::THEMED);
#endif
}

///////////////////////////////////////////////////////////////////////////////
// OpaqueBrowserFrameView, private:

// views::NonClientFrameView:
bool OpaqueBrowserFrameView::DoesIntersectRect(const views::View* target,
                                               const gfx::Rect& rect) const {
  CHECK_EQ(target, this);
  if (!views::ViewTargeterDelegate::DoesIntersectRect(this, rect)) {
    // |rect| is outside OpaqueBrowserFrameView's bounds.
    return false;
  }

  // If the rect is outside the bounds of the client area, claim it.
  gfx::RectF rect_in_client_view_coords_f(rect);
  View::ConvertRectToTarget(this, frame()->client_view(),
      &rect_in_client_view_coords_f);
  gfx::Rect rect_in_client_view_coords = gfx::ToEnclosingRect(
      rect_in_client_view_coords_f);
  if (!frame()->client_view()->HitTestRect(rect_in_client_view_coords))
    return true;

  // Otherwise, claim |rect| only if it is above the bottom of the tabstrip in
  // a non-tab portion.
  TabStrip* tabstrip = browser_view()->tabstrip();
  if (!tabstrip || !browser_view()->IsTabStripVisible())
    return false;

  gfx::RectF rect_in_tabstrip_coords_f(rect);
  View::ConvertRectToTarget(this, tabstrip, &rect_in_tabstrip_coords_f);
  gfx::Rect rect_in_tabstrip_coords = gfx::ToEnclosingRect(
      rect_in_tabstrip_coords_f);
  if (rect_in_tabstrip_coords.bottom() > tabstrip->GetLocalBounds().bottom()) {
    // |rect| is below the tabstrip.
    return false;
  }

  if (tabstrip->HitTestRect(rect_in_tabstrip_coords)) {
    // Claim |rect| if it is in a non-tab portion of the tabstrip.
    return tabstrip->IsRectInWindowCaption(rect_in_tabstrip_coords);
  }

  // We claim |rect| because it is above the bottom of the tabstrip, but
  // not in the tabstrip itself. In particular, the avatar label/button is left
  // of the tabstrip and the window controls are right of the tabstrip.
  return true;
}

views::ImageButton* OpaqueBrowserFrameView::InitWindowCaptionButton(
    int normal_image_id,
    int hot_image_id,
    int pushed_image_id,
    int mask_image_id,
    int accessibility_string_id,
    ViewID view_id) {
  views::ImageButton* button = new views::ImageButton(this);
  const ui::ThemeProvider* tp = frame()->GetThemeProvider();
  button->SetImage(views::CustomButton::STATE_NORMAL,
                   tp->GetImageSkiaNamed(normal_image_id));
  button->SetImage(views::CustomButton::STATE_HOVERED,
                   tp->GetImageSkiaNamed(hot_image_id));
  button->SetImage(views::CustomButton::STATE_PRESSED,
                   tp->GetImageSkiaNamed(pushed_image_id));
  if (browser_view()->IsBrowserTypeNormal()) {
    button->SetBackground(
        tp->GetColor(ThemeProperties::COLOR_BUTTON_BACKGROUND),
        tp->GetImageSkiaNamed(IDR_THEME_WINDOW_CONTROL_BACKGROUND),
        tp->GetImageSkiaNamed(mask_image_id));
  }
  button->SetAccessibleName(
      l10n_util::GetStringUTF16(accessibility_string_id));
  button->set_id(view_id);
  AddChildView(button);
  return button;
}

int OpaqueBrowserFrameView::FrameBorderThickness(bool restored) const {
  return layout_->FrameBorderThickness(restored);
}

int OpaqueBrowserFrameView::NonClientBorderThickness() const {
  return layout_->NonClientBorderThickness();
}

gfx::Rect OpaqueBrowserFrameView::IconBounds() const {
  return layout_->IconBounds();
}

bool OpaqueBrowserFrameView::ShouldShowWindowTitleBar() const {
  // Do not show the custom title bar if the system title bar option is enabled.
  if (!frame()->UseCustomFrame())
    return false;

  // Do not show caption buttons if the window manager is forcefully providing a
  // title bar (e.g., in Ubuntu Unity, if the window is maximized).
  if (!views::ViewsDelegate::GetInstance())
    return true;
  return !views::ViewsDelegate::GetInstance()->WindowManagerProvidesTitleBar(
      IsMaximized());
}

int OpaqueBrowserFrameView::GetTopAreaHeight() const {
  gfx::ImageSkia* frame_image = GetFrameImage();
  int top_area_height = frame_image->height();
  if (browser_view()->IsTabStripVisible()) {
    top_area_height =
        std::max(top_area_height,
                 GetBoundsForTabStrip(browser_view()->tabstrip()).bottom());
  }
  return top_area_height;
}

void OpaqueBrowserFrameView::PaintRestoredFrameBorder(
    gfx::Canvas* canvas) const {
  frame_background_->set_frame_color(GetFrameColor());
  frame_background_->set_theme_image(GetFrameImage());
  frame_background_->set_theme_overlay_image(GetFrameOverlayImage());
  frame_background_->set_top_area_height(GetTopAreaHeight());

  const ui::ThemeProvider* tp = GetThemeProvider();
  frame_background_->SetSideImages(
      tp->GetImageSkiaNamed(IDR_WINDOW_LEFT_SIDE),
      tp->GetImageSkiaNamed(IDR_WINDOW_TOP_CENTER),
      tp->GetImageSkiaNamed(IDR_WINDOW_RIGHT_SIDE),
      tp->GetImageSkiaNamed(IDR_WINDOW_BOTTOM_CENTER));
  frame_background_->SetCornerImages(
      tp->GetImageSkiaNamed(IDR_WINDOW_TOP_LEFT_CORNER),
      tp->GetImageSkiaNamed(IDR_WINDOW_TOP_RIGHT_CORNER),
      tp->GetImageSkiaNamed(IDR_WINDOW_BOTTOM_LEFT_CORNER),
      tp->GetImageSkiaNamed(IDR_WINDOW_BOTTOM_RIGHT_CORNER));
  frame_background_->PaintRestored(canvas, this);

  // Note: When we don't have a toolbar, we need to draw some kind of bottom
  // edge here.  Because the App Window graphics we use for this have an
  // attached client edge and their sizing algorithm is a little involved, we do
  // all this in PaintRestoredClientEdge().
}

void OpaqueBrowserFrameView::PaintMaximizedFrameBorder(
    gfx::Canvas* canvas) const {
  frame_background_->set_frame_color(GetFrameColor());
  frame_background_->set_theme_image(GetFrameImage());
  frame_background_->set_theme_overlay_image(GetFrameOverlayImage());
  frame_background_->set_top_area_height(GetTopAreaHeight());
  frame_background_->set_maximized_top_inset(
      GetTopInset(true) - GetTopInset(false));
  frame_background_->PaintMaximized(canvas, this);
}

void OpaqueBrowserFrameView::PaintToolbarBackground(gfx::Canvas* canvas) const {
  gfx::Rect toolbar_bounds(browser_view()->GetToolbarBounds());
  if (toolbar_bounds.IsEmpty())
    return;
  gfx::Point toolbar_origin(toolbar_bounds.origin());
  ConvertPointToTarget(browser_view(), this, &toolbar_origin);
  toolbar_bounds.set_origin(toolbar_origin);

  const ui::ThemeProvider* tp = GetThemeProvider();
  gfx::ImageSkia* bg = tp->GetImageSkiaNamed(IDR_THEME_TOOLBAR);
  int x = toolbar_bounds.x();
  const int y = toolbar_bounds.y();
  const int bg_y =
      GetTopInset(false) + Tab::GetYInsetForActiveTabBackground();
  const int w = toolbar_bounds.width();
  const int h = toolbar_bounds.height();
  const SkColor separator_color =
      tp->GetColor(ThemeProperties::COLOR_TOOLBAR_BOTTOM_SEPARATOR);
  if (ui::MaterialDesignController::IsModeMaterial()) {
    // Background.  The top stroke is drawn above the toolbar bounds, so
    // unlike in the non-Material Design code below, we don't need to exclude
    // any region from having the background image drawn over it.
    if (tp->HasCustomImage(IDR_THEME_TOOLBAR)) {
      canvas->TileImageInt(*bg, x + GetThemeBackgroundXInset(), y - bg_y, x, y,
                           w, h);
    } else {
      canvas->FillRect(toolbar_bounds,
                       tp->GetColor(ThemeProperties::COLOR_TOOLBAR));
    }

    // Material Design has no corners to mask out.

    // Top stroke.  For Material Design, the toolbar has no side strokes.
    gfx::Rect separator_rect(x, y, w, 0);
    gfx::ScopedCanvas scoped_canvas(canvas);
    gfx::Rect tabstrip_bounds(GetBoundsForTabStrip(browser_view()->tabstrip()));
    tabstrip_bounds.set_x(GetMirroredXForRect(tabstrip_bounds));
    canvas->sk_canvas()->clipRect(gfx::RectToSkRect(tabstrip_bounds),
                                  SkRegion::kDifference_Op);
    separator_rect.set_y(tabstrip_bounds.bottom());
    BrowserView::Paint1pxHorizontalLine(
        canvas, tp->GetColor(ThemeProperties::COLOR_TOOLBAR_TOP_SEPARATOR),
        separator_rect, true);

    // Toolbar/content separator.
    toolbar_bounds.Inset(kClientEdgeThickness, 0);
    BrowserView::Paint1pxHorizontalLine(canvas, separator_color, toolbar_bounds,
                                        true);
  } else {
    const int kContentEdgeShadowThickness =
        OpaqueBrowserFrameViewLayout::kContentEdgeShadowThickness;

    // Background.  We need to create a separate layer so we can mask off the
    // corners before compositing onto the frame.
    canvas->sk_canvas()->saveLayer(
        gfx::RectToSkRect(gfx::Rect(x - kContentEdgeShadowThickness, y,
                                    w + kContentEdgeShadowThickness * 2, h)),
        nullptr);

    // The top stroke is drawn using the IDR_CONTENT_TOP_XXX images, which
    // overlay the toolbar.  The top 2 px of these images is the actual top
    // stroke + shadow, and is partly transparent, so the toolbar background
    // shouldn't be drawn over it.
    const int bg_dest_y = y + kContentEdgeShadowThickness;
    canvas->TileImageInt(*bg, x + GetThemeBackgroundXInset(), bg_dest_y - bg_y,
                         x, bg_dest_y, w, h - kContentEdgeShadowThickness);

    // Mask out the corners.
    gfx::ImageSkia* left = tp->GetImageSkiaNamed(IDR_CONTENT_TOP_LEFT_CORNER);
    const int img_w = left->width();
    x -= kContentEdgeShadowThickness;
    SkPaint paint;
    paint.setXfermodeMode(SkXfermode::kDstIn_Mode);
    canvas->DrawImageInt(
        *tp->GetImageSkiaNamed(IDR_CONTENT_TOP_LEFT_CORNER_MASK), 0, 0, img_w,
        h, x, y, img_w, h, false, paint);
    const int right_x =
        toolbar_bounds.right() + kContentEdgeShadowThickness - img_w;
    canvas->DrawImageInt(
        *tp->GetImageSkiaNamed(IDR_CONTENT_TOP_RIGHT_CORNER_MASK), 0, 0, img_w,
        h, right_x, y, img_w, h, false, paint);
    canvas->Restore();

    // Corner and side strokes.
    canvas->DrawImageInt(*left, 0, 0, img_w, h, x, y, img_w, h, false);
    canvas->DrawImageInt(*tp->GetImageSkiaNamed(IDR_CONTENT_TOP_RIGHT_CORNER),
                         0, 0, img_w, h, right_x, y, img_w, h, false);

    // Top stroke.
    x += img_w;
    canvas->TileImageInt(*tp->GetImageSkiaNamed(IDR_CONTENT_TOP_CENTER), x, y,
                         right_x - x, kContentEdgeShadowThickness);

    // Toolbar/content separator.
    toolbar_bounds.Inset(kClientEdgeThickness, h - kClientEdgeThickness,
                         kClientEdgeThickness, 0);
    canvas->FillRect(toolbar_bounds, separator_color);
  }
}

void OpaqueBrowserFrameView::PaintClientEdge(gfx::Canvas* canvas) const {
  gfx::Rect client_bounds =
      layout_->CalculateClientAreaBounds(width(), height());
  const int x = client_bounds.x();
  int y = client_bounds.y();
  const int w = client_bounds.width();
  const int right = client_bounds.right();

  const bool tabstrip_visible = browser_view()->IsTabStripVisible();
  SkColor toolbar_color;
  const ui::ThemeProvider* tp = GetThemeProvider();
  const bool md = ui::MaterialDesignController::IsModeMaterial();
  const gfx::Rect toolbar_bounds(browser_view()->GetToolbarBounds());
  const bool incognito = browser_view()->IsOffTheRecord();
  const bool toolbar_visible = IsToolbarVisible();
  int img_y_offset = 0;
  if (tabstrip_visible) {
    toolbar_color = tp->GetColor(ThemeProperties::COLOR_TOOLBAR);

    // Pre-Material Design, the client edge images start below the toolbar.  In
    // MD the client edge images start at the top of the toolbar.
    y += md ? toolbar_bounds.y() : toolbar_bounds.bottom();
  } else {
    toolbar_color = ThemeProperties::GetDefaultColor(
        ThemeProperties::COLOR_TOOLBAR, incognito);

    // The toolbar isn't going to draw a top edge for us, so draw one ourselves.
    if (md) {
      client_bounds.Inset(-kClientEdgeThickness, -1, -kClientEdgeThickness,
                          client_bounds.height());

      // Shadow.
      BrowserView::Paint1pxHorizontalLine(
          canvas, tp->GetColor(ThemeProperties::COLOR_TOOLBAR_TOP_SEPARATOR),
          client_bounds, true);
    } else {
      // Ensure the client edge rects are drawn to the top of the location bar.
      img_y_offset = kClientEdgeThickness;

      // Shadow.
      gfx::ImageSkia* top_left = tp->GetImageSkiaNamed(IDR_APP_TOP_LEFT);
      const int img_w = top_left->width();
      const int height = top_left->height();
      const int top_y = y + img_y_offset - height;
      canvas->DrawImageInt(*top_left, 0, 0, img_w, height, x - img_w, top_y,
                           img_w, height, false);
      canvas->TileImageInt(*tp->GetImageSkiaNamed(IDR_APP_TOP_CENTER), 0, 0, x,
                           top_y, w, height);
      canvas->DrawImageInt(*tp->GetImageSkiaNamed(IDR_APP_TOP_RIGHT), 0, 0,
                           img_w, height, right, top_y, img_w, height, false);
    }
  }

  // In maximized mode, the only edge to draw is the top one, so we're done.
  if (layout_->IsTitleBarCondensed())
    return;

  const int img_y = y + img_y_offset;
  const int bottom = std::max(y, height() - NonClientBorderThickness());
  const int height = bottom - y;
  const int img_h = bottom - img_y;

  // Draw the client edge images.  For non-MD, we fill the toolbar color
  // underneath these images so they will lighten/darken it appropriately to
  // create a "3D shaded" effect.  For MD, where we want a flatter appearance,
  // we do the filling afterwards so the user sees the unmodified toolbar color.
  if (!md)
    FillClientEdgeRects(x, y, w, height, true, toolbar_color, canvas);
  gfx::ImageSkia* right_image = tp->GetImageSkiaNamed(IDR_CONTENT_RIGHT_SIDE);
  const int img_w = right_image->width();
  canvas->TileImageInt(*right_image, right, img_y, img_w, img_h);
  canvas->DrawImageInt(*tp->GetImageSkiaNamed(IDR_CONTENT_BOTTOM_RIGHT_CORNER),
                       right, bottom);
  gfx::ImageSkia* bottom_image =
      tp->GetImageSkiaNamed(IDR_CONTENT_BOTTOM_CENTER);
  canvas->TileImageInt(*bottom_image, x, bottom, w, bottom_image->height());
  canvas->DrawImageInt(*tp->GetImageSkiaNamed(IDR_CONTENT_BOTTOM_LEFT_CORNER),
                       x - img_w, bottom);
  canvas->TileImageInt(*tp->GetImageSkiaNamed(IDR_CONTENT_LEFT_SIDE), x - img_w,
                       img_y, img_w, img_h);
  if (md)
    FillClientEdgeRects(x, y, w, height, true, toolbar_color, canvas);


  // For popup windows, draw location bar sides.
  if (!tabstrip_visible && toolbar_visible) {
    FillClientEdgeRects(
        x, y, w, toolbar_bounds.height(), false,
        LocationBarView::GetBorderColor(incognito), canvas);
  }
}

void OpaqueBrowserFrameView::FillClientEdgeRects(int x,
                                                 int y,
                                                 int w,
                                                 int h,
                                                 bool draw_bottom,
                                                 SkColor color,
                                                 gfx::Canvas* canvas) const {
  x -= kClientEdgeThickness;
  gfx::Rect side(x, y, kClientEdgeThickness, h);
  canvas->FillRect(side, color);
  if (draw_bottom) {
    canvas->FillRect(gfx::Rect(x, y + h, w + (2 * kClientEdgeThickness),
                               kClientEdgeThickness),
                     color);
  }
  side.Offset(w + kClientEdgeThickness, 0);
  canvas->FillRect(side, color);
}
