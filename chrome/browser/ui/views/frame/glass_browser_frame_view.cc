// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/glass_browser_frame_view.h"

#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_header_helper.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/profiles/avatar_menu_button.h"
#include "chrome/browser/ui/views/profiles/new_avatar_button.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/pref_names.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "content/public/browser/notification_service.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/ui_resources.h"
#include "ui/base/resource/resource_bundle_win.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/icon_util.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/win/dpi.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/win/hwnd_util.h"
#include "ui/views/window/client_view.h"

HICON GlassBrowserFrameView::throbber_icons_[
    GlassBrowserFrameView::kThrobberIconCount];

namespace {
// There are 3 px of client edge drawn inside the outer frame borders.
const int kNonClientBorderThickness = 3;
// Besides the frame border, there's another 9 px of empty space atop the
// window in restored mode, to use to drag the window around.
const int kNonClientRestoredExtraThickness = 9;
// In the window corners, the resize areas don't actually expand bigger, but the
// 16 px at the end of the top and bottom edges triggers diagonal resizing.
const int kResizeAreaCornerSize = 16;
// The avatar ends 2 px above the bottom of the tabstrip (which, given the
// way the tabstrip draws its bottom edge, will appear like a 1 px gap to the
// user).
const int kAvatarBottomSpacing = 2;
// Space between the frame border and the left edge of the avatar.
const int kAvatarLeftSpacing = 2;
// Space between the right edge of the avatar and the tabstrip.
const int kAvatarRightSpacing = -2;
// How far the new avatar button is from the left of the minimize button.
const int kNewAvatarButtonOffset = 5;
// The content left/right images have a shadow built into them.
const int kContentEdgeShadowThickness = 2;
// The top 3 px of the tabstrip is shadow; in maximized mode we push this off
// the top of the screen so the tabs appear flush against the screen edge.
const int kTabstripTopShadowThickness = 3;
// In restored mode, the New Tab button isn't at the same height as the caption
// buttons, but the space will look cluttered if it actually slides under them,
// so we stop it when the gap between the two is down to 5 px.
const int kNewTabCaptionRestoredSpacing = 5;
// In maximized mode, where the New Tab button and the caption buttons are at
// similar vertical coordinates, we need to reserve a larger, 16 px gap to avoid
// looking too cluttered.
const int kNewTabCaptionMaximizedSpacing = 16;
// How far to indent the tabstrip from the left side of the screen when there
// is no avatar icon.
const int kTabStripIndent = -6;

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// GlassBrowserFrameView, public:

GlassBrowserFrameView::GlassBrowserFrameView(BrowserFrame* frame,
                                             BrowserView* browser_view)
    : BrowserNonClientFrameView(frame, browser_view),
      throbber_running_(false),
      throbber_frame_(0) {
  if (browser_view->ShouldShowWindowIcon())
    InitThrobberIcons();

  if (browser_view->IsRegularOrGuestSession() && switches::IsNewAvatarMenu())
    UpdateNewStyleAvatarInfo(this, NewAvatarButton::NATIVE_BUTTON);
  else
    UpdateAvatarInfo();

  if (!browser_view->IsOffTheRecord()) {
    registrar_.Add(this, chrome::NOTIFICATION_PROFILE_CACHED_INFO_CHANGED,
                   content::NotificationService::AllSources());
  }
}

GlassBrowserFrameView::~GlassBrowserFrameView() {
}

///////////////////////////////////////////////////////////////////////////////
// GlassBrowserFrameView, BrowserNonClientFrameView implementation:

gfx::Rect GlassBrowserFrameView::GetBoundsForTabStrip(
    views::View* tabstrip) const {
  int minimize_button_offset =
      std::min(frame()->GetMinimizeButtonOffset(), width());

  // The new avatar button is optionally displayed to the left of the
  // minimize button.
  if (new_avatar_button()) {
    DCHECK(switches::IsNewAvatarMenu());
    minimize_button_offset -= new_avatar_button()->width();
  }

  int tabstrip_x = browser_view()->ShouldShowAvatar() ?
      (avatar_bounds_.right() + kAvatarRightSpacing) :
      NonClientBorderThickness() + kTabStripIndent;
  // In RTL languages, we have moved an avatar icon left by the size of window
  // controls to prevent it from being rendered over them. So, we use its x
  // position to move this tab strip left when maximized. Also, we can render
  // a tab strip until the left end of this window without considering the size
  // of window controls in RTL languages.
  if (base::i18n::IsRTL()) {
    if (!browser_view()->ShouldShowAvatar() && frame()->IsMaximized()) {
      tabstrip_x += avatar_bounds_.x();
    } else if (browser_view()->IsRegularOrGuestSession() &&
               switches::IsNewAvatarMenu()) {
      tabstrip_x = width() - minimize_button_offset;
    }

    minimize_button_offset = width();
  }
  int tabstrip_width = minimize_button_offset - tabstrip_x -
      (frame()->IsMaximized() ?
          kNewTabCaptionMaximizedSpacing : kNewTabCaptionRestoredSpacing);
  return gfx::Rect(tabstrip_x, NonClientTopBorderHeight(),
                   std::max(0, tabstrip_width),
                   tabstrip->GetPreferredSize().height());
}

int GlassBrowserFrameView::GetTopInset() const {
  return GetClientAreaInsets().top();
}

int GlassBrowserFrameView::GetThemeBackgroundXInset() const {
  return 0;
}

void GlassBrowserFrameView::UpdateThrobber(bool running) {
  if (throbber_running_) {
    if (running) {
      DisplayNextThrobberFrame();
    } else {
      StopThrobber();
    }
  } else if (running) {
    StartThrobber();
  }
}

gfx::Size GlassBrowserFrameView::GetMinimumSize() const {
  gfx::Size min_size(browser_view()->GetMinimumSize());

  // Account for the client area insets.
  gfx::Insets insets = GetClientAreaInsets();
  min_size.Enlarge(insets.width(), insets.height());
  // Client area insets do not include the shadow thickness.
  min_size.Enlarge(2 * kContentEdgeShadowThickness, 0);

  // Ensure that the minimum width is enough to hold a tab strip with minimum
  // width at its usual insets.
  if (browser_view()->IsTabStripVisible()) {
    TabStrip* tabstrip = browser_view()->tabstrip();
    int min_tabstrip_width = tabstrip->GetMinimumSize().width();
    int min_tabstrip_area_width =
        width() - GetBoundsForTabStrip(tabstrip).width() + min_tabstrip_width;
    min_size.set_width(std::max(min_tabstrip_area_width, min_size.width()));
  }

  return min_size;
}

///////////////////////////////////////////////////////////////////////////////
// GlassBrowserFrameView, views::NonClientFrameView implementation:

gfx::Rect GlassBrowserFrameView::GetBoundsForClientView() const {
  return client_view_bounds_;
}

gfx::Rect GlassBrowserFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  HWND hwnd = views::HWNDForWidget(frame());
  if (!browser_view()->IsTabStripVisible() && hwnd) {
    // If we don't have a tabstrip, we're either a popup or an app window, in
    // which case we have a standard size non-client area and can just use
    // AdjustWindowRectEx to obtain it. We check for a non-NULL window handle in
    // case this gets called before the window is actually created.
    RECT rect = client_bounds.ToRECT();
    AdjustWindowRectEx(&rect, GetWindowLong(hwnd, GWL_STYLE), FALSE,
                       GetWindowLong(hwnd, GWL_EXSTYLE));
    return gfx::Rect(rect);
  }

  gfx::Insets insets = GetClientAreaInsets();
  return gfx::Rect(std::max(0, client_bounds.x() - insets.left()),
                   std::max(0, client_bounds.y() - insets.top()),
                   client_bounds.width() + insets.width(),
                   client_bounds.height() + insets.height());
}

int GlassBrowserFrameView::NonClientHitTest(const gfx::Point& point) {
  // If the browser isn't in normal mode, we haven't customized the frame, so
  // Windows can figure this out.  If the point isn't within our bounds, then
  // it's in the native portion of the frame, so again Windows can figure it
  // out.
  if (!browser_view()->IsBrowserTypeNormal() || !bounds().Contains(point))
    return HTNOWHERE;

  // See if the point is within the avatar menu button or within the avatar
  // label.
  if (avatar_button() && avatar_button()->GetMirroredBounds().Contains(point))
    return HTCLIENT;

  if (new_avatar_button() &&
     new_avatar_button()->GetMirroredBounds().Contains(point))
   return HTCLIENT;

  int frame_component = frame()->client_view()->NonClientHitTest(point);

  // See if we're in the sysmenu region.  We still have to check the tabstrip
  // first so that clicks in a tab don't get treated as sysmenu clicks.
  int nonclient_border_thickness = NonClientBorderThickness();
  if (gfx::Rect(nonclient_border_thickness,
                gfx::win::GetSystemMetricsInDIP(SM_CXSIZEFRAME),
                gfx::win::GetSystemMetricsInDIP(SM_CXSMICON),
                gfx::win::GetSystemMetricsInDIP(SM_CYSMICON)).Contains(point))
    return (frame_component == HTCLIENT) ? HTCLIENT : HTSYSMENU;

  if (frame_component != HTNOWHERE)
    return frame_component;

  int frame_border_thickness = FrameBorderThickness();
  int window_component = GetHTComponentForFrame(point, frame_border_thickness,
      nonclient_border_thickness, frame_border_thickness,
      kResizeAreaCornerSize - frame_border_thickness,
      frame()->widget_delegate()->CanResize());
  // Fall back to the caption if no other component matches.
  return (window_component == HTNOWHERE) ? HTCAPTION : window_component;
}

///////////////////////////////////////////////////////////////////////////////
// GlassBrowserFrameView, views::View overrides:

void GlassBrowserFrameView::OnPaint(gfx::Canvas* canvas) {
  if (browser_view()->IsToolbarVisible() &&
      browser_view()->toolbar()->ShouldPaintBackground())
    PaintToolbarBackground(canvas);
  if (!frame()->IsMaximized())
    PaintRestoredClientEdge(canvas);
}

void GlassBrowserFrameView::Layout() {
  if (browser_view()->IsRegularOrGuestSession() && switches::IsNewAvatarMenu())
    LayoutNewStyleAvatar();
  else
    LayoutAvatar();

  LayoutClientView();
}

///////////////////////////////////////////////////////////////////////////////
// GlassBrowserFrameView, views::ButtonListener overrides:
void GlassBrowserFrameView::ButtonPressed(views::Button* sender,
                                          const ui::Event& event) {
  if (sender == new_avatar_button()) {
    browser_view()->ShowAvatarBubbleFromAvatarButton(
        BrowserWindow::AVATAR_BUBBLE_MODE_DEFAULT,
        signin::ManageAccountsParams());
  }
}

///////////////////////////////////////////////////////////////////////////////
// GlassBrowserFrameView, private:

// views::NonClientFrameView:
bool GlassBrowserFrameView::DoesIntersectRect(const views::View* target,
                                              const gfx::Rect& rect) const {
  CHECK_EQ(target, this);
  bool hit_avatar_button = avatar_button() &&
      avatar_button()->GetMirroredBounds().Intersects(rect);
  bool hit_new_avatar_button = new_avatar_button() &&
      new_avatar_button()->GetMirroredBounds().Intersects(rect);
  return hit_avatar_button || hit_new_avatar_button ||
         !frame()->client_view()->bounds().Intersects(rect);
}

int GlassBrowserFrameView::FrameBorderThickness() const {
  return (frame()->IsMaximized() || frame()->IsFullscreen()) ?
      0 : gfx::win::GetSystemMetricsInDIP(SM_CXSIZEFRAME);
}

int GlassBrowserFrameView::NonClientBorderThickness() const {
  if (frame()->IsMaximized() || frame()->IsFullscreen())
    return 0;

  return kNonClientBorderThickness;
}

int GlassBrowserFrameView::NonClientTopBorderHeight() const {
  if (frame()->IsFullscreen())
    return 0;

  // We'd like to use FrameBorderThickness() here, but the maximized Aero glass
  // frame has a 0 frame border around most edges and a CYSIZEFRAME-thick border
  // at the top (see AeroGlassFrame::OnGetMinMaxInfo()).
  return gfx::win::GetSystemMetricsInDIP(SM_CYSIZEFRAME) +
      (!frame()->ShouldLeaveOffsetNearTopBorder() ?
      -kTabstripTopShadowThickness : kNonClientRestoredExtraThickness);
}

void GlassBrowserFrameView::PaintToolbarBackground(gfx::Canvas* canvas) {
  ui::ThemeProvider* tp = GetThemeProvider();

  gfx::Rect toolbar_bounds(browser_view()->GetToolbarBounds());
  gfx::Point toolbar_origin(toolbar_bounds.origin());
  View::ConvertPointToTarget(browser_view(), this, &toolbar_origin);
  toolbar_bounds.set_origin(toolbar_origin);
  int x = toolbar_bounds.x();
  int w = toolbar_bounds.width();
  int left_x = x - kContentEdgeShadowThickness;

  gfx::ImageSkia* theme_toolbar = tp->GetImageSkiaNamed(IDR_THEME_TOOLBAR);
  gfx::ImageSkia* toolbar_left = tp->GetImageSkiaNamed(
      IDR_CONTENT_TOP_LEFT_CORNER);
  gfx::ImageSkia* toolbar_center = tp->GetImageSkiaNamed(
      IDR_CONTENT_TOP_CENTER);

  // Tile the toolbar image starting at the frame edge on the left and where
  // the tabstrip is on the top.
  int y = toolbar_bounds.y();
  int dest_y = browser_view()->IsTabStripVisible()
                   ? y + (kFrameShadowThickness * 2)
                   : y;
  canvas->TileImageInt(*theme_toolbar,
                       x + GetThemeBackgroundXInset(),
                       dest_y - GetTopInset(), x,
                       dest_y, w, theme_toolbar->height());

  if (browser_view()->IsTabStripVisible()) {
    // Draw rounded corners for the tab.
    gfx::ImageSkia* toolbar_left_mask =
        tp->GetImageSkiaNamed(IDR_CONTENT_TOP_LEFT_CORNER_MASK);
    gfx::ImageSkia* toolbar_right_mask =
        tp->GetImageSkiaNamed(IDR_CONTENT_TOP_RIGHT_CORNER_MASK);

    // We mask out the corners by using the DestinationIn transfer mode,
    // which keeps the RGB pixels from the destination and the alpha from
    // the source.
    SkPaint paint;
    paint.setXfermodeMode(SkXfermode::kDstIn_Mode);

    // Mask out the top left corner.
    canvas->DrawImageInt(*toolbar_left_mask, left_x, y, paint);

    // Mask out the top right corner.
    int right_x =
        x + w + kContentEdgeShadowThickness - toolbar_right_mask->width();
    canvas->DrawImageInt(*toolbar_right_mask, right_x, y, paint);

    // Draw left edge.
    canvas->DrawImageInt(*toolbar_left, left_x, y);

    // Draw center edge.
    canvas->TileImageInt(*toolbar_center, left_x + toolbar_left->width(), y,
        right_x - (left_x + toolbar_left->width()), toolbar_center->height());

    // Right edge.
    canvas->DrawImageInt(*tp->GetImageSkiaNamed(IDR_CONTENT_TOP_RIGHT_CORNER),
                         right_x, y);
  }

  // Draw the content/toolbar separator.
  canvas->FillRect(
      gfx::Rect(x + kClientEdgeThickness,
                toolbar_bounds.bottom() - kClientEdgeThickness,
                w - (2 * kClientEdgeThickness),
                kClientEdgeThickness),
      ThemeProperties::GetDefaultColor(
          ThemeProperties::COLOR_TOOLBAR_SEPARATOR));
}

void GlassBrowserFrameView::PaintRestoredClientEdge(gfx::Canvas* canvas) {
  ui::ThemeProvider* tp = GetThemeProvider();
  gfx::Rect client_area_bounds = CalculateClientAreaBounds(width(), height());

  // The client edges start below the toolbar upper corner images regardless
  // of how tall the toolbar itself is.
  int client_area_top = frame()->client_view()->y() +
      browser_view()->GetToolbarBounds().y() +
      tp->GetImageSkiaNamed(IDR_CONTENT_TOP_LEFT_CORNER)->height();
  int client_area_bottom =
      std::max(client_area_top, height() - NonClientBorderThickness());
  int client_area_height = client_area_bottom - client_area_top;

  // Draw the client edge images.
  gfx::ImageSkia* right = tp->GetImageSkiaNamed(IDR_CONTENT_RIGHT_SIDE);
  canvas->TileImageInt(*right, client_area_bounds.right(), client_area_top,
                       right->width(), client_area_height);
  canvas->DrawImageInt(
      *tp->GetImageSkiaNamed(IDR_CONTENT_BOTTOM_RIGHT_CORNER),
      client_area_bounds.right(), client_area_bottom);
  gfx::ImageSkia* bottom = tp->GetImageSkiaNamed(IDR_CONTENT_BOTTOM_CENTER);
  canvas->TileImageInt(*bottom, client_area_bounds.x(),
      client_area_bottom, client_area_bounds.width(),
      bottom->height());
  gfx::ImageSkia* bottom_left =
      tp->GetImageSkiaNamed(IDR_CONTENT_BOTTOM_LEFT_CORNER);
  canvas->DrawImageInt(*bottom_left,
      client_area_bounds.x() - bottom_left->width(), client_area_bottom);
  gfx::ImageSkia* left = tp->GetImageSkiaNamed(IDR_CONTENT_LEFT_SIDE);
  canvas->TileImageInt(*left, client_area_bounds.x() - left->width(),
      client_area_top, left->width(), client_area_height);

  // Draw the toolbar color so that the client edges show the right color even
  // where not covered by the toolbar image.  NOTE: We do this after drawing the
  // images because the images are meant to alpha-blend atop the frame whereas
  // these rects are meant to be fully opaque, without anything overlaid.
  SkColor toolbar_color = tp->GetColor(ThemeProperties::COLOR_TOOLBAR);
  canvas->FillRect(gfx::Rect(client_area_bounds.x() - kClientEdgeThickness,
      client_area_top, kClientEdgeThickness,
      client_area_bottom + kClientEdgeThickness - client_area_top),
      toolbar_color);
  canvas->FillRect(gfx::Rect(client_area_bounds.x(), client_area_bottom,
                             client_area_bounds.width(), kClientEdgeThickness),
                   toolbar_color);
  canvas->FillRect(gfx::Rect(client_area_bounds.right(), client_area_top,
       kClientEdgeThickness,
       client_area_bottom + kClientEdgeThickness - client_area_top),
       toolbar_color);
}

void GlassBrowserFrameView::LayoutNewStyleAvatar() {
  DCHECK(switches::IsNewAvatarMenu());
  if (!new_avatar_button())
    return;

  gfx::Size label_size = new_avatar_button()->GetPreferredSize();

  int button_x = frame()->GetMinimizeButtonOffset() -
      kNewAvatarButtonOffset - label_size.width();
  if (base::i18n::IsRTL())
    button_x = width() - frame()->GetMinimizeButtonOffset() +
        kNewAvatarButtonOffset;

  // We need to offset the button correctly in maximized mode, so that the
  // custom glass style aligns with the native control glass style. The
  // glass shadow is off by 1px, which was determined by visual inspection.
  int button_y = !frame()->IsMaximized() ? 1 :
      NonClientTopBorderHeight() + kTabstripTopShadowThickness - 1;

  new_avatar_button()->SetBounds(
      button_x,
      button_y,
      label_size.width(),
      gfx::win::GetSystemMetricsInDIP(SM_CYMENUSIZE) + 1);
}

void GlassBrowserFrameView::LayoutAvatar() {
  // Even though the avatar is used for both incognito and profiles we always
  // use the incognito icon to layout the avatar button. The profile icon
  // can be customized so we can't depend on its size to perform layout.
  gfx::ImageSkia incognito_icon = browser_view()->GetOTRAvatarIcon();

  int avatar_x = NonClientBorderThickness() + kAvatarLeftSpacing;
  // Move this avatar icon by the size of window controls to prevent it from
  // being rendered over them in RTL languages. This code also needs to adjust
  // the width of a tab strip to avoid decreasing this size twice. (See the
  // comment in GetBoundsForTabStrip().)
  if (base::i18n::IsRTL())
    avatar_x += width() - frame()->GetMinimizeButtonOffset();

  int avatar_bottom = GetTopInset() +
      browser_view()->GetTabStripHeight() - kAvatarBottomSpacing;
  int avatar_restored_y = avatar_bottom - incognito_icon.height();
  int avatar_y = frame()->IsMaximized() ?
      (NonClientTopBorderHeight() + kTabstripTopShadowThickness) :
      avatar_restored_y;
  avatar_bounds_.SetRect(avatar_x, avatar_y, incognito_icon.width(),
      browser_view()->ShouldShowAvatar() ? (avatar_bottom - avatar_y) : 0);
  if (avatar_button())
    avatar_button()->SetBoundsRect(avatar_bounds_);
}

void GlassBrowserFrameView::LayoutClientView() {
  client_view_bounds_ = CalculateClientAreaBounds(width(), height());
}

gfx::Insets GlassBrowserFrameView::GetClientAreaInsets() const {
  if (!browser_view()->IsTabStripVisible())
    return gfx::Insets();

  const int top_height = NonClientTopBorderHeight();
  const int border_thickness = NonClientBorderThickness();
  return gfx::Insets(top_height,
                     border_thickness,
                     border_thickness,
                     border_thickness);
}

gfx::Rect GlassBrowserFrameView::CalculateClientAreaBounds(int width,
                                                           int height) const {
  gfx::Rect bounds(0, 0, width, height);
  bounds.Inset(GetClientAreaInsets());
  return bounds;
}

void GlassBrowserFrameView::StartThrobber() {
  if (!throbber_running_) {
    throbber_running_ = true;
    throbber_frame_ = 0;
    InitThrobberIcons();
    SendMessage(views::HWNDForWidget(frame()), WM_SETICON,
                static_cast<WPARAM>(ICON_SMALL),
                reinterpret_cast<LPARAM>(throbber_icons_[throbber_frame_]));
  }
}

void GlassBrowserFrameView::StopThrobber() {
  if (throbber_running_) {
    throbber_running_ = false;

    HICON frame_icon = NULL;

    // Check if hosted BrowserView has a window icon to use.
    if (browser_view()->ShouldShowWindowIcon()) {
      gfx::ImageSkia icon = browser_view()->GetWindowIcon();
      if (!icon.isNull())
        frame_icon = IconUtil::CreateHICONFromSkBitmap(*icon.bitmap());
    }

    // Fallback to class icon.
    if (!frame_icon) {
      frame_icon = reinterpret_cast<HICON>(GetClassLongPtr(
          views::HWNDForWidget(frame()), GCLP_HICONSM));
    }

    // This will reset the small icon which we set in the throbber code.
    // WM_SETICON with NULL icon restores the icon for title bar but not
    // for taskbar. See http://crbug.com/29996
    SendMessage(views::HWNDForWidget(frame()), WM_SETICON,
                static_cast<WPARAM>(ICON_SMALL),
                reinterpret_cast<LPARAM>(frame_icon));
  }
}

void GlassBrowserFrameView::DisplayNextThrobberFrame() {
  throbber_frame_ = (throbber_frame_ + 1) % kThrobberIconCount;
  SendMessage(views::HWNDForWidget(frame()), WM_SETICON,
              static_cast<WPARAM>(ICON_SMALL),
              reinterpret_cast<LPARAM>(throbber_icons_[throbber_frame_]));
}

void GlassBrowserFrameView::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_PROFILE_CACHED_INFO_CHANGED:
      if (browser_view()->IsRegularOrGuestSession() &&
          switches::IsNewAvatarMenu()) {
        UpdateNewStyleAvatarInfo(this, NewAvatarButton::NATIVE_BUTTON);
      } else {
        UpdateAvatarInfo();
      }
      break;
    default:
      NOTREACHED() << "Got a notification we didn't register for!";
      break;
  }
}

// static
void GlassBrowserFrameView::InitThrobberIcons() {
  static bool initialized = false;
  if (!initialized) {
    for (int i = 0; i < kThrobberIconCount; ++i) {
      throbber_icons_[i] =
          ui::LoadThemeIconFromResourcesDataDLL(IDI_THROBBER_01 + i);
      DCHECK(throbber_icons_[i]);
    }
    initialized = true;
  }
}
