// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_view.h"

#include <stdint.h>
#include <algorithm>
#include <utility>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/i18n/rtl.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/profiler/scoped_tracker.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/bookmarks/bookmark_stats.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/native_window_notification_source.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/signin/chrome_signin_helper.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/bookmarks/bookmark_bar_constants.h"
#include "chrome/browser/ui/bookmarks/bookmark_bubble_sign_in_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window_state.h"
#include "chrome/browser/ui/search/search_delegate.h"
#include "chrome/browser/ui/search/search_model.h"
#include "chrome/browser/ui/sync/bubble_sync_promo_delegate.h"
#include "chrome/browser/ui/tabs/tab_menu_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/accelerator_table.h"
#include "chrome/browser/ui/views/accessibility/invert_bubble_view.h"
#include "chrome/browser/ui/views/autofill/save_card_bubble_views.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bubble_view.h"
#include "chrome/browser/ui/views/browser_dialogs.h"
#include "chrome/browser/ui/views/download/download_in_progress_dialog_view.h"
#include "chrome/browser/ui/views/download/download_shelf_view.h"
#include "chrome/browser/ui/views/exclusive_access_bubble_views.h"
#include "chrome/browser/ui/views/extensions/bookmark_app_bubble_view.h"
#include "chrome/browser/ui/views/frame/browser_view_layout.h"
#include "chrome/browser/ui/views/frame/browser_view_layout_delegate.h"
#include "chrome/browser/ui/views/frame/contents_layout_manager.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/frame/web_contents_close_handler.h"
#include "chrome/browser/ui/views/infobars/infobar_container_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/views/location_bar/zoom_bubble_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/profiles/avatar_menu_button.h"
#include "chrome/browser/ui/views/profiles/profile_chooser_view.h"
#include "chrome/browser/ui/views/session_crashed_bubble_view.h"
#include "chrome/browser/ui/views/settings_api_bubble_helper_views.h"
#include "chrome/browser/ui/views/status_bubble_views.h"
#include "chrome/browser/ui/views/tabs/browser_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"
#include "chrome/browser/ui/views/toolbar/reload_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/translate/translate_bubble_view.h"
#include "chrome/browser/ui/views/update_recommended_message_box.h"
#include "chrome/browser/ui/views/website_settings/permissions_bubble_view.h"
#include "chrome/browser/ui/views/website_settings/website_settings_popup_view.h"
#include "chrome/browser/ui/website_settings/permission_bubble_manager.h"
#include "chrome/browser/ui/window_sizer/window_sizer.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/locale_settings.h"
#include "components/app_modal/app_modal_dialog.h"
#include "components/app_modal/app_modal_dialog_queue.h"
#include "components/app_modal/native_app_modal_dialog.h"
#include "components/omnibox/browser/omnibox_popup_model.h"
#include "components/omnibox/browser/omnibox_popup_view.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "components/translate/core/browser/language_state.h"
#include "content/app/resources/grit/content_resources.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "grit/theme_resources.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/content_accelerators/accelerator_util.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/vector_icons_public.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/focus/external_focus_tracker.h"
#include "ui/views/focus/view_storage.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/native_widget.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

#if defined(USE_ASH)
#include "chrome/browser/ui/ash/ash_util.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/client/window_tree_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#endif

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "chrome/browser/jumplist_win.h"
#include "ui/gfx/color_palette.h"
#include "ui/native_theme/native_theme_dark_win.h"
#include "ui/views/win/scoped_fullscreen_visibility.h"
#endif

#if defined(ENABLE_ONE_CLICK_SIGNIN)
#include "chrome/browser/ui/sync/one_click_signin_bubble_delegate.h"
#include "chrome/browser/ui/sync/one_click_signin_bubble_links_delegate.h"
#include "chrome/browser/ui/views/sync/one_click_signin_bubble_view.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager.h"
#endif

#if defined(MOJO_SHELL_CLIENT)
#include "content/public/common/mojo_shell_connection.h"
#endif

#if defined(OS_LINUX)
#include "ui/native_theme/native_theme_dark_aura.h"
#endif

using base::TimeDelta;
using base::UserMetricsAction;
using content::NativeWebKeyboardEvent;
using content::WebContents;
using views::ColumnSet;
using views::GridLayout;
using web_modal::WebContentsModalDialogHost;

namespace {

// The name of a key to store on the window handle so that other code can
// locate this object using just the handle.
const char* const kBrowserViewKey = "__BROWSER_VIEW__";

// The number of milliseconds between loading animation frames.
const int kLoadingAnimationFrameTimeMs = 30;

// Paints the horizontal border separating the Bookmarks Bar from the Toolbar
// or page content according to |at_top| with |color|.
void PaintHorizontalBorder(gfx::Canvas* canvas,
                           BookmarkBarView* view,
                           bool at_top,
                           SkColor color) {
  int thickness = views::NonClientFrameView::kClientEdgeThickness;
  int y = at_top ? 0 : (view->height() - thickness);
  canvas->FillRect(gfx::Rect(0, y, view->width(), thickness), color);
}

// TODO(kuan): These functions are temporarily for the bookmark bar while its
// detached state is at the top of the page;  it'll be moved to float on the
// content page in the very near future, at which time, these local functions
// will be removed.
void PaintDetachedBookmarkBar(gfx::Canvas* canvas,
                              BookmarkBarView* view,
                              Profile* profile) {
  // Paint background for detached state; if animating, this is fade in/out.
  const ui::ThemeProvider& tp =
      ThemeService::GetThemeProviderForProfile(profile);
  canvas->DrawColor(
      tp.GetColor(ThemeProperties::COLOR_DETACHED_BOOKMARK_BAR_BACKGROUND));
  // Draw the separators above and below bookmark bar;
  // if animating, these are fading in/out.
  SkColor separator_color =
      tp.GetColor(ThemeProperties::COLOR_DETACHED_BOOKMARK_BAR_SEPARATOR);

  if (ui::MaterialDesignController::IsModeMaterial()) {
    BrowserView::Paint1pxHorizontalLine(
        canvas, separator_color,
        gfx::Rect(0, 0, view->width(),
                  views::NonClientFrameView::kClientEdgeThickness),
        true);
  } else {
    PaintHorizontalBorder(canvas, view, true, separator_color);
  }

  // For the bottom separator, increase the luminance. Either double it or halve
  // the distance to 1.0, whichever is less of a difference.
  color_utils::HSL hsl;
  color_utils::SkColorToHSL(separator_color, &hsl);
  hsl.l = std::min((hsl.l + 1) / 2, hsl.l * 2);
  BrowserView::Paint1pxHorizontalLine(
      canvas, color_utils::HSLToSkColor(hsl, SK_AlphaOPAQUE),
      view->GetLocalBounds(), true);
}

// Paints the background (including the theme image behind content area) for
// the Bookmarks Bar when it is attached to the Toolbar into |bounds|.
// |background_origin| is the origin to use for painting the theme image.
void PaintBackgroundAttachedMode(gfx::Canvas* canvas,
                                 const ui::ThemeProvider* theme_provider,
                                 const gfx::Rect& bounds,
                                 const gfx::Point& background_origin,
                                 chrome::HostDesktopType host_desktop_type) {
  canvas->FillRect(bounds,
                   theme_provider->GetColor(ThemeProperties::COLOR_TOOLBAR));

  // Always tile the background image in pre-MD. In MD, only tile if there's a
  // non-default image.
  // TODO(estade): remove IDR_THEME_TOOLBAR when MD is default.
  if (theme_provider->HasCustomImage(IDR_THEME_TOOLBAR) ||
      !ui::MaterialDesignController::IsModeMaterial()) {
    canvas->TileImageInt(*theme_provider->GetImageSkiaNamed(IDR_THEME_TOOLBAR),
                         background_origin.x(),
                         background_origin.y(),
                         bounds.x(),
                         bounds.y(),
                         bounds.width(),
                         bounds.height());
  }

  if (host_desktop_type == chrome::HOST_DESKTOP_TYPE_ASH &&
      !ui::MaterialDesignController::IsModeMaterial()) {
    // The pre-material design version of Ash provides additional lightening
    // at the edges of the toolbar.
    gfx::ImageSkia* toolbar_left =
        theme_provider->GetImageSkiaNamed(IDR_TOOLBAR_SHADE_LEFT);
    canvas->TileImageInt(*toolbar_left,
                         bounds.x(),
                         bounds.y(),
                         toolbar_left->width(),
                         bounds.height());
    gfx::ImageSkia* toolbar_right =
        theme_provider->GetImageSkiaNamed(IDR_TOOLBAR_SHADE_RIGHT);
    canvas->TileImageInt(*toolbar_right,
                         bounds.right() - toolbar_right->width(),
                         bounds.y(),
                         toolbar_right->width(),
                         bounds.height());
  }
}

void PaintAttachedBookmarkBar(gfx::Canvas* canvas,
                              BookmarkBarView* view,
                              BrowserView* browser_view,
                              chrome::HostDesktopType host_desktop_type,
                              int toolbar_overlap) {
  // Paint background for attached state, this is fade in/out.
  gfx::Point background_image_offset =
      browser_view->OffsetPointForToolbarBackgroundImage(
          gfx::Point(view->GetMirroredX(), view->y()));
  PaintBackgroundAttachedMode(canvas,
                              view->GetThemeProvider(),
                              view->GetLocalBounds(),
                              background_image_offset,
                              host_desktop_type);
  if (view->height() >= toolbar_overlap) {
    // Draw the separator below the Bookmarks Bar; this is fading in/out.
    if (ui::MaterialDesignController::IsModeMaterial()) {
      BrowserView::Paint1pxHorizontalLine(
          canvas, view->GetThemeProvider()->GetColor(
                      ThemeProperties::COLOR_TOOLBAR_BOTTOM_SEPARATOR),
          view->GetLocalBounds(), true);
    } else {
      PaintHorizontalBorder(
          canvas, view, false,
          view->GetThemeProvider()->GetColor(
              ThemeProperties::COLOR_TOOLBAR_BOTTOM_SEPARATOR));
    }
  }
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// Delegate implementation for BrowserViewLayout. Usually just forwards calls
// into BrowserView.
class BrowserViewLayoutDelegateImpl : public BrowserViewLayoutDelegate {
 public:
  explicit BrowserViewLayoutDelegateImpl(BrowserView* browser_view)
      : browser_view_(browser_view) {}
  ~BrowserViewLayoutDelegateImpl() override {}

  // BrowserViewLayoutDelegate overrides:
  views::View* GetContentsWebView() const override {
    return browser_view_->contents_web_view_;
  }

  bool DownloadShelfNeedsLayout() const override {
    DownloadShelfView* download_shelf = browser_view_->download_shelf_.get();
    // Re-layout the shelf either if it is visible or if its close animation
    // is currently running.
    return download_shelf &&
           (download_shelf->IsShowing() || download_shelf->IsClosing());
  }

  bool IsTabStripVisible() const override {
    return browser_view_->IsTabStripVisible();
  }

  gfx::Rect GetBoundsForTabStripInBrowserView() const override {
    gfx::RectF bounds_f(browser_view_->frame()->GetBoundsForTabStrip(
        browser_view_->tabstrip()));
    views::View::ConvertRectToTarget(browser_view_->parent(), browser_view_,
        &bounds_f);
    return gfx::ToEnclosingRect(bounds_f);
  }

  int GetTopInsetInBrowserView(bool restored) const override {
    return browser_view_->frame()->GetTopInset(restored) -
        browser_view_->y();
  }

  int GetThemeBackgroundXInset() const override {
    // TODO(pkotwicz): Return the inset with respect to the left edge of the
    // BrowserView.
    return browser_view_->frame()->GetThemeBackgroundXInset();
  }

  bool IsToolbarVisible() const override {
    return browser_view_->IsToolbarVisible();
  }

  bool IsBookmarkBarVisible() const override {
    return browser_view_->IsBookmarkBarVisible();
  }

  ExclusiveAccessBubbleViews* GetExclusiveAccessBubble() const override {
    return browser_view_->exclusive_access_bubble();
  }

 private:
  BrowserView* browser_view_;

  DISALLOW_COPY_AND_ASSIGN(BrowserViewLayoutDelegateImpl);
};

// This class is used to paint the background for Bookmarks Bar.
class BookmarkBarViewBackground : public views::Background {
 public:
  BookmarkBarViewBackground(BrowserView* browser_view,
                            BookmarkBarView* bookmark_bar_view,
                            Browser* browser);

  // views:Background:
  void Paint(gfx::Canvas* canvas, views::View* view) const override;

 private:
  BrowserView* browser_view_;

  // The view hosting this background.
  BookmarkBarView* bookmark_bar_view_;

  Browser* browser_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkBarViewBackground);
};

BookmarkBarViewBackground::BookmarkBarViewBackground(
    BrowserView* browser_view,
    BookmarkBarView* bookmark_bar_view,
    Browser* browser)
    : browser_view_(browser_view),
      bookmark_bar_view_(bookmark_bar_view),
      browser_(browser) {
}

void BookmarkBarViewBackground::Paint(gfx::Canvas* canvas,
                                      views::View* view) const {
  int toolbar_overlap = bookmark_bar_view_->GetToolbarOverlap();

  SkAlpha detached_alpha = static_cast<SkAlpha>(
      bookmark_bar_view_->size_animation().CurrentValueBetween(0xff, 0));
  if (detached_alpha != 0xff) {
    PaintAttachedBookmarkBar(canvas,
                             bookmark_bar_view_,
                             browser_view_,
                             browser_->host_desktop_type(),
                             toolbar_overlap);
  }

  if (!bookmark_bar_view_->IsDetached() || detached_alpha == 0)
    return;

  // While animating, set opacity to cross-fade between attached and detached
  // backgrounds including their respective separators.
  canvas->SaveLayerAlpha(detached_alpha);
  PaintDetachedBookmarkBar(canvas, bookmark_bar_view_, browser_->profile());
  canvas->Restore();
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, public:

// static
const char BrowserView::kViewClassName[] = "BrowserView";

BrowserView::BrowserView()
    : views::ClientView(nullptr, nullptr),
      last_focused_view_storage_id_(
          views::ViewStorage::GetInstance()->CreateStorageID()),
      frame_(nullptr),
      top_container_(nullptr),
      tabstrip_(nullptr),
      toolbar_(nullptr),
      find_bar_host_view_(nullptr),
      infobar_container_(nullptr),
      contents_web_view_(nullptr),
      devtools_web_view_(nullptr),
      contents_container_(nullptr),
      initialized_(false),
      handling_theme_changed_(false),
      in_process_fullscreen_(false),
#if defined(OS_WIN)
      ticker_(0),
      hung_window_detector_(&hung_plugin_action_),
#endif
      force_location_bar_focus_(false),
      activate_modal_dialog_factory_(this) {
}

BrowserView::~BrowserView() {
  // All the tabs should have been destroyed already. If we were closed by the
  // OS with some tabs than the NativeBrowserFrame should have destroyed them.
  DCHECK_EQ(0, browser_->tab_strip_model()->count());

  // Immersive mode may need to reparent views before they are removed/deleted.
  immersive_mode_controller_.reset();

  browser_->tab_strip_model()->RemoveObserver(this);

#if defined(OS_WIN)
  // Stop hung plugin monitoring.
  ticker_.Stop();
  ticker_.UnregisterTickHandler(&hung_window_detector_);

  // Terminate the jumplist (must be called before browser_->profile() is
  // destroyed.
  if (jumplist_.get()) {
    jumplist_->Terminate();
  }
#endif

  // We destroy the download shelf before |browser_| to remove its child
  // download views from the set of download observers (since the observed
  // downloads can be destroyed along with |browser_| and the observer
  // notifications will call back into deleted objects).
  BrowserViewLayout* browser_view_layout = GetBrowserViewLayout();
  if (browser_view_layout)
    browser_view_layout->set_download_shelf(nullptr);
  download_shelf_.reset();

  // The TabStrip attaches a listener to the model. Make sure we shut down the
  // TabStrip first so that it can cleanly remove the listener.
  if (tabstrip_) {
    tabstrip_->parent()->RemoveChildView(tabstrip_);
    if (browser_view_layout)
      browser_view_layout->set_tab_strip(nullptr);
    delete tabstrip_;
    tabstrip_ = nullptr;
  }
  // Child views maintain PrefMember attributes that point to
  // OffTheRecordProfile's PrefService which gets deleted by ~Browser.
  RemoveAllChildViews(true);
  toolbar_ = nullptr;

  // Explicitly set browser_ to null.
  browser_.reset();
}

void BrowserView::Init(Browser* browser) {
  browser_.reset(browser);
  browser_->tab_strip_model()->AddObserver(this);
  immersive_mode_controller_.reset(
      chrome::CreateImmersiveModeController(browser_->host_desktop_type()));
}

// static
BrowserView* BrowserView::GetBrowserViewForNativeWindow(
    gfx::NativeWindow window) {
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
  return widget ?
      reinterpret_cast<BrowserView*>(widget->GetNativeWindowProperty(
          kBrowserViewKey)) : nullptr;
}

// static
BrowserView* BrowserView::GetBrowserViewForBrowser(const Browser* browser) {
  return static_cast<BrowserView*>(browser->window());
}

// static
void BrowserView::Paint1pxHorizontalLine(gfx::Canvas* canvas,
                                         SkColor color,
                                         const gfx::Rect& bounds,
                                         bool at_bottom) {
  gfx::ScopedCanvas scoped_canvas(canvas);
  const float scale = canvas->UndoDeviceScaleFactor();
  gfx::RectF rect(gfx::ScaleRect(gfx::RectF(bounds), scale));
  const int inset = rect.height() - 1;
  rect.Inset(0, at_bottom ? inset : 0, 0, at_bottom ? 0 : inset);
  SkPaint paint;
  paint.setColor(color);
  canvas->sk_canvas()->drawRect(gfx::RectFToSkRect(rect), paint);
}

void BrowserView::InitStatusBubble() {
  status_bubble_.reset(new StatusBubbleViews(contents_web_view_));
  contents_web_view_->SetStatusBubble(status_bubble_.get());
}

gfx::Rect BrowserView::GetToolbarBounds() const {
  gfx::Rect toolbar_bounds(toolbar_->bounds());
  if (toolbar_bounds.IsEmpty())
    return toolbar_bounds;
  // The apparent toolbar edges are outside the "real" toolbar edges.
  toolbar_bounds.Inset(-views::NonClientFrameView::kClientEdgeThickness, 0);
  return toolbar_bounds;
}

gfx::Rect BrowserView::GetFindBarBoundingBox() const {
  return GetBrowserViewLayout()->GetFindBarBoundingBox();
}

int BrowserView::GetTabStripHeight() const {
  // We want to return tabstrip_->height(), but we might be called in the midst
  // of layout, when that hasn't yet been updated to reflect the current state.
  // So return what the tabstrip height _ought_ to be right now.
  return tabstrip_ && IsTabStripVisible() ?
      tabstrip_->GetPreferredSize().height() : 0;
}

gfx::Point BrowserView::OffsetPointForToolbarBackgroundImage(
    const gfx::Point& point) const {
  // The background image starts tiling horizontally at the window left edge and
  // vertically at the top edge of the horizontal tab strip (or where it would
  // be).  We expect our parent's origin to be the window origin.
  gfx::Point window_point(point + GetMirroredPosition().OffsetFromOrigin());
  window_point.Offset(frame_->GetThemeBackgroundXInset(),
                      -frame_->GetTopInset(false));
  return window_point;
}

bool BrowserView::IsTabStripVisible() const {
  if (immersive_mode_controller_->ShouldHideTopViews() &&
      immersive_mode_controller_->ShouldHideTabIndicators())
    return false;
  return browser_->SupportsWindowFeature(Browser::FEATURE_TABSTRIP);
}

bool BrowserView::IsOffTheRecord() const {
  return browser_->profile()->IsOffTheRecord();
}

bool BrowserView::IsGuestSession() const {
  return browser_->profile()->IsGuestSession();
}

bool BrowserView::IsRegularOrGuestSession() const {
  return profiles::IsRegularOrGuestSession(browser_.get());
}

bool BrowserView::ShouldShowAvatar() const {
#if defined(OS_CHROMEOS)
  if (!browser_->is_type_tabbed() && !browser_->is_app())
    return false;
  // Don't show incognito avatar in the guest session.
  if (IsOffTheRecord() && !IsGuestSession())
    return true;
  // This function is called via BrowserNonClientFrameView::UpdateAvatarInfo
  // during the creation of the BrowserWindow, so browser->window() will not
  // yet be set. In this case we can safely return false.
  if (!browser_->window())
    return false;
  return chrome::MultiUserWindowManager::ShouldShowAvatar(
      browser_->window()->GetNativeWindow());
#else
  if (!IsBrowserTypeNormal())
    return false;
  if (IsOffTheRecord())  // Desktop guest is incognito and needs avatar.
    return true;
  // Tests may not have a profile manager.
  if (!g_browser_process->profile_manager())
    return false;
  ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  if (cache.GetIndexOfProfileWithPath(browser_->profile()->GetPath()) ==
      std::string::npos) {
    return false;
  }

  return AvatarMenu::ShouldShowAvatarMenu();
#endif
}

bool BrowserView::GetAccelerator(int cmd_id,
                                 ui::Accelerator* accelerator) const {
  // We retrieve the accelerator information for standard accelerators
  // for cut, copy and paste.
  if (chrome::GetStandardAcceleratorForCommandId(cmd_id, accelerator))
    return true;
  // Else, we retrieve the accelerator information from the accelerator table.
  for (std::map<ui::Accelerator, int>::const_iterator it =
           accelerator_table_.begin(); it != accelerator_table_.end(); ++it) {
    if (it->second == cmd_id) {
      *accelerator = it->first;
      return true;
    }
  }
  // Else, we retrieve the accelerator information from Ash (if applicable).
  return chrome::GetAshAcceleratorForCommandId(
      cmd_id, browser_->host_desktop_type(), accelerator);
}

bool BrowserView::IsAcceleratorRegistered(const ui::Accelerator& accelerator) {
  return accelerator_table_.find(accelerator) != accelerator_table_.end();
}

WebContents* BrowserView::GetActiveWebContents() const {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

gfx::ImageSkia BrowserView::GetOTRAvatarIcon() const {
  if (ui::MaterialDesignController::IsModeMaterial()) {
    SkColor icon_color = SK_ColorWHITE;
#if defined(OS_WIN)
    // On Windows 10, we can't change the frame color so must assume it's white.
    if (base::win::GetVersion() == base::win::VERSION_WIN10)
      icon_color = gfx::kChromeIconGrey;
#endif
    return gfx::CreateVectorIcon(gfx::VectorIconId::INCOGNITO, 24, icon_color);
  }

  // GetThemeProvider() can return null if this view is not yet part of the
  // widget hierarchy, but the frame's theme provider is always valid.
  return *frame_->GetThemeProvider()->GetImageSkiaNamed(IDR_OTR_ICON);
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, BrowserWindow implementation:

void BrowserView::Show() {
#if !defined(OS_WIN)
  // The Browser associated with this browser window must become the active
  // browser at the time |Show()| is called. This is the natural behavior under
  // Windows and Ash, but other platforms will not trigger
  // OnWidgetActivationChanged() until we return to the runloop. Therefore any
  // calls to Browser::GetLastActive() will return the wrong result if we do not
  // explicitly set it here.
  // A similar block also appears in BrowserWindowCocoa::Show().
  if (browser()->host_desktop_type() != chrome::HOST_DESKTOP_TYPE_ASH)
    BrowserList::SetLastActive(browser());
#endif

  // If the window is already visible, just activate it.
  if (frame_->IsVisible()) {
    frame_->Activate();
    return;
  }

  // Showing the window doesn't make the browser window active right away.
  // This can cause SetFocusToLocationBar() to skip setting focus to the
  // location bar. To avoid this we explicilty let SetFocusToLocationBar()
  // know that it's ok to steal focus.
  force_location_bar_focus_ = true;

  // Setting the focus doesn't work when the window is invisible, so any focus
  // initialization that happened before this will be lost.
  //
  // We really "should" restore the focus whenever the window becomes unhidden,
  // but I think initializing is the only time where this can happen where
  // there is some focus change we need to pick up, and this is easier than
  // plumbing through an un-hide message all the way from the frame.
  //
  // If we do find there are cases where we need to restore the focus on show,
  // that should be added and this should be removed.
  RestoreFocus();

  frame_->Show();

  force_location_bar_focus_ = false;

  browser()->OnWindowDidShow();

  chrome::MaybeShowInvertBubbleView(this);
}

void BrowserView::ShowInactive() {
  if (!frame_->IsVisible())
    frame_->ShowInactive();
}

void BrowserView::Hide() {
  // Not implemented.
}

void BrowserView::SetBounds(const gfx::Rect& bounds) {
  ExitFullscreen();
  GetWidget()->SetBounds(bounds);
}

void BrowserView::Close() {
  frame_->Close();
}

void BrowserView::Activate() {
  frame_->Activate();
}

void BrowserView::Deactivate() {
  frame_->Deactivate();
}

bool BrowserView::IsActive() const {
  return frame_->IsActive();
}

void BrowserView::FlashFrame(bool flash) {
  frame_->FlashFrame(flash);
}

bool BrowserView::IsAlwaysOnTop() const {
  return false;
}

void BrowserView::SetAlwaysOnTop(bool always_on_top) {
  // Not implemented for browser windows.
  NOTIMPLEMENTED();
}

gfx::NativeWindow BrowserView::GetNativeWindow() const {
  // While the browser destruction is going on, the widget can already be gone,
  // but utility functions like FindBrowserWithWindow will come here and crash.
  // We short circuit therefore.
  if (!GetWidget())
    return nullptr;
  return GetWidget()->GetTopLevelWidget()->GetNativeWindow();
}

StatusBubble* BrowserView::GetStatusBubble() {
  return status_bubble_.get();
}

namespace {
  // Only used by ToolbarSizeChanged() below, but placed here because template
  // arguments (to base::AutoReset<>) must have external linkage.
  enum CallState { NORMAL, REENTRANT, REENTRANT_FORCE_FAST_RESIZE };
}

void BrowserView::UpdateTitleBar() {
  // TODO(robliao): Remove ScopedTracker below once https://crbug.com/467185 is
  // fixed.
  tracked_objects::ScopedTracker tracking_profile1(
      FROM_HERE_WITH_EXPLICIT_FUNCTION("467185 BrowserView::UpdateTitleBar1"));
  frame_->UpdateWindowTitle();
  if (ShouldShowWindowIcon() && !loading_animation_timer_.IsRunning()) {
    // TODO(robliao): Remove ScopedTracker below once https://crbug.com/467185
    // is fixed.
    tracked_objects::ScopedTracker tracking_profile2(
        FROM_HERE_WITH_EXPLICIT_FUNCTION(
            "467185 BrowserView::UpdateTitleBar2"));
    frame_->UpdateWindowIcon();
  }
}

void BrowserView::BookmarkBarStateChanged(
    BookmarkBar::AnimateChangeType change_type) {
  if (bookmark_bar_view_.get()) {
    BookmarkBar::State new_state = browser_->bookmark_bar_state();

    // We don't properly support animating the bookmark bar to and from the
    // detached state in immersive fullscreen.
    bool detached_changed = (new_state == BookmarkBar::DETACHED) ||
        bookmark_bar_view_->IsDetached();
    if (detached_changed && immersive_mode_controller_->IsEnabled())
      change_type = BookmarkBar::DONT_ANIMATE_STATE_CHANGE;

    bookmark_bar_view_->SetBookmarkBarState(new_state, change_type);
  }
  if (MaybeShowBookmarkBar(GetActiveWebContents()))
    Layout();
}

void BrowserView::UpdateDevTools() {
  UpdateDevToolsForContents(GetActiveWebContents(), true);
  Layout();
}

void BrowserView::UpdateLoadingAnimations(bool should_animate) {
  if (should_animate) {
    if (!loading_animation_timer_.IsRunning()) {
      // Loads are happening, and the timer isn't running, so start it.
      last_animation_time_ = base::TimeTicks::Now();
      loading_animation_timer_.Start(FROM_HERE,
          TimeDelta::FromMilliseconds(kLoadingAnimationFrameTimeMs), this,
          &BrowserView::LoadingAnimationCallback);
    }
  } else {
    if (loading_animation_timer_.IsRunning()) {
      last_animation_time_ = base::TimeTicks();
      loading_animation_timer_.Stop();
      // Loads are now complete, update the state if a task was scheduled.
      LoadingAnimationCallback();
    }
  }
}

void BrowserView::SetStarredState(bool is_starred) {
  GetLocationBarView()->SetStarToggled(is_starred);
}

void BrowserView::SetTranslateIconToggled(bool is_lit) {
  // Translate icon is never active on Views.
}

void BrowserView::OnActiveTabChanged(content::WebContents* old_contents,
                                     content::WebContents* new_contents,
                                     int index,
                                     int reason) {
  DCHECK(new_contents);

  // If |contents_container_| already has the correct WebContents, we can save
  // some work.  This also prevents extra events from being reported by the
  // Visibility API under Windows, as ChangeWebContents will briefly hide
  // the WebContents window.
  bool change_tab_contents =
      contents_web_view_->web_contents() != new_contents;

  // Update various elements that are interested in knowing the current
  // WebContents.

  // When we toggle the NTP floating bookmarks bar and/or the info bar,
  // we don't want any WebContents to be attached, so that we
  // avoid an unnecessary resize and re-layout of a WebContents.
  if (change_tab_contents) {
    contents_web_view_->SetWebContents(nullptr);
    devtools_web_view_->SetWebContents(nullptr);
  }

  // Do this before updating InfoBarContainer as the InfoBarContainer may
  // callback to us and trigger layout.
  if (bookmark_bar_view_.get()) {
    bookmark_bar_view_->SetBookmarkBarState(
        browser_->bookmark_bar_state(),
        BookmarkBar::DONT_ANIMATE_STATE_CHANGE);
  }

  infobar_container_->ChangeInfoBarManager(
      InfoBarService::FromWebContents(new_contents));

  if (old_contents && PermissionBubbleManager::FromWebContents(old_contents))
    PermissionBubbleManager::FromWebContents(old_contents)->HideBubble();

  if (new_contents && PermissionBubbleManager::FromWebContents(new_contents)) {
    PermissionBubbleManager::FromWebContents(new_contents)
        ->DisplayPendingRequests();
  }

  UpdateUIForContents(new_contents);

  // Layout for DevTools _before_ setting the both main and devtools WebContents
  // to avoid toggling the size of any of them.
  UpdateDevToolsForContents(new_contents, !change_tab_contents);

  if (change_tab_contents) {
    web_contents_close_handler_->ActiveTabChanged();
    contents_web_view_->SetWebContents(new_contents);
    // The second layout update should be no-op. It will just set the
    // DevTools WebContents.
    UpdateDevToolsForContents(new_contents, true);
  }

  if (!browser_->tab_strip_model()->closing_all() && GetWidget()->IsActive() &&
      GetWidget()->IsVisible()) {
    // We only restore focus if our window is visible, to avoid invoking blur
    // handlers when we are eventually shown.
    new_contents->RestoreFocus();
  }

  // Update all the UI bits.
  UpdateTitleBar();

  TranslateBubbleView::CloseBubble();
  ZoomBubbleView::CloseBubble();
}

void BrowserView::ZoomChangedForActiveTab(bool can_show_bubble) {
  GetLocationBarView()->ZoomChangedForActiveTab(
      can_show_bubble && !toolbar_->app_menu_button()->IsMenuShowing());
}

gfx::Rect BrowserView::GetRestoredBounds() const {
  gfx::Rect bounds;
  ui::WindowShowState state;
  frame_->GetWindowPlacement(&bounds, &state);
  return bounds;
}

ui::WindowShowState BrowserView::GetRestoredState() const {
  gfx::Rect bounds;
  ui::WindowShowState state;
  frame_->GetWindowPlacement(&bounds, &state);
  return state;
}

gfx::Rect BrowserView::GetBounds() const {
  return frame_->GetWindowBoundsInScreen();
}

bool BrowserView::IsMaximized() const {
  return frame_->IsMaximized();
}

bool BrowserView::IsMinimized() const {
  return frame_->IsMinimized();
}

void BrowserView::Maximize() {
  frame_->Maximize();
}

void BrowserView::Minimize() {
  frame_->Minimize();
}

void BrowserView::Restore() {
  frame_->Restore();
}

void BrowserView::EnterFullscreen(const GURL& url,
                                  ExclusiveAccessBubbleType bubble_type,
                                  bool with_toolbar) {
  if (IsFullscreen())
    return;  // Nothing to do.

  ProcessFullscreen(true, NORMAL_FULLSCREEN, url, bubble_type);
}

void BrowserView::ExitFullscreen() {
  if (!IsFullscreen())
    return;  // Nothing to do.

  ProcessFullscreen(false, NORMAL_FULLSCREEN, GURL(),
                    EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE);
}

void BrowserView::UpdateExclusiveAccessExitBubbleContent(
    const GURL& url,
    ExclusiveAccessBubbleType bubble_type) {
  // Immersive mode has no exit bubble because it has a visible strip at the
  // top that gives the user a hover target.
  // TODO(jamescook): Figure out what to do with mouse-lock.
  if (bubble_type == EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE ||
      ShouldUseImmersiveFullscreenForUrl(url)) {
    exclusive_access_bubble_.reset();
  } else if (exclusive_access_bubble_.get()) {
    exclusive_access_bubble_->UpdateContent(url, bubble_type);
  } else {
    exclusive_access_bubble_.reset(
        new ExclusiveAccessBubbleViews(this, url, bubble_type));
  }
}

void BrowserView::OnExclusiveAccessUserInput() {
  if (exclusive_access_bubble_.get())
    exclusive_access_bubble_->OnUserInput();
}

bool BrowserView::ShouldHideUIForFullscreen() const {
  // Immersive mode needs UI for the slide-down top panel.
  if (immersive_mode_controller_->IsEnabled())
    return false;

  return IsFullscreen();
}

bool BrowserView::IsFullscreen() const {
  return frame_->IsFullscreen();
}

bool BrowserView::IsFullscreenBubbleVisible() const {
  return exclusive_access_bubble_ != nullptr;
}

#if defined(OS_WIN)
void BrowserView::SetMetroSnapMode(bool enable) {
  LOCAL_HISTOGRAM_COUNTS("Metro.SnapModeToggle", enable);
  ProcessFullscreen(enable, METRO_SNAP_FULLSCREEN, GURL(),
                    EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE);
}

bool BrowserView::IsInMetroSnapMode() const {
  return false;
}
#endif  // defined(OS_WIN)

void BrowserView::RestoreFocus() {
  WebContents* selected_web_contents = GetActiveWebContents();
  if (selected_web_contents)
    selected_web_contents->RestoreFocus();
}

void BrowserView::FullscreenStateChanged() {
  CHECK(!IsFullscreen());
  ProcessFullscreen(false, NORMAL_FULLSCREEN, GURL(),
                    EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE);
}

LocationBar* BrowserView::GetLocationBar() const {
  return GetLocationBarView();
}

void BrowserView::SetFocusToLocationBar(bool select_all) {
  // On Windows, changing focus to the location bar causes the browser
  // window to become active. This can steal focus if the user has
  // another window open already. On ChromeOS, changing focus makes a
  // view believe it has a focus even if the widget doens't have a
  // focus. Either cases, we need to ignore this when the browser
  // window isn't active.
#if defined(OS_WIN) || defined(OS_CHROMEOS)
  if (!force_location_bar_focus_ && !IsActive())
    return;
#endif

  // Temporarily reveal the top-of-window views (if not already revealed) so
  // that the location bar view is visible and is considered focusable. If the
  // location bar view gains focus, |immersive_mode_controller_| will keep the
  // top-of-window views revealed.
  scoped_ptr<ImmersiveRevealedLock> focus_reveal_lock(
      immersive_mode_controller_->GetRevealedLock(
          ImmersiveModeController::ANIMATE_REVEAL_YES));

  LocationBarView* location_bar = GetLocationBarView();
  if (location_bar->omnibox_view()->IsFocusable()) {
    // Location bar got focus.
    location_bar->FocusLocation(select_all);
  } else {
    // If none of location bar got focus, then clear focus.
    views::FocusManager* focus_manager = GetFocusManager();
    DCHECK(focus_manager);
    focus_manager->ClearFocus();
  }
}

void BrowserView::UpdateReloadStopState(bool is_loading, bool force) {
  toolbar_->reload_button()->ChangeMode(
      is_loading ? ReloadButton::MODE_STOP : ReloadButton::MODE_RELOAD, force);
}

void BrowserView::UpdateToolbar(content::WebContents* contents) {
  // We may end up here during destruction.
  if (toolbar_)
    toolbar_->Update(contents);
  frame_->UpdateToolbar();
}

void BrowserView::ResetToolbarTabState(content::WebContents* contents) {
  // We may end up here during destruction.
  if (toolbar_)
    toolbar_->ResetTabState(contents);
}

void BrowserView::FocusToolbar() {
  // Temporarily reveal the top-of-window views (if not already revealed) so
  // that the toolbar is visible and is considered focusable. If the
  // toolbar gains focus, |immersive_mode_controller_| will keep the
  // top-of-window views revealed.
  scoped_ptr<ImmersiveRevealedLock> focus_reveal_lock(
      immersive_mode_controller_->GetRevealedLock(
          ImmersiveModeController::ANIMATE_REVEAL_YES));

  // Start the traversal within the main toolbar. SetPaneFocus stores
  // the current focused view before changing focus.
  toolbar_->SetPaneFocus(nullptr);
}

ToolbarActionsBar* BrowserView::GetToolbarActionsBar() {
  return toolbar_ ?
      toolbar_->browser_actions()->toolbar_actions_bar() : nullptr;
}

void BrowserView::ToolbarSizeChanged(bool is_animating) {
  // The call to SetMaxTopArrowHeight() below can result in reentrancy;
  // |call_state| tracks whether we're reentrant.  We can't just early-return in
  // this case because we need to layout again so the infobar container's bounds
  // are set correctly.
  static CallState call_state = NORMAL;

  // A reentrant call can (and should) use the fast resize path unless both it
  // and the normal call are both non-animating.
  bool use_fast_resize =
      is_animating || (call_state == REENTRANT_FORCE_FAST_RESIZE);
  if (use_fast_resize)
    contents_web_view_->SetFastResize(true);
  UpdateUIForContents(GetActiveWebContents());
  if (use_fast_resize)
    contents_web_view_->SetFastResize(false);

  // Inform the InfoBarContainer that the distance to the location icon may have
  // changed.  We have to do this after the block above so that the toolbars are
  // laid out correctly for calculating the maximum arrow height below.
  {
    base::AutoReset<CallState> resetter(&call_state,
        is_animating ? REENTRANT_FORCE_FAST_RESIZE : REENTRANT);
    SetMaxTopArrowHeight(GetMaxTopInfoBarArrowHeight(), infobar_container_);
  }

  // When transitioning from animating to not animating we need to make sure the
  // contents_container_ gets layed out. If we don't do this and the bounds
  // haven't changed contents_container_ won't get a Layout out and we'll end up
  // with a gray rect because the clip wasn't updated.  Note that a reentrant
  // call never needs to do this, because after it returns, the normal call
  // wrapping it will do it.
  if ((call_state == NORMAL) && !is_animating) {
    contents_web_view_->InvalidateLayout();
    contents_container_->Layout();
  }
}

void BrowserView::FocusBookmarksToolbar() {
  DCHECK(!immersive_mode_controller_->IsEnabled());
  if (bookmark_bar_view_.get() &&
      bookmark_bar_view_->visible() &&
      bookmark_bar_view_->GetPreferredSize().height() != 0) {
    bookmark_bar_view_->SetPaneFocusAndFocusDefault();
  }
}

void BrowserView::FocusInfobars() {
  if (infobar_container_->child_count() > 0)
    infobar_container_->SetPaneFocusAndFocusDefault();
}

void BrowserView::FocusAppMenu() {
  // Chrome doesn't have a traditional menu bar, but it has a menu button in the
  // main toolbar that plays the same role.  If the user presses a key that
  // would typically focus the menu bar, tell the toolbar to focus the menu
  // button.  If the user presses the key again, return focus to the previous
  // location.
  //
  // Not used on the Mac, which has a normal menu bar.
  if (toolbar_->IsAppMenuFocused()) {
    RestoreFocus();
  } else {
    DCHECK(!immersive_mode_controller_->IsEnabled());
    toolbar_->SetPaneFocusAndFocusAppMenu();
  }
}

void BrowserView::RotatePaneFocus(bool forwards) {
  GetFocusManager()->RotatePaneFocus(
      forwards ?
          views::FocusManager::kForward : views::FocusManager::kBackward,
      views::FocusManager::kWrap);
}

void BrowserView::DestroyBrowser() {
  // After this returns other parts of Chrome are going to be shutdown. Close
  // the window now so that we are deleted immediately and aren't left holding
  // references to deleted objects.
  GetWidget()->RemoveObserver(this);
  GetLocationBar()->GetOmniboxView()->model()->popup_model()->RemoveObserver(
      this);
  frame_->CloseNow();
}

bool BrowserView::IsBookmarkBarVisible() const {
  if (!browser_->SupportsWindowFeature(Browser::FEATURE_BOOKMARKBAR))
    return false;
  if (!bookmark_bar_view_.get())
    return false;
  if (bookmark_bar_view_->GetPreferredSize().height() == 0)
    return false;
  // New tab page needs visible bookmarks even when top-views are hidden.
  if (immersive_mode_controller_->ShouldHideTopViews() &&
      !bookmark_bar_view_->IsDetached())
    return false;
  return true;
}

bool BrowserView::IsBookmarkBarAnimating() const {
  return bookmark_bar_view_.get() &&
         bookmark_bar_view_->size_animation().is_animating();
}

bool BrowserView::IsTabStripEditable() const {
  return tabstrip_->IsTabStripEditable();
}

bool BrowserView::IsToolbarVisible() const {
  if (immersive_mode_controller_->ShouldHideTopViews())
    return false;
  // It's possible to reach here before we've been notified of being added to a
  // widget, so |toolbar_| is still null.  Return false in this case so callers
  // don't assume they can access the toolbar yet.
  return (browser_->SupportsWindowFeature(Browser::FEATURE_TOOLBAR) ||
          browser_->SupportsWindowFeature(Browser::FEATURE_LOCATIONBAR)) &&
         toolbar_;
}

gfx::Rect BrowserView::GetRootWindowResizerRect() const {
  // Views does not support resizer rects because they caused page cycler
  // performance regressions when they were added. See crrev.com/9654
  return gfx::Rect();
}

void BrowserView::ConfirmAddSearchProvider(TemplateURL* template_url,
                                           Profile* profile) {
  chrome::EditSearchEngine(GetWidget()->GetNativeWindow(), template_url,
                           nullptr, profile);
}

void BrowserView::ShowUpdateChromeDialog() {
  UpdateRecommendedMessageBox::Show(GetWidget()->GetNativeWindow());
}

void BrowserView::ShowBookmarkBubble(const GURL& url, bool already_bookmarked) {
  scoped_ptr<BubbleSyncPromoDelegate> delegate;
  delegate.reset(new BookmarkBubbleSignInDelegate(browser_.get()));

  views::View* anchor_view = GetToolbarView()->GetBookmarkBubbleAnchor();
  views::Widget* bubble_widget = BookmarkBubbleView::ShowBubble(
      anchor_view, gfx::Rect(), nullptr, bookmark_bar_view_.get(),
      std::move(delegate), browser_->profile(), url, already_bookmarked);
  GetToolbarView()->OnBubbleCreatedForAnchor(anchor_view, bubble_widget);
}

void BrowserView::ShowBookmarkAppBubble(
    const WebApplicationInfo& web_app_info,
    const ShowBookmarkAppBubbleCallback& callback) {
  BookmarkAppBubbleView::ShowBubble(GetToolbarView(), web_app_info, callback);
}

autofill::SaveCardBubbleView* BrowserView::ShowSaveCreditCardBubble(
    content::WebContents* web_contents,
    autofill::SaveCardBubbleController* controller,
    bool is_user_gesture) {
  views::View* anchor_view = GetToolbarView()->GetSaveCreditCardBubbleAnchor();
  autofill::SaveCardBubbleViews* view = new autofill::SaveCardBubbleViews(
      anchor_view, web_contents, controller);
  GetToolbarView()->OnBubbleCreatedForAnchor(anchor_view, view->GetWidget());
  view->Show(is_user_gesture ? autofill::SaveCardBubbleViews::USER_GESTURE
                             : autofill::SaveCardBubbleViews::AUTOMATIC);
  return view;
}

void BrowserView::ShowTranslateBubble(
    content::WebContents* web_contents,
    translate::TranslateStep step,
    translate::TranslateErrors::Type error_type,
    bool is_user_gesture) {
  if (contents_web_view_->HasFocus() &&
      !GetLocationBarView()->IsMouseHovered()) {
    content::RenderViewHost* rvh = web_contents->GetRenderViewHost();
    if (rvh->IsFocusedElementEditable())
      return;
  }

  ChromeTranslateClient* chrome_translate_client =
      ChromeTranslateClient::FromWebContents(web_contents);
  translate::LanguageState& language_state =
      chrome_translate_client->GetLanguageState();
  language_state.SetTranslateEnabled(true);

  if (IsMinimized())
    return;

  views::View* anchor_view = GetToolbarView()->GetTranslateBubbleAnchor();
  views::Widget* bubble_widget = TranslateBubbleView::ShowBubble(
      anchor_view, web_contents, step,
      error_type, is_user_gesture ? TranslateBubbleView::USER_GESTURE
                                  : TranslateBubbleView::AUTOMATIC);
  GetToolbarView()->OnBubbleCreatedForAnchor(anchor_view, bubble_widget);
}

#if defined(ENABLE_ONE_CLICK_SIGNIN)
void BrowserView::ShowOneClickSigninBubble(
    OneClickSigninBubbleType type,
    const base::string16& email,
    const base::string16& error_message,
    const StartSyncCallback& start_sync_callback) {
  scoped_ptr<OneClickSigninBubbleDelegate> delegate;
  delegate.reset(new OneClickSigninBubbleLinksDelegate(browser()));

  views::View* anchor_view;
  if (type == BrowserWindow::ONE_CLICK_SIGNIN_BUBBLE_TYPE_BUBBLE)
    anchor_view = toolbar_->app_menu_button();
  else
    anchor_view = toolbar_->location_bar();

  OneClickSigninBubbleView::ShowBubble(type, email, error_message,
                                       std::move(delegate), anchor_view,
                                       start_sync_callback);
}
#endif

void BrowserView::SetDownloadShelfVisible(bool visible) {
  DCHECK(download_shelf_);
  browser_->UpdateDownloadShelfVisibility(visible);

  // SetDownloadShelfVisible can force-close the shelf, so make sure we lay out
  // everything correctly, as if the animation had finished. This doesn't
  // matter for showing the shelf, as the show animation will do it.
  ToolbarSizeChanged(false);
}

bool BrowserView::IsDownloadShelfVisible() const {
  return download_shelf_.get() && download_shelf_->IsShowing();
}

DownloadShelf* BrowserView::GetDownloadShelf() {
  DCHECK(browser_->SupportsWindowFeature(Browser::FEATURE_DOWNLOADSHELF));
  if (!download_shelf_.get()) {
    download_shelf_.reset(new DownloadShelfView(browser_.get(), this));
    download_shelf_->set_owned_by_client();
    GetBrowserViewLayout()->set_download_shelf(download_shelf_.get());
  }
  return download_shelf_.get();
}

void BrowserView::ConfirmBrowserCloseWithPendingDownloads(
    int download_count,
    Browser::DownloadClosePreventionType dialog_type,
    bool app_modal,
    const base::Callback<void(bool)>& callback) {
  DownloadInProgressDialogView::Show(
      GetNativeWindow(), download_count, dialog_type, app_modal, callback);
}

void BrowserView::UserChangedTheme() {
  frame_->FrameTypeChanged();
}

void BrowserView::ShowWebsiteSettings(
    Profile* profile,
    content::WebContents* web_contents,
    const GURL& url,
    const security_state::SecurityStateModel::SecurityInfo& security_info) {
  // Some browser windows have a location icon embedded in the frame. Try to
  // use that if it exists. If it doesn't exist, use the location icon from
  // the location bar.
  views::View* popup_anchor = frame_->GetLocationIconView();
  if (!popup_anchor)
    popup_anchor = GetLocationBarView()->location_icon_view();

  WebsiteSettingsPopupView::ShowPopup(popup_anchor, gfx::Rect(), profile,
                                      web_contents, url, security_info);
}

void BrowserView::ShowAppMenu() {
  // Keep the top-of-window views revealed as long as the app menu is visible.
  scoped_ptr<ImmersiveRevealedLock> revealed_lock(
      immersive_mode_controller_->GetRevealedLock(
          ImmersiveModeController::ANIMATE_REVEAL_NO));

  toolbar_->app_menu_button()->Activate();
}

bool BrowserView::PreHandleKeyboardEvent(const NativeWebKeyboardEvent& event,
                                         bool* is_keyboard_shortcut) {
  *is_keyboard_shortcut = false;

  if ((event.type != blink::WebInputEvent::RawKeyDown) &&
      (event.type != blink::WebInputEvent::KeyUp)) {
    return false;
  }

  views::FocusManager* focus_manager = GetFocusManager();
  DCHECK(focus_manager);

  if (focus_manager->shortcut_handling_suspended())
    return false;

  ui::Accelerator accelerator =
      ui::GetAcceleratorFromNativeWebKeyboardEvent(event);

  // What we have to do here is as follows:
  // - If the |browser_| is for an app, do nothing.
  // - On CrOS if |accelerator| is deprecated, we allow web contents to consume
  //   it if needed.
  // - If the |browser_| is not for an app, and the |accelerator| is not
  //   associated with the browser (e.g. an Ash shortcut), process it.
  // - If the |browser_| is not for an app, and the |accelerator| is associated
  //   with the browser, and it is a reserved one (e.g. Ctrl+w), process it.
  // - If the |browser_| is not for an app, and the |accelerator| is associated
  //   with the browser, and it is not a reserved one, do nothing.

  if (browser_->is_app()) {
    // Let all keys fall through to a v1 app's web content, even accelerators.
    // We don't have to flip |is_keyboard_shortcut| here. If we do that, the app
    // might not be able to see a subsequent Char event. See OnHandleInputEvent
    // in content/renderer/render_widget.cc for details.
    return false;
  }

#if defined(OS_CHROMEOS)
  if (chrome::IsAcceleratorDeprecated(accelerator)) {
    if (event.type == blink::WebInputEvent::RawKeyDown)
      *is_keyboard_shortcut = true;
    return false;
  }
#endif  // defined(OS_CHROMEOS)

  chrome::BrowserCommandController* controller = browser_->command_controller();

  // Here we need to retrieve the command id (if any) associated to the
  // keyboard event. Instead of looking up the command id in the
  // |accelerator_table_| by ourselves, we block the command execution of
  // the |browser_| object then send the keyboard event to the
  // |focus_manager| as if we are activating an accelerator key.
  // Then we can retrieve the command id from the |browser_| object.
  bool original_block_command_state = controller->block_command_execution();
  controller->SetBlockCommandExecution(true);
  // If the |accelerator| is a non-browser shortcut (e.g. Ash shortcut), the
  // command execution cannot be blocked and true is returned. However, it is
  // okay as long as is_app() is false. See comments in this function.
  const bool processed = focus_manager->ProcessAccelerator(accelerator);
  const int id = controller->GetLastBlockedCommand(nullptr);
  controller->SetBlockCommandExecution(original_block_command_state);

  // Executing the command may cause |this| object to be destroyed.
  if (controller->IsReservedCommandOrKey(id, event)) {
    UpdateAcceleratorMetrics(accelerator, id);
    return chrome::ExecuteCommand(browser_.get(), id);
  }

  if (id != -1) {
    // |accelerator| is a non-reserved browser shortcut (e.g. Ctrl+f).
    if (event.type == blink::WebInputEvent::RawKeyDown)
      *is_keyboard_shortcut = true;
  } else if (processed) {
    // |accelerator| is a non-browser shortcut (e.g. F4-F10 on Ash). Report
    // that we handled it.
    return true;
  }

  return false;
}

void BrowserView::HandleKeyboardEvent(const NativeWebKeyboardEvent& event) {
  unhandled_keyboard_event_handler_.HandleKeyboardEvent(event,
                                                        GetFocusManager());
}

// TODO(devint): http://b/issue?id=1117225 Cut, Copy, and Paste are always
// enabled in the page menu regardless of whether the command will do
// anything. When someone selects the menu item, we just act as if they hit
// the keyboard shortcut for the command by sending the associated key press
// to windows. The real fix to this bug is to disable the commands when they
// won't do anything. We'll need something like an overall clipboard command
// manager to do that.
void BrowserView::CutCopyPaste(int command_id) {
  // If a WebContents is focused, call its member method.
  //
  // We could make WebContents register accelerators and then just use the
  // plumbing for accelerators below to dispatch these, but it's not clear
  // whether that would still allow keypresses of ctrl-X/C/V to be sent as
  // key events (and not accelerators) to the WebContents so it can give the web
  // page a chance to override them.
  WebContents* contents = browser_->tab_strip_model()->GetActiveWebContents();
  if (contents) {
    void (WebContents::*method)();
    if (command_id == IDC_CUT)
      method = &content::WebContents::Cut;
    else if (command_id == IDC_COPY)
      method = &content::WebContents::Copy;
    else
      method = &content::WebContents::Paste;
    if (DoCutCopyPasteForWebContents(contents, method))
      return;

    WebContents* devtools =
        DevToolsWindow::GetInTabWebContents(contents, nullptr);
    if (devtools && DoCutCopyPasteForWebContents(devtools, method))
      return;
  }

  // Any Views which want to handle the clipboard commands in the Chrome menu
  // should:
  //   (a) Register ctrl-X/C/V as accelerators
  //   (b) Implement CanHandleAccelerators() to not return true unless they're
  //       focused, as the FocusManager will try all registered accelerator
  //       handlers, not just the focused one.
  // Currently, Textfield (which covers the omnibox and find bar, and likely any
  // other native UI in the future that wants to deal with clipboard commands)
  // does the above.
  ui::Accelerator accelerator;
  GetAccelerator(command_id, &accelerator);
  GetFocusManager()->ProcessAccelerator(accelerator);
}

WindowOpenDisposition BrowserView::GetDispositionForPopupBounds(
    const gfx::Rect& bounds) {
  return NEW_POPUP;
}

FindBar* BrowserView::CreateFindBar() {
  return chrome::CreateFindBar(this);
}

WebContentsModalDialogHost* BrowserView::GetWebContentsModalDialogHost() {
  return GetBrowserViewLayout()->GetWebContentsModalDialogHost();
}

BookmarkBarView* BrowserView::GetBookmarkBarView() const {
  return bookmark_bar_view_.get();
}

LocationBarView* BrowserView::GetLocationBarView() const {
  return toolbar_ ? toolbar_->location_bar() : nullptr;
}

views::View* BrowserView::GetTabContentsContainerView() const {
  return contents_web_view_;
}

ToolbarView* BrowserView::GetToolbarView() const {
  return toolbar_;
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, TabStripModelObserver implementation:

void BrowserView::TabInsertedAt(WebContents* contents,
                                int index,
                                bool foreground) {
#if defined(USE_AURA)
  // WebContents inserted in tabs might not have been added to the root
  // window yet. Per http://crbug/342672 add them now since drawing the
  // WebContents requires root window specific data - information about
  // the screen the WebContents is drawn on, for example.
  if (!contents->GetNativeView()->GetRootWindow()) {
    aura::Window* window = contents->GetNativeView();
    aura::Window* root_window = GetNativeWindow()->GetRootWindow();
    aura::client::ParentWindowWithContext(
        window, root_window, root_window->GetBoundsInScreen());
    DCHECK(contents->GetNativeView()->GetRootWindow());
  }
#endif
  web_contents_close_handler_->TabInserted();

  if (foreground)
    extensions::MaybeShowExtensionControlledNewTabPage(browser(), contents);
}

void BrowserView::TabDetachedAt(WebContents* contents, int index) {
  if (PermissionBubbleManager::FromWebContents(contents))
    PermissionBubbleManager::FromWebContents(contents)->HideBubble();

  // We use index here rather than comparing |contents| because by this time
  // the model has already removed |contents| from its list, so
  // browser_->GetActiveWebContents() will return null or something else.
  if (index == browser_->tab_strip_model()->active_index()) {
    // We need to reset the current tab contents to null before it gets
    // freed. This is because the focus manager performs some operations
    // on the selected WebContents when it is removed.
    web_contents_close_handler_->ActiveTabChanged();
    contents_web_view_->SetWebContents(nullptr);
    infobar_container_->ChangeInfoBarManager(nullptr);
    UpdateDevToolsForContents(nullptr, true);
  }
}

void BrowserView::TabDeactivated(WebContents* contents) {
  if (PermissionBubbleManager::FromWebContents(contents))
    PermissionBubbleManager::FromWebContents(contents)->HideBubble();

  // We do not store the focus when closing the tab to work-around bug 4633.
  // Some reports seem to show that the focus manager and/or focused view can
  // be garbage at that point, it is not clear why.
  if (!contents->IsBeingDestroyed())
    contents->StoreFocus();
}

void BrowserView::TabStripEmpty() {
  // Make sure all optional UI is removed before we are destroyed, otherwise
  // there will be consequences (since our view hierarchy will still have
  // references to freed views).
  UpdateUIForContents(nullptr);
}

void BrowserView::WillCloseAllTabs() {
  web_contents_close_handler_->WillCloseAllTabs();
}

void BrowserView::CloseAllTabsCanceled() {
  web_contents_close_handler_->CloseAllTabsCanceled();
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, ui::AcceleratorProvider implementation:

bool BrowserView::GetAcceleratorForCommandId(int command_id,
                                             ui::Accelerator* accelerator) {
  // Let's let the ToolbarView own the canonical implementation of this method.
  return toolbar_->GetAcceleratorForCommandId(command_id, accelerator);
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, views::WidgetDelegate implementation:

bool BrowserView::CanResize() const {
  return true;
}

bool BrowserView::CanMaximize() const {
  return true;
}

bool BrowserView::CanMinimize() const {
  return true;
}

bool BrowserView::CanActivate() const {
  app_modal::AppModalDialogQueue* queue =
      app_modal::AppModalDialogQueue::GetInstance();
  if (!queue->active_dialog() || !queue->active_dialog()->native_dialog() ||
      !queue->active_dialog()->native_dialog()->IsShowing()) {
    return true;
  }

#if defined(USE_AURA) && defined(OS_CHROMEOS)
  // On Aura window manager controls all windows so settings focus via PostTask
  // will make only worse because posted task will keep trying to steal focus.
  queue->ActivateModalDialog();
#else
  // If another browser is app modal, flash and activate the modal browser. This
  // has to be done in a post task, otherwise if the user clicked on a window
  // that doesn't have the modal dialog the windows keep trying to get the focus
  // from each other on Windows. http://crbug.com/141650.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&BrowserView::ActivateAppModalDialog,
                            activate_modal_dialog_factory_.GetWeakPtr()));
#endif
  return false;
}

base::string16 BrowserView::GetWindowTitle() const {
  return browser_->GetWindowTitleForCurrentTab();
}

base::string16 BrowserView::GetAccessibleWindowTitle() const {
  if (IsOffTheRecord()) {
    return l10n_util::GetStringFUTF16(
        IDS_ACCESSIBLE_INCOGNITO_WINDOW_TITLE_FORMAT,
        GetWindowTitle());
  }
  return GetWindowTitle();
}

views::View* BrowserView::GetInitiallyFocusedView() {
  return nullptr;
}

bool BrowserView::ShouldShowWindowTitle() const {
  // For Ash only, trusted windows (apps and settings) do not show a title,
  // crbug.com/119411. Child windows (i.e. popups) do show a title.
  if (browser_->host_desktop_type() == chrome::HOST_DESKTOP_TYPE_ASH &&
      browser_->is_trusted_source() &&
      !browser_->SupportsWindowFeature(Browser::FEATURE_WEBAPPFRAME))
    return false;

  return browser_->SupportsWindowFeature(Browser::FEATURE_TITLEBAR);
}

gfx::ImageSkia BrowserView::GetWindowAppIcon() {
  if (browser_->is_app()) {
    WebContents* contents = browser_->tab_strip_model()->GetActiveWebContents();
    extensions::TabHelper* extensions_tab_helper =
        contents ? extensions::TabHelper::FromWebContents(contents) : nullptr;
    if (extensions_tab_helper && extensions_tab_helper->GetExtensionAppIcon())
      return gfx::ImageSkia::CreateFrom1xBitmap(
          *extensions_tab_helper->GetExtensionAppIcon());
  }

  return GetWindowIcon();
}

gfx::ImageSkia BrowserView::GetWindowIcon() {
  // Use the default icon for devtools.
  if (browser_->is_devtools())
    return gfx::ImageSkia();

  if (browser_->is_app() || browser_->is_type_popup())
    return browser_->GetCurrentPageIcon().AsImageSkia();
  return gfx::ImageSkia();
}

bool BrowserView::ShouldShowWindowIcon() const {
  // For Ash only, trusted windows (apps and settings) do not show an icon,
  // crbug.com/119411. Child windows (i.e. popups) do show an icon.
  if (browser_->host_desktop_type() == chrome::HOST_DESKTOP_TYPE_ASH &&
      browser_->is_trusted_source() &&
      !browser_->SupportsWindowFeature(Browser::FEATURE_WEBAPPFRAME))
    return false;

  return browser_->SupportsWindowFeature(Browser::FEATURE_TITLEBAR);
}

bool BrowserView::ExecuteWindowsCommand(int command_id) {
  // This function handles WM_SYSCOMMAND, WM_APPCOMMAND, and WM_COMMAND.
#if defined(OS_WIN)
  if (command_id == IDC_DEBUG_FRAME_TOGGLE)
    GetWidget()->DebugToggleFrameType();
#endif
  // Translate WM_APPCOMMAND command ids into a command id that the browser
  // knows how to handle.
  int command_id_from_app_command = GetCommandIDForAppCommandID(command_id);
  if (command_id_from_app_command != -1)
    command_id = command_id_from_app_command;

  return chrome::ExecuteCommand(browser_.get(), command_id);
}

std::string BrowserView::GetWindowName() const {
  return chrome::GetWindowName(browser_.get());
}

void BrowserView::SaveWindowPlacement(const gfx::Rect& bounds,
                                      ui::WindowShowState show_state) {
  // If IsFullscreen() is true, we've just changed into fullscreen mode, and
  // we're catching the going-into-fullscreen sizing and positioning calls,
  // which we want to ignore.
  if (!IsFullscreen() && frame_->ShouldSaveWindowPlacement() &&
      chrome::ShouldSaveWindowPlacement(browser_.get())) {
    WidgetDelegate::SaveWindowPlacement(bounds, show_state);
    chrome::SaveWindowPlacement(browser_.get(), bounds, show_state);
  }
}

bool BrowserView::GetSavedWindowPlacement(
    const views::Widget* widget,
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {
  chrome::GetSavedWindowBoundsAndShowState(browser_.get(), bounds, show_state);

  if (chrome::SavedBoundsAreContentBounds(browser_.get())) {
    // This is normal non-app popup window. The value passed in |bounds|
    // represents two pieces of information:
    // - the position of the window, in screen coordinates (outer position).
    // - the size of the content area (inner size).
    // We need to use these values to determine the appropriate size and
    // position of the resulting window.
    if (IsToolbarVisible()) {
      // If we're showing the toolbar, we need to adjust |*bounds| to include
      // its desired height, since the toolbar is considered part of the
      // window's client area as far as GetWindowBoundsForClientBounds is
      // concerned...
      bounds->set_height(
          bounds->height() + toolbar_->GetPreferredSize().height());
    }

    gfx::Rect window_rect = frame_->non_client_view()->
        GetWindowBoundsForClientBounds(*bounds);
    window_rect.set_origin(bounds->origin());

    // When we are given x/y coordinates of 0 on a created popup window,
    // assume none were given by the window.open() command.
    if (window_rect.x() == 0 && window_rect.y() == 0) {
      gfx::Size size = window_rect.size();
      window_rect.set_origin(WindowSizer::GetDefaultPopupOrigin(size));
    }

    *bounds = window_rect;
    *show_state = ui::SHOW_STATE_NORMAL;
  }

  // We return true because we can _always_ locate reasonable bounds using the
  // WindowSizer, and we don't want to trigger the Window's built-in "size to
  // default" handling because the browser window has no default preferred
  // size.
  return true;
}

views::View* BrowserView::GetContentsView() {
  return contents_web_view_;
}

views::ClientView* BrowserView::CreateClientView(views::Widget* widget) {
  return this;
}

void BrowserView::OnWidgetActivationChanged(views::Widget* widget,
                                            bool active) {
  if (active)
    BrowserList::SetLastActive(browser_.get());
}

void BrowserView::OnWindowBeginUserBoundsChange() {
  WebContents* web_contents = GetActiveWebContents();
  if (!web_contents)
    return;
  web_contents->GetRenderViewHost()->NotifyMoveOrResizeStarted();
}

void BrowserView::OnWidgetMove() {
  if (!initialized_) {
    // Creating the widget can trigger a move. Ignore it until we've initialized
    // things.
    return;
  }

  // Cancel any tabstrip animations, some of them may be invalidated by the
  // window being repositioned.
  // Comment out for one cycle to see if this fixes dist tests.
  // tabstrip_->DestroyDragController();

  // status_bubble_ may be null if this is invoked during construction.
  if (status_bubble_.get())
    status_bubble_->Reposition();

  BookmarkBubbleView::Hide();

  // Close the omnibox popup, if any.
  LocationBarView* location_bar_view = GetLocationBarView();
  if (location_bar_view)
    location_bar_view->GetOmniboxView()->CloseOmniboxPopup();
}

views::Widget* BrowserView::GetWidget() {
  return View::GetWidget();
}

const views::Widget* BrowserView::GetWidget() const {
  return View::GetWidget();
}

void BrowserView::GetAccessiblePanes(std::vector<views::View*>* panes) {
  // This should be in the order of pane traversal of the panes using F6
  // (Windows) or Ctrl+Back/Forward (Chrome OS).  If one of these is
  // invisible or has no focusable children, it will be automatically
  // skipped.
  panes->push_back(toolbar_);
  if (bookmark_bar_view_.get())
    panes->push_back(bookmark_bar_view_.get());
  if (infobar_container_)
    panes->push_back(infobar_container_);
  if (download_shelf_.get())
    panes->push_back(download_shelf_.get());
  panes->push_back(GetTabContentsContainerView());
  if (devtools_web_view_->visible())
    panes->push_back(devtools_web_view_);
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, views::ClientView overrides:

bool BrowserView::CanClose() {
  // You cannot close a frame for which there is an active originating drag
  // session.
  if (tabstrip_ && !tabstrip_->IsTabStripCloseable())
    return false;

  // Give beforeunload handlers the chance to cancel the close before we hide
  // the window below.
  if (!browser_->ShouldCloseWindow())
    return false;

  bool fast_tab_closing_enabled =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableFastUnload);

  if (!browser_->tab_strip_model()->empty()) {
    // Tab strip isn't empty.  Hide the frame (so it appears to have closed
    // immediately) and close all the tabs, allowing the renderers to shut
    // down. When the tab strip is empty we'll be called back again.
    frame_->Hide();
    browser_->OnWindowClosing();
    if (fast_tab_closing_enabled)
      browser_->tab_strip_model()->CloseAllTabs();
    return false;
  } else if (fast_tab_closing_enabled &&
        !browser_->HasCompletedUnloadProcessing()) {
    // The browser needs to finish running unload handlers.
    // Hide the frame (so it appears to have closed immediately), and
    // the browser will call us back again when it is ready to close.
    frame_->Hide();
    return false;
  }

  return true;
}

int BrowserView::NonClientHitTest(const gfx::Point& point) {
  return GetBrowserViewLayout()->NonClientHitTest(point);
}

gfx::Size BrowserView::GetMinimumSize() const {
  return GetBrowserViewLayout()->GetMinimumSize();
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, views::View overrides:

const char* BrowserView::GetClassName() const {
  return kViewClassName;
}

void BrowserView::Layout() {
  if (!initialized_ || in_process_fullscreen_)
    return;

  views::View::Layout();

  // TODO(jamescook): Why was this in the middle of layout code?
  toolbar_->location_bar()->omnibox_view()->SetFocusable(IsToolbarVisible());
}

void BrowserView::PaintChildren(const ui::PaintContext& context) {
  // Paint the |infobar_container_| last so that it may paint its
  // overlapping tabs.
  for (int i = 0; i < child_count(); ++i) {
    View* child = child_at(i);
    if (child != infobar_container_ && !child->layer())
      child->Paint(context);
  }

  infobar_container_->Paint(context);
}

void BrowserView::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (!initialized_ && details.is_add && details.child == this && GetWidget()) {
    InitViews();
    initialized_ = true;
  }
}

void BrowserView::ChildPreferredSizeChanged(View* child) {
  Layout();
}

void BrowserView::GetAccessibleState(ui::AXViewState* state) {
  state->role = ui::AX_ROLE_CLIENT;
}

void BrowserView::OnThemeChanged() {
  if (!IsRegularOrGuestSession() &&
      ui::MaterialDesignController::IsModeMaterial()) {
    // When the theme changes, the native theme may also change (in OTR, the
    // usage of dark or normal hinges on the browser theme), so we have to
    // propagate both kinds of change.
    base::AutoReset<bool> reset(&handling_theme_changed_, true);
#if defined(OS_WIN)
    ui::NativeThemeDarkWin::instance()->NotifyObservers();
    ui::NativeThemeWin::instance()->NotifyObservers();
#elif defined(OS_LINUX)
    ui::NativeThemeDarkAura::instance()->NotifyObservers();
    ui::NativeThemeAura::instance()->NotifyObservers();
#endif
  }

  views::View::OnThemeChanged();
}

void BrowserView::OnNativeThemeChanged(const ui::NativeTheme* theme) {
  // Do not handle native theme changes before the browser view is initialized.
  if (!initialized_)
    return;
  ClientView::OnNativeThemeChanged(theme);
  // Don't infinitely recurse.
  if (!handling_theme_changed_)
    UserChangedTheme();
  chrome::MaybeShowInvertBubbleView(this);
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, ui::AcceleratorTarget overrides:

bool BrowserView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  std::map<ui::Accelerator, int>::const_iterator iter =
      accelerator_table_.find(accelerator);
  DCHECK(iter != accelerator_table_.end());
  int command_id = iter->second;

  chrome::BrowserCommandController* controller = browser_->command_controller();
  if (!controller->block_command_execution())
    UpdateAcceleratorMetrics(accelerator, command_id);
  return chrome::ExecuteCommand(browser_.get(), command_id);
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, OmniboxPopupModelObserver overrides:
void BrowserView::OnOmniboxPopupShownOrHidden() {
  SetMaxTopArrowHeight(GetMaxTopInfoBarArrowHeight(), infobar_container_);
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, InfoBarContainerDelegate overrides:

SkColor BrowserView::GetInfoBarSeparatorColor() const {
  return GetThemeProvider()->GetColor(
      ThemeProperties::COLOR_TOOLBAR_BOTTOM_SEPARATOR);
}

void BrowserView::InfoBarContainerStateChanged(bool is_animating) {
  ToolbarSizeChanged(is_animating);
}

bool BrowserView::DrawInfoBarArrows(int* x) const {
  if (x) {
    gfx::Point anchor(toolbar_->location_bar()->GetLocationBarAnchorPoint());
    ConvertPointToTarget(toolbar_->location_bar(), this, &anchor);
    *x = anchor.x();
  }
  return true;
}

void BrowserView::InitViews() {
  GetWidget()->AddObserver(this);

  // Stow a pointer to this object onto the window handle so that we can get at
  // it later when all we have is a native view.
  GetWidget()->SetNativeWindowProperty(kBrowserViewKey, this);

  // Stow a pointer to the browser's profile onto the window handle so that we
  // can get it later when all we have is a native view.
  GetWidget()->SetNativeWindowProperty(Profile::kProfileKey,
                                       browser_->profile());

  // Start a hung plugin window detector for this browser object (as long as
  // hang detection is not disabled).
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableHangMonitor)) {
    InitHangMonitor();
  }

  LoadAccelerators();

  infobar_container_ = new InfoBarContainerView(this);
  AddChildView(infobar_container_);

  contents_web_view_ = new ContentsWebView(browser_->profile());
  contents_web_view_->set_id(VIEW_ID_TAB_CONTAINER);
  contents_web_view_->SetEmbedFullscreenWidgetMode(true);

  web_contents_close_handler_.reset(
      new WebContentsCloseHandler(contents_web_view_));

  devtools_web_view_ = new views::WebView(browser_->profile());
  devtools_web_view_->set_id(VIEW_ID_DEV_TOOLS_DOCKED);
  devtools_web_view_->SetVisible(false);

  contents_container_ = new views::View();
  contents_container_->set_background(views::Background::CreateSolidBackground(
      GetThemeProvider()->GetColor(ThemeProperties::COLOR_CONTROL_BACKGROUND)));
  contents_container_->AddChildView(devtools_web_view_);
  contents_container_->AddChildView(contents_web_view_);
  contents_container_->SetLayoutManager(new ContentsLayoutManager(
      devtools_web_view_, contents_web_view_));
  AddChildView(contents_container_);
  set_contents_view(contents_container_);

  // Top container holds tab strip and toolbar and lives at the front of the
  // view hierarchy.
  top_container_ = new TopContainerView(this);
  AddChildView(top_container_);

  // TabStrip takes ownership of the controller.
  BrowserTabStripController* tabstrip_controller =
      new BrowserTabStripController(browser_.get(),
                                    browser_->tab_strip_model());
  tabstrip_ = new TabStrip(tabstrip_controller);
  top_container_->AddChildView(tabstrip_);
  tabstrip_controller->InitFromModel(tabstrip_);

  toolbar_ = new ToolbarView(browser_.get());
  top_container_->AddChildView(toolbar_);
  toolbar_->Init();

  InitStatusBubble();

  // Create do-nothing view for the sake of controlling the z-order of the find
  // bar widget.
  find_bar_host_view_ = new View();
  AddChildView(find_bar_host_view_);

  immersive_mode_controller_->Init(this);

  BrowserViewLayout* browser_view_layout = new BrowserViewLayout;
  browser_view_layout->Init(new BrowserViewLayoutDelegateImpl(this),
                            browser(),
                            this,
                            top_container_,
                            tabstrip_,
                            toolbar_,
                            infobar_container_,
                            contents_container_,
                            GetContentsLayoutManager(),
                            immersive_mode_controller_.get());
  SetLayoutManager(browser_view_layout);

#if defined(OS_WIN)
  // Create a custom JumpList and add it to an observer of TabRestoreService
  // so we can update the custom JumpList when a tab is added or removed.
  if (JumpList::Enabled()) {
    load_complete_listener_.reset(new LoadCompleteListener(this));
  }
#endif

  GetLocationBar()->GetOmniboxView()->model()->popup_model()->AddObserver(this);

  frame_->OnBrowserViewInitViewsComplete();
}

void BrowserView::LoadingAnimationCallback() {
  base::TimeTicks now = base::TimeTicks::Now();
  if (!last_animation_time_.is_null()) {
    UMA_HISTOGRAM_TIMES(
        "Tabs.LoadingAnimationTime",
        now - last_animation_time_);
  }
  last_animation_time_ = now;
  if (browser_->is_type_tabbed()) {
    // Loading animations are shown in the tab for tabbed windows.  We check the
    // browser type instead of calling IsTabStripVisible() because the latter
    // will return false for fullscreen windows, but we still need to update
    // their animations (so that when they come out of fullscreen mode they'll
    // be correct).
    tabstrip_->UpdateLoadingAnimations();
  } else if (ShouldShowWindowIcon()) {
    // ... or in the window icon area for popups and app windows.
    WebContents* web_contents =
        browser_->tab_strip_model()->GetActiveWebContents();
    // GetActiveWebContents can return null for example under Purify when
    // the animations are running slowly and this function is called on a timer
    // through LoadingAnimationCallback.
    frame_->UpdateThrobber(web_contents && web_contents->IsLoading());
  }
}

void BrowserView::OnLoadCompleted() {
#if defined(OS_WIN)
  DCHECK(!jumplist_.get());
  jumplist_ = new JumpList(browser_->profile());
#endif
}

BrowserViewLayout* BrowserView::GetBrowserViewLayout() const {
  return static_cast<BrowserViewLayout*>(GetLayoutManager());
}

ContentsLayoutManager* BrowserView::GetContentsLayoutManager() const {
  return static_cast<ContentsLayoutManager*>(
      contents_container_->GetLayoutManager());
}

bool BrowserView::MaybeShowBookmarkBar(WebContents* contents) {
  bool show_bookmark_bar = contents &&
      browser_->SupportsWindowFeature(Browser::FEATURE_BOOKMARKBAR);
  if (!show_bookmark_bar && !bookmark_bar_view_.get())
    return false;
  if (!bookmark_bar_view_.get()) {
    bookmark_bar_view_.reset(new BookmarkBarView(browser_.get(), this));
    bookmark_bar_view_->set_owned_by_client();
    bookmark_bar_view_->set_background(new BookmarkBarViewBackground(
        this, bookmark_bar_view_.get(), browser_.get()));
    bookmark_bar_view_->SetBookmarkBarState(
        browser_->bookmark_bar_state(),
        BookmarkBar::DONT_ANIMATE_STATE_CHANGE);
    GetBrowserViewLayout()->set_bookmark_bar(bookmark_bar_view_.get());
  }
  // Don't change the visibility of the BookmarkBarView. BrowserViewLayout
  // handles it.
  bookmark_bar_view_->SetPageNavigator(GetActiveWebContents());

  // Update parenting for the bookmark bar. This may detach it from all views.
  bool needs_layout = false;
  views::View* new_parent = nullptr;
  if (show_bookmark_bar) {
    if (bookmark_bar_view_->IsDetached())
      new_parent = this;
    else
      new_parent = top_container_;
  }
  if (new_parent != bookmark_bar_view_->parent()) {
    SetBookmarkBarParent(new_parent);
    needs_layout = true;
  }

  // Check for updates to the desired size.
  if (bookmark_bar_view_->GetPreferredSize().height() !=
      bookmark_bar_view_->height())
    needs_layout = true;

  return needs_layout;
}

void BrowserView::SetBookmarkBarParent(views::View* new_parent) {
  if (new_parent == this) {
    // Add it underneath |top_container_| or at the end if top container isn't
    // found.
    int top_container_index = GetIndexOf(top_container_);
    if (top_container_index >= 0)
      AddChildViewAt(bookmark_bar_view_.get(), top_container_index);
    else
      AddChildView(bookmark_bar_view_.get());
  } else if (new_parent) {
    // No special stacking is required for other parents.
    new_parent->AddChildView(bookmark_bar_view_.get());
  } else {
    // Bookmark bar is being detached from all views because it is hidden.
    bookmark_bar_view_->parent()->RemoveChildView(bookmark_bar_view_.get());
  }
}

bool BrowserView::MaybeShowInfoBar(WebContents* contents) {
  // TODO(beng): Remove this function once the interface between
  //             InfoBarContainer, DownloadShelfView and WebContents and this
  //             view is sorted out.
  return true;
}

void BrowserView::UpdateDevToolsForContents(
    WebContents* web_contents, bool update_devtools_web_contents) {
  DevToolsContentsResizingStrategy strategy;
  WebContents* devtools = DevToolsWindow::GetInTabWebContents(
      web_contents, &strategy);

  if (!devtools_web_view_->web_contents() && devtools &&
      !devtools_focus_tracker_.get()) {
    // Install devtools focus tracker when dev tools window is shown for the
    // first time.
    devtools_focus_tracker_.reset(
        new views::ExternalFocusTracker(devtools_web_view_,
                                        GetFocusManager()));
  }

  // Restore focus to the last focused view when hiding devtools window.
  if (devtools_web_view_->web_contents() && !devtools &&
      devtools_focus_tracker_.get()) {
    devtools_focus_tracker_->FocusLastFocusedExternalView();
    devtools_focus_tracker_.reset();
  }

  // Replace devtools WebContents.
  if (devtools_web_view_->web_contents() != devtools &&
      update_devtools_web_contents) {
    devtools_web_view_->SetWebContents(devtools);
  }

  if (devtools) {
    devtools_web_view_->SetVisible(true);
    GetContentsLayoutManager()->SetContentsResizingStrategy(strategy);
  } else {
    devtools_web_view_->SetVisible(false);
    GetContentsLayoutManager()->SetContentsResizingStrategy(
        DevToolsContentsResizingStrategy());
  }
  contents_container_->Layout();

  if (devtools) {
    // When strategy.hide_inspected_contents() returns true, we are hiding
    // contents_web_view_ behind the devtools_web_view_. Otherwise,
    // contents_web_view_ should be right above the devtools_web_view_.
    int devtools_index = contents_container_->GetIndexOf(devtools_web_view_);
    int contents_index = contents_container_->GetIndexOf(contents_web_view_);
    bool devtools_is_on_top = devtools_index > contents_index;
    if (strategy.hide_inspected_contents() != devtools_is_on_top)
      contents_container_->ReorderChildView(contents_web_view_, devtools_index);
  }
}

void BrowserView::UpdateUIForContents(WebContents* contents) {
  bool needs_layout = MaybeShowBookmarkBar(contents);
  // TODO(jamescook): This function always returns true. Remove it and figure
  // out when layout is actually required.
  needs_layout |= MaybeShowInfoBar(contents);
  if (needs_layout)
    Layout();
}

void BrowserView::ProcessFullscreen(bool fullscreen,
                                    FullscreenMode mode,
                                    const GURL& url,
                                    ExclusiveAccessBubbleType bubble_type) {
  if (in_process_fullscreen_)
    return;
  in_process_fullscreen_ = true;

  // Reduce jankiness during the following position changes by:
  //   * Hiding the window until it's in the final position
  //   * Ignoring all intervening Layout() calls, which resize the webpage and
  //     thus are slow and look ugly (enforced via |in_process_fullscreen_|).
  LocationBarView* location_bar = GetLocationBarView();

  if (mode == METRO_SNAP_FULLSCREEN || !fullscreen) {
    // Hide the fullscreen bubble as soon as possible, since the mode toggle can
    // take enough time for the user to notice.
    exclusive_access_bubble_.reset();
  }

  if (fullscreen) {
    // Move focus out of the location bar if necessary.
    views::FocusManager* focus_manager = GetFocusManager();
    DCHECK(focus_manager);
    // Look for focus in the location bar itself or any child view.
    if (location_bar->Contains(focus_manager->GetFocusedView()))
      focus_manager->ClearFocus();
  }

  // Toggle fullscreen mode.
  frame_->SetFullscreen(fullscreen);

  // Enable immersive before the browser refreshes its list of enabled commands.
  if (mode != METRO_SNAP_FULLSCREEN && ShouldUseImmersiveFullscreenForUrl(url))
    immersive_mode_controller_->SetEnabled(fullscreen);

  browser_->WindowFullscreenStateChanged();

  if (fullscreen && !chrome::IsRunningInAppMode() &&
      mode != METRO_SNAP_FULLSCREEN) {
    UpdateExclusiveAccessExitBubbleContent(url, bubble_type);
  }

  // Undo our anti-jankiness hacks and force a re-layout. We also need to
  // recompute the height of the infobar top arrow because toggling in and out
  // of fullscreen changes it. Calling ToolbarSizeChanged() will do both these
  // things since it computes the arrow height directly and forces a layout
  // indirectly via UpdateUIForContents(). Reset |in_process_fullscreen_| in
  // order to let the layout occur.
  in_process_fullscreen_ = false;
  ToolbarSizeChanged(false);

  WebContents* contents = browser_->tab_strip_model()->GetActiveWebContents();
  if (contents && PermissionBubbleManager::FromWebContents(contents))
    PermissionBubbleManager::FromWebContents(contents)->UpdateAnchorPosition();
}

bool BrowserView::ShouldUseImmersiveFullscreenForUrl(const GURL& url) const {
  // Kiosk mode needs the whole screen, and if we're not in an Ash desktop
  // immersive fullscreen doesn't exist.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kKioskMode) ||
      browser()->host_desktop_type() != chrome::HOST_DESKTOP_TYPE_ASH) {
    return false;
  }

  return url.is_empty();
}

void BrowserView::LoadAccelerators() {
  // TODO(beng): for some reason GetFocusManager() returns null in this case,
  //             investigate, but for now just disable accelerators in this
  //             mode.
#if defined(MOJO_SHELL_CLIENT)
  if (content::MojoShellConnection::Get())
    return;
#endif

  views::FocusManager* focus_manager = GetFocusManager();
  DCHECK(focus_manager);

  // Let's fill our own accelerator table.
  const bool is_app_mode = chrome::IsRunningInForcedAppMode();
  const std::vector<chrome::AcceleratorMapping> accelerator_list(
      chrome::GetAcceleratorList());
  for (std::vector<chrome::AcceleratorMapping>::const_iterator it =
           accelerator_list.begin(); it != accelerator_list.end(); ++it) {
    // In app mode, only allow accelerators of white listed commands to pass
    // through.
    if (is_app_mode && !chrome::IsCommandAllowedInAppMode(it->command_id))
      continue;

    ui::Accelerator accelerator(it->keycode, it->modifiers);
    accelerator_table_[accelerator] = it->command_id;

    // Also register with the focus manager.
    focus_manager->RegisterAccelerator(
        accelerator, ui::AcceleratorManager::kNormalPriority, this);
  }
}

int BrowserView::GetCommandIDForAppCommandID(int app_command_id) const {
#if defined(OS_WIN)
  switch (app_command_id) {
    // NOTE: The order here matches the APPCOMMAND declaration order in the
    // Windows headers.
    case APPCOMMAND_BROWSER_BACKWARD: return IDC_BACK;
    case APPCOMMAND_BROWSER_FORWARD:  return IDC_FORWARD;
    case APPCOMMAND_BROWSER_REFRESH:  return IDC_RELOAD;
    case APPCOMMAND_BROWSER_HOME:     return IDC_HOME;
    case APPCOMMAND_BROWSER_STOP:     return IDC_STOP;
    case APPCOMMAND_BROWSER_SEARCH:   return IDC_FOCUS_SEARCH;
    case APPCOMMAND_HELP:             return IDC_HELP_PAGE_VIA_KEYBOARD;
    case APPCOMMAND_NEW:              return IDC_NEW_TAB;
    case APPCOMMAND_OPEN:             return IDC_OPEN_FILE;
    case APPCOMMAND_CLOSE:            return IDC_CLOSE_TAB;
    case APPCOMMAND_SAVE:             return IDC_SAVE_PAGE;
    case APPCOMMAND_PRINT:            return IDC_PRINT;
    case APPCOMMAND_COPY:             return IDC_COPY;
    case APPCOMMAND_CUT:              return IDC_CUT;
    case APPCOMMAND_PASTE:            return IDC_PASTE;

      // TODO(pkasting): http://b/1113069 Handle these.
    case APPCOMMAND_UNDO:
    case APPCOMMAND_REDO:
    case APPCOMMAND_SPELL_CHECK:
    default:                          return -1;
  }
#else
  // App commands are Windows-specific so there's nothing to do here.
  return -1;
#endif
}

void BrowserView::InitHangMonitor() {
#if defined(OS_WIN)
  PrefService* pref_service = g_browser_process->local_state();
  if (!pref_service)
    return;

  int plugin_message_response_timeout =
      pref_service->GetInteger(prefs::kPluginMessageResponseTimeout);
  int hung_plugin_detect_freq =
      pref_service->GetInteger(prefs::kHungPluginDetectFrequency);
  HWND window = GetWidget()->GetNativeView()->GetHost()->
      GetAcceleratedWidget();
  if ((hung_plugin_detect_freq > 0) &&
      hung_window_detector_.Initialize(window,
                                       plugin_message_response_timeout)) {
    ticker_.set_tick_interval(hung_plugin_detect_freq);
    ticker_.RegisterTickHandler(&hung_window_detector_);
    ticker_.Start();

    pref_service->SetInteger(prefs::kPluginMessageResponseTimeout,
                             plugin_message_response_timeout);
    pref_service->SetInteger(prefs::kHungPluginDetectFrequency,
                             hung_plugin_detect_freq);
  }
#endif
}

void BrowserView::UpdateAcceleratorMetrics(const ui::Accelerator& accelerator,
                                           int command_id) {
  const ui::KeyboardCode key_code = accelerator.key_code();
  if (command_id == IDC_HELP_PAGE_VIA_KEYBOARD && key_code == ui::VKEY_F1)
    content::RecordAction(UserMetricsAction("ShowHelpTabViaF1"));

  if (command_id == IDC_BOOKMARK_PAGE)
    UMA_HISTOGRAM_ENUMERATION("Bookmarks.EntryPoint",
                              BOOKMARK_ENTRY_POINT_ACCELERATOR,
                              BOOKMARK_ENTRY_POINT_LIMIT);

#if defined(OS_CHROMEOS)
  // Collect information about the relative popularity of various accelerators
  // on Chrome OS.
  switch (command_id) {
    case IDC_BACK:
      if (key_code == ui::VKEY_BACK)
        content::RecordAction(UserMetricsAction("Accel_Back_Backspace"));
      else if (key_code == ui::VKEY_BROWSER_BACK)
        content::RecordAction(UserMetricsAction("Accel_Back_F1"));
      else if (key_code == ui::VKEY_LEFT)
        content::RecordAction(UserMetricsAction("Accel_Back_Left"));
      break;
    case IDC_FORWARD:
      if (key_code == ui::VKEY_BACK)
        content::RecordAction(UserMetricsAction("Accel_Forward_Backspace"));
      else if (key_code == ui::VKEY_BROWSER_FORWARD)
        content::RecordAction(UserMetricsAction("Accel_Forward_F2"));
      else if (key_code == ui::VKEY_RIGHT)
        content::RecordAction(UserMetricsAction("Accel_Forward_Right"));
      break;
    case IDC_RELOAD:
    case IDC_RELOAD_IGNORING_CACHE:
      if (key_code == ui::VKEY_R)
        content::RecordAction(UserMetricsAction("Accel_Reload_R"));
      else if (key_code == ui::VKEY_BROWSER_REFRESH)
        content::RecordAction(UserMetricsAction("Accel_Reload_F3"));
      break;
    case IDC_FOCUS_LOCATION:
      if (key_code == ui::VKEY_D)
        content::RecordAction(UserMetricsAction("Accel_FocusLocation_D"));
      else if (key_code == ui::VKEY_L)
        content::RecordAction(UserMetricsAction("Accel_FocusLocation_L"));
      break;
    case IDC_FOCUS_SEARCH:
      if (key_code == ui::VKEY_E)
        content::RecordAction(UserMetricsAction("Accel_FocusSearch_E"));
      else if (key_code == ui::VKEY_K)
        content::RecordAction(UserMetricsAction("Accel_FocusSearch_K"));
      break;
    default:
      // Do nothing.
      break;
  }
#endif
}

void BrowserView::ShowAvatarBubbleFromAvatarButton(
    AvatarBubbleMode mode,
    const signin::ManageAccountsParams& manage_accounts_params,
    signin_metrics::AccessPoint access_point) {
#if defined(FRAME_AVATAR_BUTTON)
  // Do not show avatar bubble if there is no avatar menu button.
  if (!frame_->GetNewAvatarMenuButton())
    return;

  profiles::BubbleViewMode bubble_view_mode;
  profiles::TutorialMode tutorial_mode;
  profiles::BubbleViewModeFromAvatarBubbleMode(mode, &bubble_view_mode,
                                               &tutorial_mode);
  if (SigninViewController::ShouldShowModalSigninForMode(bubble_view_mode)) {
    ShowModalSigninWindow(mode, access_point);
  } else {
    ProfileChooserView::ShowBubble(
        bubble_view_mode, tutorial_mode, manage_accounts_params, access_point,
        frame_->GetNewAvatarMenuButton(), views::BubbleBorder::TOP_RIGHT,
        views::BubbleBorder::ALIGN_EDGE_TO_ANCHOR_EDGE, browser());
    ProfileMetrics::LogProfileOpenMethod(ProfileMetrics::ICON_AVATAR_BUBBLE);
  }
#else
  NOTREACHED();
#endif
}

void BrowserView::CloseModalSigninWindow() {
  signin_view_controller_.CloseModalSignin();
}

void BrowserView::ShowModalSigninWindow(
    AvatarBubbleMode mode,
    signin_metrics::AccessPoint access_point) {
  profiles::BubbleViewMode bubble_view_mode;
  profiles::TutorialMode tutorial_mode;
  profiles::BubbleViewModeFromAvatarBubbleMode(mode, &bubble_view_mode,
                                               &tutorial_mode);
  signin_view_controller_.ShowModalSignin(bubble_view_mode, browser(),
                                          access_point);
}

void BrowserView::ShowModalSyncConfirmationWindow() {
  signin_view_controller_.ShowModalSyncConfirmationDialog(browser());
}

int BrowserView::GetRenderViewHeightInsetWithDetachedBookmarkBar() {
  if (browser_->bookmark_bar_state() != BookmarkBar::DETACHED ||
      !bookmark_bar_view_ || !bookmark_bar_view_->IsDetached()) {
    return 0;
  }
  // Don't use bookmark_bar_view_->height() which won't be the final height if
  // the bookmark bar is animating.
  return chrome::kNTPBookmarkBarHeight -
      views::NonClientFrameView::kClientEdgeThickness;
}

void BrowserView::ExecuteExtensionCommand(
    const extensions::Extension* extension,
    const extensions::Command& command) {
  toolbar_->ExecuteExtensionCommand(extension, command);
}

ExclusiveAccessContext* BrowserView::GetExclusiveAccessContext() {
  return this;
}

bool BrowserView::DoCutCopyPasteForWebContents(
    WebContents* contents,
    void (WebContents::*method)()) {
  // It's possible for a non-null WebContents to have a null RWHV if it's
  // crashed or otherwise been killed.
  content::RenderWidgetHostView* rwhv = contents->GetRenderWidgetHostView();
  if (!rwhv || !rwhv->HasFocus())
    return false;
  // Calling |method| rather than using a fake key event is important since a
  // fake event might be consumed by the web content.
  (contents->*method)();
  return true;
}

void BrowserView::ActivateAppModalDialog() const {
  // If another browser is app modal, flash and activate the modal browser.
  app_modal::AppModalDialog* active_dialog =
      app_modal::AppModalDialogQueue::GetInstance()->active_dialog();
  if (!active_dialog)
    return;

  Browser* modal_browser =
      chrome::FindBrowserWithWebContents(active_dialog->web_contents());
  if (modal_browser && (browser_.get() != modal_browser)) {
    modal_browser->window()->FlashFrame(true);
    modal_browser->window()->Activate();
  }

  app_modal::AppModalDialogQueue::GetInstance()->ActivateModalDialog();
}

int BrowserView::GetMaxTopInfoBarArrowHeight() {
  int top_arrow_height = 0;
  // Only show the arrows when not in fullscreen and when there's no omnibox
  // popup.
  if (!IsFullscreen() &&
      !GetLocationBar()->GetOmniboxView()->model()->popup_model()->IsOpen()) {
    gfx::Point icon_bottom(
        toolbar_->location_bar()->GetLocationBarAnchorPoint());
    ConvertPointToTarget(toolbar_->location_bar(), this, &icon_bottom);
    gfx::Point infobar_top(0, infobar_container_->GetVerticalOverlap(nullptr));
    ConvertPointToTarget(infobar_container_, this, &infobar_top);
    top_arrow_height = infobar_top.y() - icon_bottom.y();
  }
  return top_arrow_height;
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, ExclusiveAccessContext overrides
Profile* BrowserView::GetProfile() {
  return browser_->profile();
}

WebContents* BrowserView::GetActiveWebContents() {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

void BrowserView::UnhideDownloadShelf() {
  if (download_shelf_)
    download_shelf_->Unhide();
}

void BrowserView::HideDownloadShelf() {
  if (download_shelf_)
    download_shelf_->Hide();

  StatusBubble* status_bubble = GetStatusBubble();
  if (status_bubble)
    status_bubble->Hide();
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, ExclusiveAccessBubbleViewsContext overrides
ExclusiveAccessManager* BrowserView::GetExclusiveAccessManager() {
  return browser_->exclusive_access_manager();
}

bool BrowserView::IsImmersiveModeEnabled() {
  return immersive_mode_controller()->IsEnabled();
}

views::Widget* BrowserView::GetBubbleAssociatedWidget() {
  return GetWidget();
}

gfx::Rect BrowserView::GetTopContainerBoundsInScreen() {
  return top_container_->GetBoundsInScreen();
}
