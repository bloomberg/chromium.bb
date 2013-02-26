// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_view.h"

#include <algorithm>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/i18n/rtl.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/api/infobars/infobar_service.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/managed_mode/managed_mode.h"
#include "chrome/browser/native_window_notification_source.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/profiles/avatar_menu_model.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/speech/tts_controller.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/app_modal_dialogs/app_modal_dialog.h"
#include "chrome/browser/ui/app_modal_dialogs/app_modal_dialog_queue.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_instant_controller.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window_state.h"
#include "chrome/browser/ui/ntp_background_util.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_model.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_view.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/search/search.h"
#include "chrome/browser/ui/search/search_delegate.h"
#include "chrome/browser/ui/search/search_model.h"
#include "chrome/browser/ui/search/search_ui.h"
#include "chrome/browser/ui/tabs/tab_menu_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/accelerator_table.h"
#include "chrome/browser/ui/views/accessibility/invert_bubble_view.h"
#include "chrome/browser/ui/views/avatar_menu_bubble_view.h"
#include "chrome/browser/ui/views/avatar_menu_button.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/browser_dialogs.h"
#include "chrome/browser/ui/views/download/download_in_progress_dialog_view.h"
#include "chrome/browser/ui/views/download/download_shelf_view.h"
#include "chrome/browser/ui/views/frame/browser_view_layout.h"
#include "chrome/browser/ui/views/frame/contents_container.h"
#include "chrome/browser/ui/views/frame/instant_preview_controller_views.h"
#include "chrome/browser/ui/views/fullscreen_exit_bubble_views.h"
#include "chrome/browser/ui/views/immersive_mode_controller.h"
#include "chrome/browser/ui/views/infobars/infobar_container_view.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/omnibox/omnibox_views.h"
#include "chrome/browser/ui/views/password_generation_bubble_view.h"
#include "chrome/browser/ui/views/status_bubble_views.h"
#include "chrome/browser/ui/views/tabs/browser_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar_view.h"
#include "chrome/browser/ui/views/update_recommended_message_box.h"
#include "chrome/browser/ui/views/website_settings/website_settings_popup_view.h"
#include "chrome/browser/ui/window_sizer/window_sizer.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_resource.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/common/content_switches.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "grit/ui_resources.h"
#include "grit/ui_strings.h"
#include "grit/webkit_resources.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/events/event_utils.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/sys_color_change_listener.h"
#include "ui/views/controls/single_split_view.h"
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
#include "ash/ash_switches.h"
#include "ash/launcher/launcher.h"
#include "ash/launcher/launcher_model.h"
#include "ash/shell.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/ash/chrome_shell_delegate.h"
#include "chrome/browser/ui/ash/launcher/browser_launcher_item_controller.h"
#include "chrome/browser/ui/ash/window_positioner.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/window.h"
#include "ui/gfx/screen.h"
#elif defined(OS_WIN)  // !defined(USE_AURA)
#include "chrome/browser/jumplist_win.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_win.h"
#include "ui/views/widget/native_widget_win.h"
#include "ui/views/win/scoped_fullscreen_visibility.h"
#endif

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "win8/util/win8_util.h"
#endif

#if defined(ENABLE_ONE_CLICK_SIGNIN)
#include "chrome/browser/ui/views/sync/one_click_signin_bubble_view.h"
#endif

using base::TimeDelta;
using content::NativeWebKeyboardEvent;
using content::SSLStatus;
using content::UserMetricsAction;
using content::WebContents;
using views::ColumnSet;
using views::GridLayout;

namespace {
// The height of the status bubble.
const int kStatusBubbleHeight = 20;
// The name of a key to store on the window handle so that other code can
// locate this object using just the handle.
const char* const kBrowserViewKey = "__BROWSER_VIEW__";

// The number of milliseconds between loading animation frames.
const int kLoadingAnimationFrameTimeMs = 30;
// The amount of space we expect the window border to take up.
const int kWindowBorderWidth = 5;

// How round the 'new tab' style bookmarks bar is.
const int kNewtabBarRoundness = 5;

}  // namespace

// Returned from BrowserView::GetClassName.
const char BrowserView::kViewClassName[] = "browser/ui/views/BrowserView";
// static
const int BrowserView::kTabstripIndex = 0;
const int BrowserView::kInfoBarIndex = 1;
const int BrowserView::kToolbarIndex = 2;

namespace {

bool ShouldSaveOrRestoreWindowPos() {
#if defined(OS_WIN)
  // In Windows 8's single window Metro mode the window is always maximized
  // (without the WS_MAXIMIZE style).
  if (win8::IsSingleWindowMetroMode())
    return false;
#endif
  return true;
}

// Returns whether immersive mode should replace fullscreen, which should only
// occur for "browser-fullscreen" and not for "tab-fullscreen" (which has a URL
// for the tab entering fullscreen).
bool UseImmersiveFullscreen(const GURL& url) {
#if defined(USE_ASH)
  bool is_browser_fullscreen = url.is_empty();
  return is_browser_fullscreen &&
      CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kAshImmersiveMode);
#endif
  return false;
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// BookmarkExtensionBackground, private:
// This object serves as the views::Background object which is used to layout
// and paint the bookmark bar.
class BookmarkExtensionBackground : public views::Background {
 public:
  BookmarkExtensionBackground(BrowserView* browser_view,
                              DetachableToolbarView* host_view,
                              Browser* browser);

  // View methods overridden from views:Background.
  virtual void Paint(gfx::Canvas* canvas, views::View* view) const OVERRIDE;

 private:
  BrowserView* browser_view_;

  // The view hosting this background.
  DetachableToolbarView* host_view_;

  Browser* browser_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkExtensionBackground);
};

BookmarkExtensionBackground::BookmarkExtensionBackground(
    BrowserView* browser_view,
    DetachableToolbarView* host_view,
    Browser* browser)
    : browser_view_(browser_view),
      host_view_(host_view),
      browser_(browser) {
}

void BookmarkExtensionBackground::Paint(gfx::Canvas* canvas,
                                        views::View* view) const {
  ui::ThemeProvider* tp = host_view_->GetThemeProvider();
  int toolbar_overlap = host_view_->GetToolbarOverlap();
  // The client edge is drawn below the toolbar bounds.
  if (toolbar_overlap)
    toolbar_overlap += views::NonClientFrameView::kClientEdgeThickness;
  if (host_view_->IsDetached()) {
    // Draw the background to match the new tab page.
    int height = 0;
    WebContents* contents = browser_->tab_strip_model()->GetActiveWebContents();
    if (contents && contents->GetView())
      height = contents->GetView()->GetContainerSize().height();
    NtpBackgroundUtil::PaintBackgroundDetachedMode(
        tp, canvas,
        gfx::Rect(0, toolbar_overlap, host_view_->width(),
                  host_view_->height() - toolbar_overlap),
        height);

    // As 'hidden' according to the animation is the full in-tab state,
    // we invert the value - when current_state is at '0', we expect the
    // bar to be docked.
    double current_state = 1 - host_view_->GetAnimationValue();
    double h_padding =
        static_cast<double>(BookmarkBarView::kNewtabHorizontalPadding) *
        current_state;
    double v_padding =
        static_cast<double>(BookmarkBarView::kNewtabVerticalPadding) *
        current_state;

    SkRect rect;
    double roundness = 0;
    DetachableToolbarView::CalculateContentArea(current_state, h_padding,
        v_padding, &rect, &roundness, host_view_);
    DetachableToolbarView::PaintContentAreaBackground(canvas, tp, rect,
                                                      roundness);
    DetachableToolbarView::PaintContentAreaBorder(canvas, tp, rect, roundness);
    if (!toolbar_overlap)
      DetachableToolbarView::PaintHorizontalBorder(canvas, host_view_);
  } else {
    DetachableToolbarView::PaintBackgroundAttachedMode(canvas, host_view_,
        browser_view_->OffsetPointForToolbarBackgroundImage(
            gfx::Point(host_view_->GetMirroredX(), host_view_->y())),
        browser_->host_desktop_type());
    if (host_view_->height() >= toolbar_overlap)
      DetachableToolbarView::PaintHorizontalBorder(canvas, host_view_);
  }
}

///////////////////////////////////////////////////////////////////////////////
// ResizeCorner, private:

class ResizeCorner : public views::View {
 public:
  ResizeCorner() {
    EnableCanvasFlippingForRTLUI(true);
  }

  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    views::Widget* widget = GetWidget();
    if (!widget || (widget->IsMaximized() || widget->IsFullscreen()))
      return;

    gfx::ImageSkia* image = ui::ResourceBundle::GetSharedInstance().
        GetImageSkiaNamed(IDR_TEXTAREA_RESIZER);
    canvas->DrawImageInt(*image, width() - image->width(),
                         height() - image->height());
  }

  static gfx::Size GetSize() {
    // This is disabled until we find what makes us slower when we let
    // WebKit know that we have a resizer rect...
    // int scrollbar_thickness = gfx::scrollbar_size();
    // return gfx::Size(scrollbar_thickness, scrollbar_thickness);
    return gfx::Size();
  }

  virtual gfx::Size GetPreferredSize() OVERRIDE {
    views::Widget* widget = GetWidget();
    return (!widget || widget->IsMaximized() || widget->IsFullscreen()) ?
        gfx::Size() : GetSize();
  }

  virtual void Layout() OVERRIDE {
    if (parent()) {
      gfx::Size ps = GetPreferredSize();
      // No need to handle Right to left text direction here,
      // our parent must take care of it for us...
      SetBounds(parent()->width() - ps.width(),
                parent()->height() - ps.height(), ps.width(), ps.height());
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ResizeCorner);
};

///////////////////////////////////////////////////////////////////////////////
// BrowserView, public:

BrowserView::BrowserView(Browser* browser)
    : views::ClientView(NULL, NULL),
      last_focused_view_storage_id_(
          views::ViewStorage::GetInstance()->CreateStorageID()),
      frame_(NULL),
      browser_(browser),
      active_bookmark_bar_(NULL),
      tabstrip_(NULL),
      toolbar_(NULL),
      window_switcher_button_(NULL),
      infobar_container_(NULL),
      contents_container_(NULL),
      devtools_container_(NULL),
      contents_(NULL),
      contents_split_(NULL),
      devtools_dock_side_(DEVTOOLS_DOCK_SIDE_BOTTOM),
      devtools_window_(NULL),
      initialized_(false),
      ignore_layout_(true),
#if defined(OS_WIN) && !defined(USE_AURA)
      hung_window_detector_(&hung_plugin_action_),
      ticker_(0),
#endif
      force_location_bar_focus_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          immersive_mode_controller_(new ImmersiveModeController(this))),
      ALLOW_THIS_IN_INITIALIZER_LIST(color_change_listener_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(activate_modal_dialog_factory_(this)) {
  browser_->tab_strip_model()->AddObserver(this);
}

BrowserView::~BrowserView() {
#if defined(USE_ASH)
  // Destroy BrowserLauncherItemController early on as it listens to the
  // TabstripModel, which is destroyed by the browser.
  launcher_item_controller_.reset();
#endif

  // Immersive mode may need to reparent views before they are removed/deleted.
  immersive_mode_controller_.reset();

  preview_controller_.reset();

  browser_->tab_strip_model()->RemoveObserver(this);

#if defined(OS_WIN) && !defined(USE_AURA)
  // Stop hung plugin monitoring.
  ticker_.Stop();
  ticker_.UnregisterTickHandler(&hung_window_detector_);

  // Terminate the jumplist (must be called before browser_->profile() is
  // destroyed.
  if (jumplist_) {
    jumplist_->Terminate();
  }
#endif

  // We destroy the download shelf before |browser_| to remove its child
  // download views from the set of download observers (since the observed
  // downloads can be destroyed along with |browser_| and the observer
  // notifications will call back into deleted objects).
  download_shelf_.reset();

  // The TabStrip attaches a listener to the model. Make sure we shut down the
  // TabStrip first so that it can cleanly remove the listener.
  if (tabstrip_) {
    tabstrip_->parent()->RemoveChildView(tabstrip_);
    delete tabstrip_;
    tabstrip_ = NULL;
  }
  // Child views maintain PrefMember attributes that point to
  // OffTheRecordProfile's PrefService which gets deleted by ~Browser.
  RemoveAllChildViews(true);

  // It is possible that we were forced-closed by the native view system and
  // that tabs remain in the browser. Close any such remaining tabs.
  while (browser_->tab_strip_model()->count())
    delete browser_->tab_strip_model()->GetWebContentsAt(0);

  // Explicitly set browser_ to NULL.
  browser_.reset();
}

#if defined(OS_WIN) || defined(USE_AURA)
// static
BrowserView* BrowserView::GetBrowserViewForNativeWindow(
    gfx::NativeWindow window) {
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
  return widget ?
      reinterpret_cast<BrowserView*>(widget->GetNativeWindowProperty(
          kBrowserViewKey)) : NULL;
}
#endif

// static
BrowserView* BrowserView::GetBrowserViewForBrowser(const Browser* browser) {
  return static_cast<BrowserView*>(browser->window());
}

gfx::Rect BrowserView::GetToolbarBounds() const {
  gfx::Rect toolbar_bounds(toolbar_->bounds());
  if (toolbar_bounds.IsEmpty())
    return toolbar_bounds;
  // The apparent toolbar edges are outside the "real" toolbar edges.
  toolbar_bounds.Inset(-views::NonClientFrameView::kClientEdgeThickness, 0);
  return toolbar_bounds;
}

gfx::Rect BrowserView::GetClientAreaBounds() const {
  gfx::Rect container_bounds = contents_->bounds();
  gfx::Point container_origin = container_bounds.origin();
  ConvertPointToTarget(this, parent(), &container_origin);
  container_bounds.set_origin(container_origin);
  return container_bounds;
}

gfx::Rect BrowserView::GetFindBarBoundingBox() const {
  return GetBrowserViewLayout()->GetFindBarBoundingBox();
}

int BrowserView::GetTabStripHeight() const {
  // We want to return tabstrip_->height(), but we might be called in the midst
  // of layout, when that hasn't yet been updated to reflect the current state.
  // So return what the tabstrip height _ought_ to be right now.
  return IsTabStripVisible() ? tabstrip_->GetPreferredSize().height() : 0;
}

gfx::Point BrowserView::OffsetPointForToolbarBackgroundImage(
    const gfx::Point& point) const {
  // The background image starts tiling horizontally at the window left edge and
  // vertically at the top edge of the horizontal tab strip (or where it would
  // be).  We expect our parent's origin to be the window origin.
  gfx::Point window_point(point + GetMirroredPosition().OffsetFromOrigin());
  window_point.Offset(frame_->GetThemeBackgroundXInset(),
                      -frame_->GetTabStripInsets(false).top);
  return window_point;
}

bool BrowserView::IsTabStripVisible() const {
  if (immersive_mode_controller_->ShouldHideTopViews() &&
      immersive_mode_controller_->hide_tab_indicators())
    return false;
  return browser_->SupportsWindowFeature(Browser::FEATURE_TABSTRIP);
}

bool BrowserView::IsOffTheRecord() const {
  return browser_->profile()->IsOffTheRecord();
}

bool BrowserView::IsGuestSession() const {
  return browser_->profile()->IsGuestSession();
}

bool BrowserView::ShouldShowAvatar() const {
  if (!IsBrowserTypeNormal())
    return false;
  if (IsOffTheRecord() && !IsGuestSession())
    return true;
  if (ManagedMode::IsInManagedMode())
    return true;

  ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  if (cache.GetIndexOfProfileWithPath(browser_->profile()->GetPath()) ==
      std::string::npos) {
    return false;
  }

  return AvatarMenuModel::ShouldShowAvatarMenu();
}

bool BrowserView::AcceleratorPressed(const ui::Accelerator& accelerator) {
#if defined(OS_CHROMEOS)
  // If accessibility is enabled, stop speech and return false so that key
  // combinations involving Search can be used for extra accessibility
  // functionality.
  if (accelerator.key_code() == ui::VKEY_LWIN &&
      g_browser_process->local_state()->GetBoolean(
          prefs::kSpokenFeedbackEnabled)) {
    TtsController::GetInstance()->Stop();
    return false;
  }
#endif

  std::map<ui::Accelerator, int>::const_iterator iter =
      accelerator_table_.find(accelerator);
  DCHECK(iter != accelerator_table_.end());
  int command_id = iter->second;

  chrome::BrowserCommandController* controller = browser_->command_controller();
  if (!controller->block_command_execution())
    UpdateAcceleratorMetrics(accelerator, command_id);
  return chrome::ExecuteCommand(browser_.get(), command_id);
}

bool BrowserView::GetAccelerator(int cmd_id, ui::Accelerator* accelerator) {
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

WebContents* BrowserView::GetActiveWebContents() const {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

gfx::ImageSkia BrowserView::GetOTRAvatarIcon() const {
  return *GetThemeProvider()->GetImageSkiaNamed(GetOTRIconResourceID());
}

bool BrowserView::IsPositionInWindowCaption(const gfx::Point& point) {
  if (window_switcher_button_) {
    gfx::Point window_switcher_point(point);
    views::View::ConvertPointToTarget(this, window_switcher_button_,
                                      &window_switcher_point);
    if (window_switcher_button_->HitTestPoint(window_switcher_point))
      return false;
  }
  return GetBrowserViewLayout()->IsPositionInWindowCaption(point);
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, BrowserWindow implementation:

void BrowserView::Show() {
  // If the window is already visible, just activate it.
  if (frame_->IsVisible()) {
    frame_->Activate();
    return;
  }

  CreateLauncherIcon();

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

  chrome::MaybeShowInvertBubbleView(browser_.get(), contents_);
}

void BrowserView::ShowInactive() {
  if (frame_->IsVisible())
    return;
  CreateLauncherIcon();
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

gfx::NativeWindow BrowserView::GetNativeWindow() {
  // While the browser destruction is going on, the widget can already be gone,
  // but utility functions like FindBrowserWithWindow will come here and crash.
  // We short circuit therefore.
  if (!GetWidget())
    return NULL;
  return GetWidget()->GetTopLevelWidget()->GetNativeWindow();
}

BrowserWindowTesting* BrowserView::GetBrowserWindowTesting() {
  return this;
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
  frame_->UpdateWindowTitle();
  if (ShouldShowWindowIcon() && !loading_animation_timer_.IsRunning())
    frame_->UpdateWindowIcon();
}

void BrowserView::BookmarkBarStateChanged(
    BookmarkBar::AnimateChangeType change_type) {
  if (bookmark_bar_view_.get()) {
    bookmark_bar_view_->SetBookmarkBarState(
        browser_->bookmark_bar_state(), change_type);
  }
  if (MaybeShowBookmarkBar(GetActiveWebContents()))
    Layout();
}

void BrowserView::UpdateDevTools() {
  UpdateDevToolsForContents(GetActiveWebContents());
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

void BrowserView::ZoomChangedForActiveTab(bool can_show_bubble) {
  GetLocationBarView()->ZoomChangedForActiveTab(
      can_show_bubble && !toolbar_->IsWrenchMenuShowing());
}

gfx::Rect BrowserView::GetRestoredBounds() const {
  return frame_->GetRestoredBounds();
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

void BrowserView::EnterFullscreen(
    const GURL& url, FullscreenExitBubbleType bubble_type) {
  if (IsFullscreen())
    return;  // Nothing to do.

  ProcessFullscreen(true, FOR_DESKTOP, url, bubble_type);
}

void BrowserView::ExitFullscreen() {
  if (!IsFullscreen())
    return;  // Nothing to do.

  ProcessFullscreen(false, FOR_DESKTOP, GURL(), FEB_TYPE_NONE);
}

void BrowserView::UpdateFullscreenExitBubbleContent(
    const GURL& url,
    FullscreenExitBubbleType bubble_type) {
  // Immersive mode has no exit bubble because it has a visible strip at the
  // top that gives the user a hover target.
  // TODO(jamescook): Figure out what to do with mouse-lock.
  if (bubble_type == FEB_TYPE_NONE || UseImmersiveFullscreen(url)) {
    fullscreen_bubble_.reset();
  } else if (fullscreen_bubble_.get()) {
    fullscreen_bubble_->UpdateContent(url, bubble_type);
  } else {
    fullscreen_bubble_.reset(new FullscreenExitBubbleViews(
        GetWidget(), browser_.get(), url, bubble_type));
  }
}

bool BrowserView::ShouldHideUIForFullscreen() const {
#if defined(USE_ASH)
  // Immersive reveal needs UI for the slide-down top panel.
  return IsFullscreen() && !immersive_mode_controller_->IsRevealed();
#endif
  return IsFullscreen();
}

bool BrowserView::IsFullscreen() const {
  return frame_->IsFullscreen();
}

bool BrowserView::IsFullscreenBubbleVisible() const {
  return fullscreen_bubble_ != NULL;
}

void BrowserView::FullScreenStateChanged() {
  if (IsFullscreen()) {
    if (fullscreen_request_.pending) {
      fullscreen_request_.pending = false;
      ProcessFullscreen(true, FOR_DESKTOP,
                        fullscreen_request_.url,
                        fullscreen_request_.bubble_type);
    } else {
      ProcessFullscreen(true, FOR_DESKTOP, GURL(),
                        FEB_TYPE_BROWSER_FULLSCREEN_EXIT_INSTRUCTION);
    }
  } else {
    ProcessFullscreen(false, FOR_DESKTOP, GURL(), FEB_TYPE_NONE);
  }
}

#if defined(OS_WIN)
void BrowserView::SetMetroSnapMode(bool enable) {
  HISTOGRAM_COUNTS("Metro.SnapModeToggle", enable);
  ProcessFullscreen(enable, FOR_METRO, GURL(), FEB_TYPE_NONE);
}

bool BrowserView::IsInMetroSnapMode() const {
#if defined(USE_AURA)
  return false;
#else
  return static_cast<views::NativeWidgetWin*>(
      frame_->native_widget())->IsInMetroSnapMode();
#endif
}
#endif  // defined(OS_WIN)

void BrowserView::RestoreFocus() {
  WebContents* selected_web_contents = GetActiveWebContents();
  if (selected_web_contents)
    selected_web_contents->GetView()->RestoreFocus();
}

void BrowserView::SetWindowSwitcherButton(views::Button* button) {
  if (window_switcher_button_)
    RemoveChildView(window_switcher_button_);
  window_switcher_button_ = button;
  AddChildView(button);
}

void BrowserView::ToolbarSizeChanged(bool is_animating) {
  // The call to InfoBarContainer::SetMaxTopArrowHeight() below can result in
  // reentrancy; |call_state| tracks whether we're reentrant.  We can't just
  // early-return in this case because we need to layout again so the infobar
  // container's bounds are set correctly.
  static CallState call_state = NORMAL;

  // A reentrant call can (and should) use the fast resize path unless both it
  // and the normal call are both non-animating.
  bool use_fast_resize =
      is_animating || (call_state == REENTRANT_FORCE_FAST_RESIZE);
  if (use_fast_resize)
    contents_container_->SetFastResize(true);
  UpdateUIForContents(GetActiveWebContents());
  if (use_fast_resize)
    contents_container_->SetFastResize(false);

  // Inform the InfoBarContainer that the distance to the location icon may have
  // changed.  We have to do this after the block above so that the toolbars are
  // laid out correctly for calculating the maximum arrow height below.
  {
    int top_arrow_height = 0;
    // Hide the arrows on the Instant Extended NTP.
    if (!chrome::search::IsInstantExtendedAPIEnabled(browser()->profile()) ||
        !browser()->search_model()->mode().is_ntp()) {
      const LocationIconView* location_icon_view =
          toolbar_->location_bar()->location_icon_view();
      // The +1 in the next line creates a 1-px gap between icon and arrow tip.
      gfx::Point icon_bottom(0, location_icon_view->GetImageBounds().bottom() -
          LocationBarView::kIconInternalPadding + 1);
      ConvertPointToTarget(location_icon_view, this, &icon_bottom);
      gfx::Point infobar_top(0, infobar_container_->GetVerticalOverlap(NULL));
      ConvertPointToTarget(infobar_container_, this, &infobar_top);
      top_arrow_height = infobar_top.y() - icon_bottom.y();
    }
    base::AutoReset<CallState> resetter(&call_state,
        is_animating ? REENTRANT_FORCE_FAST_RESIZE : REENTRANT);
    infobar_container_->SetMaxTopArrowHeight(top_arrow_height);
  }

  // When transitioning from animating to not animating we need to make sure the
  // contents_container_ gets layed out. If we don't do this and the bounds
  // haven't changed contents_container_ won't get a Layout out and we'll end up
  // with a gray rect because the clip wasn't updated.  Note that a reentrant
  // call never needs to do this, because after it returns, the normal call
  // wrapping it will do it.
  if ((call_state == NORMAL) && !is_animating) {
    contents_container_->InvalidateLayout();
    contents_split_->Layout();
  }
}

void BrowserView::MaybeStackImmersiveRevealAtTop() {
  if (immersive_mode_controller_)
    immersive_mode_controller_->MaybeStackViewAtTop();
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
  if (!force_location_bar_focus_ && !IsActive())
    return;

  // The location bar view must be visible for it to be considered focusable,
  // so always reveal it before testing for focusable.
  immersive_mode_controller_->MaybeStartReveal();

  LocationBarView* location_bar = GetLocationBarView();
  if (location_bar->IsLocationEntryFocusableInRootView()) {
    // Location bar got focus.
    location_bar->FocusLocation(select_all);
  } else {
    // If none of location bar got focus,
    // then clear focus.
    views::FocusManager* focus_manager = GetFocusManager();
    DCHECK(focus_manager);
    focus_manager->ClearFocus();
    // The view doesn't need to be revealed after all.
    immersive_mode_controller_->CancelReveal();
  }
}

void BrowserView::UpdateReloadStopState(bool is_loading, bool force) {
  toolbar_->reload_button()->ChangeMode(
      is_loading ? ReloadButton::MODE_STOP : ReloadButton::MODE_RELOAD, force);
}

void BrowserView::UpdateToolbar(content::WebContents* contents,
                                bool should_restore_state) {
  toolbar_->Update(contents, should_restore_state);
}

void BrowserView::FocusToolbar() {
  immersive_mode_controller_->MaybeStartReveal();
  // Start the traversal within the main toolbar. SetPaneFocus stores
  // the current focused view before changing focus.
  toolbar_->SetPaneFocus(NULL);
}

void BrowserView::FocusBookmarksToolbar() {
  if (active_bookmark_bar_ && bookmark_bar_view_->visible()) {
    immersive_mode_controller_->MaybeStartReveal();
    bookmark_bar_view_->SetPaneFocus(bookmark_bar_view_.get());
  }
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
    immersive_mode_controller_->MaybeStartReveal();
    toolbar_->SetPaneFocusAndFocusAppMenu();
  }
}

void BrowserView::RotatePaneFocus(bool forwards) {
  // This gets called when the user presses F6 (forwards) or Shift+F6
  // (backwards) to rotate to the next pane. Here, our "panes" are the
  // tab contents and each of our accessible toolbars, infobars, downloads
  // shelf, etc.  When a pane has focus, all of its controls are accessible
  // in the tab traversal, and the tab traversal is "trapped" within that pane.
  //
  // Get a vector of all panes in the order we want them to be focused,
  // with NULL to represent the tab contents getting focus. If one of these
  // is currently invisible or has no focusable children it will be
  // automatically skipped.
  std::vector<views::AccessiblePaneView*> accessible_panes;
  GetAccessiblePanes(&accessible_panes);
  int pane_count = static_cast<int>(accessible_panes.size());
  int special_index = -1;

  std::vector<views::View*> accessible_views(
      accessible_panes.begin(), accessible_panes.end());
  accessible_views.push_back(GetTabContentsContainerView());
  if (devtools_container_->visible())
    accessible_views.push_back(devtools_container_);
  int count = static_cast<int>(accessible_views.size());

  // Figure out which view (if any) currently has the focus.
  const views::View* focused_view = GetFocusManager()->GetFocusedView();
  int index = -1;
  if (focused_view) {
    for (int i = 0; i < count; ++i) {
      if (accessible_views[i] == focused_view ||
          accessible_views[i]->Contains(focused_view)) {
        index = i;
        break;
      }
    }
  }

  // If the focus isn't currently in a pane, save the focus so we
  // can restore it if the user presses Escape.
  if (focused_view && index >= pane_count)
    GetFocusManager()->StoreFocusedView(true);

#if defined(OS_CHROMEOS) && defined(USE_AURA)
  // Add the special panes to the rotation.
  special_index = count;
  ++count;
#endif

  // Try to focus the next pane; if SetPaneFocusAndFocusDefault returns
  // false it means the pane didn't have any focusable controls, so skip
  // it and try the next one.
  for (;;) {
    if (forwards)
      index = (index + 1) % count;
    else
      index = ((index - 1) + count) % count;

    if (index == special_index) {
#if defined(USE_ASH)
      ash::Shell::GetInstance()->RotateFocus(
          forwards ? ash::Shell::FORWARD : ash::Shell::BACKWARD);
#endif
      break;
    } else if (index < pane_count) {
      if (accessible_panes[index]->SetPaneFocusAndFocusDefault())
        break;
    } else {
      accessible_views[index]->RequestFocus();
      break;
    }
  }
}

void BrowserView::DestroyBrowser() {
  // After this returns other parts of Chrome are going to be shutdown. Close
  // the window now so that we are deleted immediately and aren't left holding
  // references to deleted objects.
  GetWidget()->RemoveObserver(this);
  frame_->CloseNow();
}

bool BrowserView::IsBookmarkBarVisible() const {
  if (immersive_mode_controller_->ShouldHideTopViews())
    return false;
  return browser_->SupportsWindowFeature(Browser::FEATURE_BOOKMARKBAR) &&
      active_bookmark_bar_ &&
      (active_bookmark_bar_->GetPreferredSize().height() != 0);
}

bool BrowserView::IsBookmarkBarAnimating() const {
  return bookmark_bar_view_.get() && bookmark_bar_view_->is_animating();
}

bool BrowserView::IsTabStripEditable() const {
  return tabstrip_->IsTabStripEditable();
}

bool BrowserView::IsToolbarVisible() const {
  if (immersive_mode_controller_->ShouldHideTopViews())
    return false;
  return browser_->SupportsWindowFeature(Browser::FEATURE_TOOLBAR) ||
         browser_->SupportsWindowFeature(Browser::FEATURE_LOCATIONBAR);
}

gfx::Rect BrowserView::GetRootWindowResizerRect() const {
  if (frame_->IsMaximized() || frame_->IsFullscreen())
    return gfx::Rect();

  // We don't specify a resize corner size if we have a bottom shelf either.
  // This is because we take care of drawing the resize corner on top of that
  // shelf, so we don't want others to do it for us in this case.
  // Currently, the only visible bottom shelf is the download shelf.
  // Other tests should be added here if we add more bottom shelves.
  if (IsDownloadShelfVisible())
    return gfx::Rect();

  gfx::Rect client_rect = contents_split_->bounds();
  gfx::Size resize_corner_size = ResizeCorner::GetSize();
  int x = client_rect.width() - resize_corner_size.width();
  if (base::i18n::IsRTL())
    x = 0;
  return gfx::Rect(x, client_rect.height() - resize_corner_size.height(),
                   resize_corner_size.width(), resize_corner_size.height());
}

bool BrowserView::IsPanel() const {
  return false;
}

void BrowserView::DisableInactiveFrame() {
#if defined(OS_WIN) && !defined(USE_AURA)
  frame_->DisableInactiveRendering();
#endif  // No tricks are needed to get the right behavior on Linux.
}

void BrowserView::ConfirmAddSearchProvider(TemplateURL* template_url,
                                           Profile* profile) {
  chrome::EditSearchEngine(GetWidget()->GetNativeWindow(), template_url, NULL,
                           profile);
}

void BrowserView::ToggleBookmarkBar() {
  chrome::ToggleBookmarkBarWhenVisible(browser_->profile());
}

void BrowserView::ShowUpdateChromeDialog() {
  UpdateRecommendedMessageBox::Show(GetWidget()->GetNativeWindow());
}

void BrowserView::ShowTaskManager() {
  chrome::ShowTaskManager(browser());
}

void BrowserView::ShowBackgroundPages() {
  chrome::ShowBackgroundPages(browser());
}

void BrowserView::ShowBookmarkBubble(const GURL& url, bool already_bookmarked) {
  chrome::ShowBookmarkBubbleView(GetToolbarView()->GetBookmarkBubbleAnchor(),
                                 bookmark_bar_view_.get(), browser_->profile(),
                                 url, !already_bookmarked);
}

void BrowserView::ShowBookmarkPrompt() {
  GetLocationBarView()->ShowBookmarkPrompt();
}

void BrowserView::ShowChromeToMobileBubble() {
  GetLocationBarView()->ShowChromeToMobileBubble();
}

#if defined(ENABLE_ONE_CLICK_SIGNIN)
void BrowserView::ShowOneClickSigninBubble(
    OneClickSigninBubbleType type,
    const StartSyncCallback& start_sync_callback) {
  OneClickSigninBubbleView::ShowBubble(type, toolbar_, start_sync_callback);
}
#endif

void BrowserView::SetDownloadShelfVisible(bool visible) {
  // This can be called from the superclass destructor, when it destroys our
  // child views. At that point, browser_ is already gone.
  if (browser_ == NULL)
    return;

  if (visible && IsDownloadShelfVisible() != visible) {
    // Invoke GetDownloadShelf to force the shelf to be created.
    GetDownloadShelf();
  }

  if (browser_ != NULL)
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
  if (!download_shelf_.get()) {
    download_shelf_.reset(new DownloadShelfView(browser_.get(), this));
    download_shelf_->set_owned_by_client();
  }
  return download_shelf_.get();
}

void BrowserView::ConfirmBrowserCloseWithPendingDownloads() {
  DownloadInProgressDialogView::Show(browser_.get(), GetNativeWindow());
}

void BrowserView::ShowCreateChromeAppShortcutsDialog(
    Profile* profile,
    const extensions::Extension* app) {
  chrome::ShowCreateChromeAppShortcutsDialog(GetNativeWindow(), profile, app);
}

void BrowserView::UserChangedTheme() {
  frame_->FrameTypeChanged();
}

int BrowserView::GetExtraRenderViewHeight() const {
  // Currently this is only used on linux.
  return 0;
}

void BrowserView::WebContentsFocused(WebContents* contents) {
  if (contents_container_->GetWebContents() == contents)
    contents_container_->OnWebContentsFocused(contents);
  else if (contents_->preview_web_contents() == contents)
    preview_controller_->preview()->OnWebContentsFocused(contents);
  else
    devtools_container_->OnWebContentsFocused(contents);
}

void BrowserView::ShowWebsiteSettings(Profile* profile,
                                      content::WebContents* web_contents,
                                      const GURL& url,
                                      const content::SSLStatus& ssl,
                                      bool show_history) {
  WebsiteSettingsPopupView::ShowPopup(
      GetLocationBarView()->location_icon_view(), profile,
      web_contents, url, ssl, browser_.get());
}

void BrowserView::ShowAppMenu() {
  toolbar_->app_menu()->Activate();
}

bool BrowserView::PreHandleKeyboardEvent(const NativeWebKeyboardEvent& event,
                                         bool* is_keyboard_shortcut) {
  *is_keyboard_shortcut = false;

  if ((event.type != WebKit::WebInputEvent::RawKeyDown) &&
      (event.type != WebKit::WebInputEvent::KeyUp)) {
    return false;
  }

#if defined(OS_WIN) && !defined(USE_AURA)
  // As Alt+F4 is the close-app keyboard shortcut, it needs processing
  // immediately.
  if (event.windowsKeyCode == ui::VKEY_F4 &&
      event.type == WebKit::WebInputEvent::RawKeyDown &&
      event.modifiers == NativeWebKeyboardEvent::AltKey) {
    DefWindowProc(event.os_event.hwnd, event.os_event.message,
                  event.os_event.wParam, event.os_event.lParam);
    return true;
  }
#endif

  views::FocusManager* focus_manager = GetFocusManager();
  DCHECK(focus_manager);

  if (focus_manager->shortcut_handling_suspended())
    return false;

  ui::Accelerator accelerator(
      static_cast<ui::KeyboardCode>(event.windowsKeyCode),
      content::GetModifiersFromNativeWebKeyboardEvent(event));
  if (event.type == WebKit::WebInputEvent::KeyUp)
    accelerator.set_type(ui::ET_KEY_RELEASED);

  // What we have to do here is as follows:
  // - If the |browser_| is for an app, do nothing.
  // - If the |browser_| is not for an app, and the |accelerator| is not
  //   associated with the browser (e.g. an Ash shortcut), process it.
  // - If the |browser_| is not for an app, and the |accelerator| is associated
  //   with the browser, and it is a reserved one (e.g. Ctrl+w), process it.
  // - If the |browser_| is not for an app, and the |accelerator| is associated
  //   with the browser, and it is not a reserved one, do nothing.

  if (browser_->is_app()) {
    // We don't have to flip |is_keyboard_shortcut| here. If we do that, the app
    // might not be able to see a subsequent Char event. See OnHandleInputEvent
    // in content/renderer/render_widget.cc for details.
    return false;
  }

  chrome::BrowserCommandController* controller = browser_->command_controller();

  // Here we need to retrieve the command id (if any) associated to the
  // keyboard event. Instead of looking up the command id in the
  // |accelerator_table_| by ourselves, we block the command execution of
  // the |browser_| object then send the keyboard event to the
  // |focus_manager| as if we are activating an accelerator key.
  // Then we can retrieve the command id from the |browser_| object.
  controller->SetBlockCommandExecution(true);
  // If the |accelerator| is a non-browser shortcut (e.g. Ash shortcut), the
  // command execution cannot be blocked and true is returned. However, it is
  // okay as long as is_app() is false. See comments in this function.
  const bool processed = focus_manager->ProcessAccelerator(accelerator);
  const int id = controller->GetLastBlockedCommand(NULL);
  controller->SetBlockCommandExecution(false);

  // Executing the command may cause |this| object to be destroyed.
  if (controller->IsReservedCommandOrKey(id, event)) {
    UpdateAcceleratorMetrics(accelerator, id);
    return chrome::ExecuteCommand(browser_.get(), id);
  }

  if (id != -1) {
    // |accelerator| is a non-reserved browser shortcut (e.g. Ctrl+f).
    if (event.type == WebKit::WebInputEvent::RawKeyDown)
      *is_keyboard_shortcut = true;
  } else if (processed) {
    // |accelerator| is a non-browser shortcut (e.g. F4-F10 on Ash).
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
void BrowserView::Cut() {
  // If a WebContent is focused, call RenderWidgetHost::Cut. Otherwise, e.g. if
  // Omnibox is focused, send a Ctrl+x key event to Chrome. Using RWH interface
  // rather than the fake key event for a WebContent is important since the fake
  // event might be consumed by the web content (crbug.com/137908).
  DoCutCopyPaste(&content::RenderWidgetHost::Cut,
#if defined(OS_WIN)
                 WM_CUT,
#endif
                 IDS_APP_CUT);
}

void BrowserView::Copy() {
  DoCutCopyPaste(&content::RenderWidgetHost::Copy,
#if defined(OS_WIN)
                 WM_COPY,
#endif
                 IDS_APP_COPY);
}

void BrowserView::Paste() {
  DoCutCopyPaste(&content::RenderWidgetHost::Paste,
#if defined(OS_WIN)
                 WM_PASTE,
#endif
                 IDS_APP_PASTE);
}

gfx::Rect BrowserView::GetInstantBounds() {
  return contents_->GetPreviewBounds();
}

WindowOpenDisposition BrowserView::GetDispositionForPopupBounds(
    const gfx::Rect& bounds) {
#if defined(OS_WIN)
  // If we are in Win8's single window Metro mode, we can't allow popup windows.
  return win8::IsSingleWindowMetroMode() ? NEW_BACKGROUND_TAB : NEW_POPUP;
#else
  return NEW_POPUP;
#endif
}

FindBar* BrowserView::CreateFindBar() {
  return chrome::CreateFindBar(this);
}

bool BrowserView::GetConstrainedWindowTopY(int* top_y) {
  return GetBrowserViewLayout()->GetConstrainedWindowTopY(top_y);
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, BrowserWindowTesting implementation:

BookmarkBarView* BrowserView::GetBookmarkBarView() const {
  return bookmark_bar_view_.get();
}

LocationBarView* BrowserView::GetLocationBarView() const {
  return toolbar_ ? toolbar_->location_bar() : NULL;
}

views::View* BrowserView::GetTabContentsContainerView() const {
  return contents_container_;
}

ToolbarView* BrowserView::GetToolbarView() const {
  return toolbar_;
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, TabStripModelObserver implementation:

void BrowserView::TabDetachedAt(WebContents* contents, int index) {
  // We use index here rather than comparing |contents| because by this time
  // the model has already removed |contents| from its list, so
  // browser_->GetActiveWebContents() will return NULL or something else.
  if (index == browser_->tab_strip_model()->active_index()) {
    // We need to reset the current tab contents to NULL before it gets
    // freed. This is because the focus manager performs some operations
    // on the selected WebContents when it is removed.
    contents_container_->SetWebContents(NULL);
    infobar_container_->ChangeInfoBarService(NULL);
    UpdateDevToolsForContents(NULL);
  }
}

void BrowserView::TabDeactivated(WebContents* contents) {
  // We do not store the focus when closing the tab to work-around bug 4633.
  // Some reports seem to show that the focus manager and/or focused view can
  // be garbage at that point, it is not clear why.
  if (!contents->IsBeingDestroyed())
    contents->GetView()->StoreFocus();
}

void BrowserView::ActiveTabChanged(content::WebContents* old_contents,
                                   content::WebContents* new_contents,
                                   int index,
                                   bool user_gesture) {
  DCHECK(new_contents);

  // See if the Instant preview is being activated (committed).
  if (contents_->preview_web_contents() == new_contents) {
    contents_->MakePreviewContentsActiveContents();
    views::WebView* old_container = contents_container_;
    contents_container_ = preview_controller_->release_preview();
    old_container->SetWebContents(NULL);
    delete old_container;
  }

  // If |contents_container_| already has the correct WebContents, we can save
  // some work.  This also prevents extra events from being reported by the
  // Visibility API under Windows, as ChangeWebContents will briefly hide
  // the WebContents window.
  bool change_tab_contents =
      contents_container_->web_contents() != new_contents;

  // Update various elements that are interested in knowing the current
  // WebContents.

  // When we toggle the NTP floating bookmarks bar and/or the info bar,
  // we don't want any WebContents to be attached, so that we
  // avoid an unnecessary resize and re-layout of a WebContents.
  if (change_tab_contents)
    contents_container_->SetWebContents(NULL);
  infobar_container_->ChangeInfoBarService(
      InfoBarService::FromWebContents(new_contents));
  if (bookmark_bar_view_.get()) {
    bookmark_bar_view_->SetBookmarkBarState(
        browser_->bookmark_bar_state(),
        BookmarkBar::DONT_ANIMATE_STATE_CHANGE);
  }
  UpdateUIForContents(new_contents);

  // Layout for DevTools _before_ setting the main WebContents to avoid
  // toggling the size of the main WebContents.
  UpdateDevToolsForContents(new_contents);

  if (change_tab_contents) {
    contents_container_->SetWebContents(new_contents);
    contents_->MaybeStackPreviewAtTop();
  }

  if (!browser_->tab_strip_model()->closing_all() && GetWidget()->IsActive() &&
      GetWidget()->IsVisible()) {
    // We only restore focus if our window is visible, to avoid invoking blur
    // handlers when we are eventually shown.
    new_contents->GetView()->RestoreFocus();
  }

  // Update all the UI bits.
  UpdateTitleBar();

  // Like the preview layer and the bookmark bar layer, the immersive mode
  // reveal view's layer may need to live above the web contents.
  MaybeStackImmersiveRevealAtTop();

  // No need to update Toolbar because it's already updated in
  // browser.cc.
}

void BrowserView::TabStripEmpty() {
  // Make sure all optional UI is removed before we are destroyed, otherwise
  // there will be consequences (since our view hierarchy will still have
  // references to freed views).
  UpdateUIForContents(NULL);
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

bool BrowserView::CanActivate() const {
  if (!AppModalDialogQueue::GetInstance()->active_dialog())
    return true;

  // If another browser is app modal, flash and activate the modal browser. This
  // has to be done in a post task, otherwise if the user clicked on a window
  // that doesn't have the modal dialog the windows keep trying to get the focus
  // from each other on Windows. http://crbug.com/141650.
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&BrowserView::ActivateAppModalDialog,
                 activate_modal_dialog_factory_.GetWeakPtr()));
  return false;
}

string16 BrowserView::GetWindowTitle() const {
  return browser_->GetWindowTitleForCurrentTab();
}

string16 BrowserView::GetAccessibleWindowTitle() const {
  if (IsOffTheRecord()) {
    return l10n_util::GetStringFUTF16(
        IDS_ACCESSIBLE_INCOGNITO_WINDOW_TITLE_FORMAT,
        GetWindowTitle());
  }
  return GetWindowTitle();
}

views::View* BrowserView::GetInitiallyFocusedView() {
  // We set the frame not focus on creation so this should never be called.
  NOTREACHED();
  return NULL;
}

bool BrowserView::ShouldShowWindowTitle() const {
#if defined(USE_ASH)
  // For Ash only, app host windows do not show an icon, crbug.com/119411.
  // Child windows (e.g. extension panels, popups) do show an icon.
  if (browser_->is_app() && browser_->app_type() == Browser::APP_TYPE_HOST)
    return false;
#endif
  return browser_->SupportsWindowFeature(Browser::FEATURE_TITLEBAR);
}

gfx::ImageSkia BrowserView::GetWindowAppIcon() {
  if (browser_->is_app()) {
    WebContents* contents = browser_->tab_strip_model()->GetActiveWebContents();
    extensions::TabHelper* extensions_tab_helper =
        contents ? extensions::TabHelper::FromWebContents(contents) : NULL;
    if (extensions_tab_helper && extensions_tab_helper->GetExtensionAppIcon())
      return gfx::ImageSkia::CreateFrom1xBitmap(
          *extensions_tab_helper->GetExtensionAppIcon());
  }

  return GetWindowIcon();
}

gfx::ImageSkia BrowserView::GetWindowIcon() {
  if (browser_->is_app())
    return browser_->GetCurrentPageIcon().AsImageSkia();
  return gfx::ImageSkia();
}

bool BrowserView::ShouldShowWindowIcon() const {
#if defined(USE_ASH)
  // For Ash only, app host windows do not show an icon, crbug.com/119411.
  // Child windows (e.g. extension panels, popups) do show an icon.
  if (browser_->is_app() && browser_->app_type() == Browser::APP_TYPE_HOST)
    return false;
#endif
  return browser_->SupportsWindowFeature(Browser::FEATURE_TITLEBAR);
}

bool BrowserView::ExecuteWindowsCommand(int command_id) {
  // This function handles WM_SYSCOMMAND, WM_APPCOMMAND, and WM_COMMAND.
#if defined(OS_WIN)
  if (command_id == IDC_DEBUG_FRAME_TOGGLE)
    GetWidget()->DebugToggleFrameType();

  // In Windows 8 metro mode prevent sizing and moving.
  if (win8::IsSingleWindowMetroMode()) {
    // Windows uses the 4 lower order bits of |notification_code| for type-
    // specific information so we must exclude this when comparing.
    static const int sc_mask = 0xFFF0;
    if (((command_id & sc_mask) == SC_MOVE) ||
        ((command_id & sc_mask) == SC_SIZE) ||
        ((command_id & sc_mask) == SC_MAXIMIZE))
      return true;
  }
#endif
  // Translate WM_APPCOMMAND command ids into a command id that the browser
  // knows how to handle.
  int command_id_from_app_command = GetCommandIDForAppCommandID(command_id);
  if (command_id_from_app_command != -1)
    command_id = command_id_from_app_command;

  return chrome::ExecuteCommand(browser_.get(), command_id);
}

std::string BrowserView::GetWindowName() const {
  return chrome::GetWindowPlacementKey(browser_.get());
}

void BrowserView::SaveWindowPlacement(const gfx::Rect& bounds,
                                      ui::WindowShowState show_state) {
  // If IsFullscreen() is true, we've just changed into fullscreen mode, and
  // we're catching the going-into-fullscreen sizing and positioning calls,
  // which we want to ignore.
  if (!ShouldSaveOrRestoreWindowPos())
    return;

  if (!IsFullscreen() && chrome::ShouldSaveWindowPlacement(browser_.get())) {
    WidgetDelegate::SaveWindowPlacement(bounds, show_state);
    chrome::SaveWindowPlacement(browser_.get(), bounds, show_state);
  }
}

bool BrowserView::GetSavedWindowPlacement(
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {
  if (!ShouldSaveOrRestoreWindowPos())
    return false;
  chrome::GetSavedWindowBoundsAndShowState(browser_.get(), bounds, show_state);

#if defined(USE_ASH)
  if (chrome::IsNativeWindowInAsh(
          const_cast<BrowserView*>(this)->GetNativeWindow())) {
  if (browser_->is_type_popup() || browser_->is_type_panel()) {
      // In case of a popup or panel with an 'unspecified' location we are
      // looking for a good screen location. We are interpreting (0,0) as an
      // unspecified location.
    if (bounds->x() == 0 && bounds->y() == 0) {
      *bounds = ChromeShellDelegate::instance()->window_positioner()->
          GetPopupPosition(*bounds);
    }
  }
  }
#endif

  if ((browser_->is_type_popup() || browser_->is_type_panel()) &&
      bounds->width() == browser_->override_bounds().width() &&
      bounds->height() == browser_->override_bounds().height() &&
      !browser_->is_devtools()) {
    // This is a popup window that has not been resized. The value passed in
    // |bounds| represents two pieces of information:
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
      window_rect.set_origin(
          WindowSizer::GetDefaultPopupOrigin(size,
                                             browser_->host_desktop_type()));
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
  return contents_container_;
}

views::ClientView* BrowserView::CreateClientView(views::Widget* widget) {
  return this;
}

void BrowserView::OnWidgetActivationChanged(views::Widget* widget,
                                            bool active) {
#if defined(USE_ASH)
  if (launcher_item_controller_.get())
    launcher_item_controller_->BrowserActivationStateChanged();
#endif

  if (active) {
    BrowserList::SetLastActive(browser_.get());
    browser_->OnWindowActivated();
  }
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

  // status_bubble_ may be NULL if this is invoked during construction.
  if (status_bubble_.get())
    status_bubble_->Reposition();

  chrome::HideBookmarkBubbleView();

  // Close the omnibox popup, if any.
  LocationBarView* location_bar_view = GetLocationBarView();
  if (location_bar_view)
    location_bar_view->GetLocationEntry()->CloseOmniboxPopup();
}

views::Widget* BrowserView::GetWidget() {
  return View::GetWidget();
}

const views::Widget* BrowserView::GetWidget() const {
  return View::GetWidget();
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

  if (!browser_->tab_strip_model()->empty()) {
    // Tab strip isn't empty.  Hide the frame (so it appears to have closed
    // immediately) and close all the tabs, allowing the renderers to shut
    // down. When the tab strip is empty we'll be called back again.
    frame_->Hide();
    browser_->OnWindowClosing();
    return false;
  }

  // Empty TabStripModel, it's now safe to allow the Window to be closed.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_WINDOW_CLOSED,
      content::Source<gfx::NativeWindow>(frame_->GetNativeWindow()),
      content::NotificationService::NoDetails());
  return true;
}

int BrowserView::NonClientHitTest(const gfx::Point& point) {
#if defined(OS_WIN) && !defined(USE_AURA)
  // The following code is not in the LayoutManager because it's
  // independent of layout and also depends on the ResizeCorner which
  // is private.
  if (!frame_->IsMaximized() && !frame_->IsFullscreen()) {
    CRect client_rect;
    ::GetClientRect(frame_->GetNativeWindow(), &client_rect);
    gfx::Size resize_corner_size = ResizeCorner::GetSize();
    gfx::Rect resize_corner_rect(client_rect.right - resize_corner_size.width(),
        client_rect.bottom - resize_corner_size.height(),
        resize_corner_size.width(), resize_corner_size.height());
    bool rtl_dir = base::i18n::IsRTL();
    if (rtl_dir)
      resize_corner_rect.set_x(0);
    if (resize_corner_rect.Contains(point)) {
      if (rtl_dir)
        return HTBOTTOMLEFT;
      return HTBOTTOMRIGHT;
    }
  }
#endif

  return GetBrowserViewLayout()->NonClientHitTest(point);
}

gfx::Size BrowserView::GetMinimumSize() {
  return GetBrowserViewLayout()->GetMinimumSize();
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, views::View overrides:

std::string BrowserView::GetClassName() const {
  return kViewClassName;
}

void BrowserView::Layout() {
  if (ignore_layout_)
    return;

  views::View::Layout();

  // The status bubble position requires that all other layout finish first.
  LayoutStatusBubble();
}

void BrowserView::PaintChildren(gfx::Canvas* canvas) {
  // Paint the |infobar_container_| last so that it may paint its
  // overlapping tabs.
  for (int i = 0; i < child_count(); ++i) {
    View* child = child_at(i);
    if (child != infobar_container_)
      child->Paint(canvas);
  }

  infobar_container_->Paint(canvas);
}

void BrowserView::ViewHierarchyChanged(bool is_add,
                                       views::View* parent,
                                       views::View* child) {
  if (!initialized_ && is_add && child == this && GetWidget()) {
    Init();
    initialized_ = true;
  }
}

void BrowserView::ChildPreferredSizeChanged(View* child) {
  Layout();
}

void BrowserView::GetAccessibleState(ui::AccessibleViewState* state) {
  state->name = l10n_util::GetStringUTF16(IDS_PRODUCT_NAME);
  state->role = ui::AccessibilityTypes::ROLE_CLIENT;
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, protected

void BrowserView::GetAccessiblePanes(
    std::vector<views::AccessiblePaneView*>* panes) {
  // This should be in the order of pane traversal of the panes using F6.
  // If one of these is invisible or has no focusable children, it will be
  // automatically skipped.
  panes->push_back(toolbar_);
  if (bookmark_bar_view_.get())
    panes->push_back(bookmark_bar_view_.get());
  if (infobar_container_)
    panes->push_back(infobar_container_);
  if (download_shelf_.get())
    panes->push_back(download_shelf_.get());
}

SkColor BrowserView::GetInfoBarSeparatorColor() const {
  // NOTE: Keep this in sync with ToolbarView::OnPaint()!
  return (IsTabStripVisible() || !frame_->ShouldUseNativeFrame()) ?
      ThemeProperties::GetDefaultColor(
          ThemeProperties::COLOR_TOOLBAR_SEPARATOR) :
      SK_ColorBLACK;
}

void BrowserView::InfoBarContainerStateChanged(bool is_animating) {
  ToolbarSizeChanged(is_animating);
}

bool BrowserView::DrawInfoBarArrows(int* x) const {
  if (x) {
    const LocationIconView* location_icon_view =
        toolbar_->location_bar()->location_icon_view();
    gfx::Point icon_center(location_icon_view->GetImageBounds().CenterPoint());
    ConvertPointToTarget(location_icon_view, this, &icon_center);
    *x = icon_center.x();
  }
  return true;
}

bool BrowserView::SplitHandleMoved(views::SingleSplitView* sender) {
  for (int i = 0; i < sender->child_count(); ++i)
    sender->child_at(i)->InvalidateLayout();
  SchedulePaint();
  Layout();
  return false;
}

void BrowserView::OnSysColorChange() {
  chrome::MaybeShowInvertBubbleView(browser_.get(), contents_);
}

int BrowserView::GetOTRIconResourceID() const {
  int otr_resource_id = IDR_OTR_ICON;
  if (ui::GetDisplayLayout() == ui::LAYOUT_TOUCH) {
    if (IsFullscreen())
      otr_resource_id = IDR_OTR_ICON_FULLSCREEN;
#if defined(OS_WIN)
    if (win8::IsSingleWindowMetroMode())
      otr_resource_id = IDR_OTR_ICON_FULLSCREEN;
#endif
  }

  return otr_resource_id;
}

views::LayoutManager* BrowserView::CreateLayoutManager() const {
  return new BrowserViewLayout;
}

void BrowserView::Init() {
  GetWidget()->AddObserver(this);

  SetLayoutManager(CreateLayoutManager());
  // Stow a pointer to this object onto the window handle so that we can get at
  // it later when all we have is a native view.
  GetWidget()->SetNativeWindowProperty(kBrowserViewKey, this);

  // Stow a pointer to the browser's profile onto the window handle so that we
  // can get it later when all we have is a native view.
  GetWidget()->SetNativeWindowProperty(Profile::kProfileKey,
                                       browser_->profile());

  // Initialize after |this| has a Widget and native window.
  immersive_mode_controller_->Init();

  // Start a hung plugin window detector for this browser object (as long as
  // hang detection is not disabled).
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableHangMonitor)) {
    InitHangMonitor();
  }

  LoadAccelerators();

  // TabStrip takes ownership of the controller.
  BrowserTabStripController* tabstrip_controller =
      new BrowserTabStripController(browser_.get(),
                                    browser_->tab_strip_model());
  tabstrip_ = new TabStrip(tabstrip_controller);
  AddChildViewAt(tabstrip_, kTabstripIndex);
  tabstrip_controller->InitFromModel(tabstrip_);

  infobar_container_ = new InfoBarContainerView(this,
                                                browser()->search_model());
  AddChildViewAt(infobar_container_, kInfoBarIndex);

  contents_container_ = new views::WebView(browser_->profile());
  contents_container_->set_id(VIEW_ID_TAB_CONTAINER);
  contents_ = new ContentsContainer(contents_container_);

  toolbar_ = new ToolbarView(browser_.get());
  AddChildViewAt(toolbar_, kToolbarIndex);
  toolbar_->Init();

  preview_controller_.reset(
      new InstantPreviewControllerViews(browser(), contents_));

  SkColor bg_color = GetWidget()->GetThemeProvider()->
      GetColor(ThemeProperties::COLOR_TOOLBAR);

  devtools_container_ = new views::WebView(browser_->profile());
  devtools_container_->set_id(VIEW_ID_DEV_TOOLS_DOCKED);
  devtools_container_->SetVisible(false);

  views::View* contents_view = contents_;

  contents_split_ = new views::SingleSplitView(
      contents_view,
      devtools_container_,
      views::SingleSplitView::VERTICAL_SPLIT,
      this);
  contents_split_->set_id(VIEW_ID_CONTENTS_SPLIT);
  contents_split_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_WEB_CONTENTS));
  contents_split_->set_background(
      views::Background::CreateSolidBackground(bg_color));
  AddChildView(contents_split_);
  set_contents_view(contents_split_);

  status_bubble_.reset(new StatusBubbleViews(contents_));

#if defined(OS_WIN) && !defined(USE_AURA)
  // Create a custom JumpList and add it to an observer of TabRestoreService
  // so we can update the custom JumpList when a tab is added or removed.
  if (JumpList::Enabled()) {
    load_complete_listener_.reset(new LoadCompleteListener(this));
  }
#endif

  // We're now initialized and ready to process Layout requests.
  ignore_layout_ = false;
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
    // GetActiveWebContents can return NULL for example under Purify when
    // the animations are running slowly and this function is called on a timer
    // through LoadingAnimationCallback.
    frame_->UpdateThrobber(web_contents && web_contents->IsLoading());
  }
}

void BrowserView::OnLoadCompleted() {
#if defined(OS_WIN) && !defined(USE_AURA)
  DCHECK(!jumplist_);
  jumplist_ = new JumpList();
  jumplist_->AddObserver(browser_->profile());
#endif
}

// BrowserView, private --------------------------------------------------------

BrowserViewLayout* BrowserView::GetBrowserViewLayout() const {
  return static_cast<BrowserViewLayout*>(GetLayoutManager());
}

void BrowserView::LayoutStatusBubble() {
  // In restored mode, the client area has a client edge between it and the
  // frame.
  int overlap = StatusBubbleViews::kShadowThickness;
  // The extra pixels defined by kClientEdgeThickness is only drawn in frame
  // content border on windows for non-aura build.
#if !defined(USE_ASH)
  overlap +=
      IsMaximized() ? 0 : views::NonClientFrameView::kClientEdgeThickness;
#endif
  int height = status_bubble_->GetPreferredSize().height();
  int contents_height = status_bubble_->base_view()->bounds().height();
  gfx::Point origin(-overlap, contents_height - height + overlap);
  status_bubble_->SetBounds(origin.x(), origin.y(), width() / 3, height);
}

bool BrowserView::MaybeShowBookmarkBar(WebContents* contents) {
  views::View* new_bookmark_bar_view = NULL;
  if (browser_->SupportsWindowFeature(Browser::FEATURE_BOOKMARKBAR) &&
      contents) {
    if (!bookmark_bar_view_.get()) {
      bookmark_bar_view_.reset(new BookmarkBarView(browser_.get(), this));
      bookmark_bar_view_->set_owned_by_client();
      bookmark_bar_view_->set_background(
          new BookmarkExtensionBackground(this, bookmark_bar_view_.get(),
                                          browser_.get()));
      bookmark_bar_view_->SetBookmarkBarState(
          browser_->bookmark_bar_state(),
          BookmarkBar::DONT_ANIMATE_STATE_CHANGE);
    }
    bookmark_bar_view_->SetPageNavigator(contents);
    new_bookmark_bar_view = bookmark_bar_view_.get();
  }
  return UpdateChildViewAndLayout(new_bookmark_bar_view, &active_bookmark_bar_);
}

bool BrowserView::MaybeShowInfoBar(WebContents* contents) {
  // TODO(beng): Remove this function once the interface between
  //             InfoBarContainer, DownloadShelfView and WebContents and this
  //             view is sorted out.
  return true;
}

void BrowserView::UpdateDevToolsForContents(WebContents* web_contents) {
  DevToolsWindow* new_devtools_window = web_contents ?
      DevToolsWindow::GetDockedInstanceForInspectedTab(web_contents) : NULL;
  // Fast return in case of the same window having same orientation.
  if (devtools_window_ == new_devtools_window) {
    if (!new_devtools_window ||
        (new_devtools_window->dock_side() == devtools_dock_side_)) {
      if (new_devtools_window)
        UpdateDevToolsSplitPosition();
      return;
    }
  }

  // Replace tab contents.
  if (devtools_window_ != new_devtools_window) {
    devtools_container_->SetWebContents(
        new_devtools_window ? new_devtools_window->web_contents() : NULL);
  }

  // Store last used position.
  if (devtools_window_) {
    if (devtools_dock_side_ == DEVTOOLS_DOCK_SIDE_RIGHT) {
      devtools_window_->SetWidth(
          contents_split_->width() - contents_split_->divider_offset());
    } else {
      devtools_window_->SetHeight(
          contents_split_->height() - contents_split_->divider_offset());
    }
  }

  // Show / hide container if necessary. Changing dock orientation is
  // hide + show.
  bool should_hide = devtools_window_ && (!new_devtools_window ||
      devtools_dock_side_ != new_devtools_window->dock_side());
  bool should_show = new_devtools_window && (!devtools_window_ || should_hide);

  if (should_hide)
    HideDevToolsContainer();

  devtools_window_ = new_devtools_window;

  if (should_show) {
    devtools_dock_side_ = new_devtools_window->dock_side();
    ShowDevToolsContainer();
  } else if (new_devtools_window) {
    UpdateDevToolsSplitPosition();
    contents_split_->Layout();
  }
}

void BrowserView::ShowDevToolsContainer() {
  if (!devtools_focus_tracker_.get()) {
    // Install devtools focus tracker when dev tools window is shown for the
    // first time.
    devtools_focus_tracker_.reset(
        new views::ExternalFocusTracker(devtools_container_,
                                        GetFocusManager()));
  }
  devtools_container_->SetVisible(true);
  devtools_dock_side_ = devtools_window_->dock_side();
  bool dock_to_right = devtools_dock_side_ == DEVTOOLS_DOCK_SIDE_RIGHT;
  contents_split_->set_orientation(
      dock_to_right ? views::SingleSplitView::HORIZONTAL_SPLIT
                    : views::SingleSplitView::VERTICAL_SPLIT);
  UpdateDevToolsSplitPosition();
  contents_split_->InvalidateLayout();
  Layout();
}

void BrowserView::HideDevToolsContainer() {
  // Restore focus to the last focused view when hiding devtools window.
  devtools_focus_tracker_->FocusLastFocusedExternalView();
  devtools_container_->SetVisible(false);
  contents_split_->InvalidateLayout();
  Layout();
}

void BrowserView::UpdateDevToolsSplitPosition() {
  int split_size = contents_split_->GetDividerSize();
  if (devtools_window_->dock_side() == DEVTOOLS_DOCK_SIDE_RIGHT) {
    int split_offset = contents_split_->width() - (split_size / 2) -
        devtools_window_->GetWidth(contents_split_->width());
    contents_split_->set_divider_offset(split_offset);
  } else {
    int split_offset = contents_split_->height() - (split_size / 2) -
        devtools_window_->GetHeight(contents_split_->height());
    contents_split_->set_divider_offset(split_offset);
  }
}

void BrowserView::UpdateUIForContents(WebContents* contents) {
  bool needs_layout = MaybeShowBookmarkBar(contents);
  needs_layout |= MaybeShowInfoBar(contents);
  if (needs_layout)
    Layout();
}

bool BrowserView::UpdateChildViewAndLayout(views::View* new_view,
                                           views::View** old_view) {
  DCHECK(old_view);
  if (*old_view == new_view) {
    // The views haven't changed, if the views pref changed schedule a layout.
    if (new_view) {
      if (new_view->GetPreferredSize().height() != new_view->height())
        return true;
    }
    return false;
  }

  // The views differ, and one may be null (but not both). Remove the old
  // view (if it non-null), and add the new one (if it is non-null). If the
  // height has changed, schedule a layout, otherwise reuse the existing
  // bounds to avoid scheduling a layout.

  int current_height = 0;
  if (*old_view) {
    current_height = (*old_view)->height();
    RemoveChildView(*old_view);
  }

  int new_height = 0;
  if (new_view) {
    new_height = new_view->GetPreferredSize().height();
    AddChildView(new_view);
  }
  bool changed = false;
  if (new_height != current_height) {
    changed = true;
  } else if (new_view && *old_view) {
    // The view changed, but the new view wants the same size, give it the
    // bounds of the last view and have it repaint.
    new_view->SetBoundsRect((*old_view)->bounds());
    new_view->SchedulePaint();
  } else if (new_view) {
    DCHECK_EQ(0, new_height);
    // The heights are the same, but the old view is null. This only happens
    // when the height is zero. Zero out the bounds.
    new_view->SetBounds(0, 0, 0, 0);
  }
  *old_view = new_view;
  return changed;
}

void BrowserView::ProcessFullscreen(bool fullscreen,
                                    FullscreenType type,
                                    const GURL& url,
                                    FullscreenExitBubbleType bubble_type) {
  // Reduce jankiness during the following position changes by:
  //   * Hiding the window until it's in the final position
  //   * Ignoring all intervening Layout() calls, which resize the webpage and
  //     thus are slow and look ugly
  ignore_layout_ = true;
  LocationBarView* location_bar = GetLocationBarView();
#if defined(OS_WIN) && !defined(USE_AURA)
  OmniboxViewWin* omnibox_win =
      GetOmniboxViewWin(location_bar->GetLocationEntry());
#endif

  if (type == FOR_METRO || !fullscreen) {
    // Hide the fullscreen bubble as soon as possible, since the mode toggle can
    // take enough time for the user to notice.
    fullscreen_bubble_.reset();
  }

  if (fullscreen) {
    // Move focus out of the location bar if necessary.
    views::FocusManager* focus_manager = GetFocusManager();
    DCHECK(focus_manager);
    // Look for focus in the location bar itself or any child view.
    if (location_bar->Contains(focus_manager->GetFocusedView()))
      focus_manager->ClearFocus();

#if defined(OS_WIN) && !defined(USE_AURA)
    if (omnibox_win) {
      // If we don't hide the edit and force it to not show until we come out of
      // fullscreen, then if the user was on the New Tab Page, the edit contents
      // will appear atop the web contents once we go into fullscreen mode. This
      // has something to do with how we move the main window while it's hidden;
      // if we don't hide the main window below, we don't get this problem.
      omnibox_win->set_force_hidden(true);
      ShowWindow(omnibox_win->m_hWnd, SW_HIDE);
    }
#endif
  }
#if defined(OS_WIN) && !defined(USE_AURA)
  views::ScopedFullscreenVisibility visibility(frame_->GetNativeView());
#endif

  if (type == FOR_METRO) {
#if defined(OS_WIN) && !defined(USE_AURA)
    // Enter metro snap mode.
    static_cast<views::NativeWidgetWin*>(
        frame_->native_widget())->SetMetroSnapFullscreen(fullscreen);
#endif
  } else {
    // Toggle fullscreen mode.
    frame_->SetFullscreen(fullscreen);
  }

  browser_->WindowFullscreenStateChanged();

  if (fullscreen) {
    if (!chrome::IsRunningInAppMode() &&
        type != FOR_METRO &&
        !UseImmersiveFullscreen(url)) {
      fullscreen_bubble_.reset(new FullscreenExitBubbleViews(
          GetWidget(), browser_.get(), url, bubble_type));
    }
  } else {
#if defined(OS_WIN) && !defined(USE_AURA)
    if (omnibox_win) {
      // Show the edit again since we're no longer in fullscreen mode.
      omnibox_win->set_force_hidden(false);
      ShowWindow(omnibox_win->m_hWnd, SW_SHOW);
    }
#endif
  }

  if (UseImmersiveFullscreen(url))
    immersive_mode_controller_->SetEnabled(fullscreen);

  // Undo our anti-jankiness hacks and force the window to re-layout now that
  // it's in its final position.
  ignore_layout_ = false;
  Layout();
}

void BrowserView::LoadAccelerators() {
#if defined(OS_WIN) && !defined(USE_AURA)
  HACCEL accelerator_table = AtlLoadAccelerators(IDR_MAINFRAME);
  DCHECK(accelerator_table);

  // We have to copy the table to access its contents.
  int count = CopyAcceleratorTable(accelerator_table, 0, 0);
  if (count == 0) {
    // Nothing to do in that case.
    return;
  }

  ACCEL* accelerators = static_cast<ACCEL*>(malloc(sizeof(ACCEL) * count));
  CopyAcceleratorTable(accelerator_table, accelerators, count);

  views::FocusManager* focus_manager = GetFocusManager();
  DCHECK(focus_manager);

  // Let's fill our own accelerator table.
  for (int i = 0; i < count; ++i) {
    ui::Accelerator accelerator(
        static_cast<ui::KeyboardCode>(accelerators[i].key),
        ui::GetModifiersFromACCEL(accelerators[i]));
    accelerator_table_[accelerator] = accelerators[i].cmd;

    // Also register with the focus manager.
    focus_manager->RegisterAccelerator(
        accelerator, ui::AcceleratorManager::kNormalPriority, this);
  }

  // We don't need the Windows accelerator table anymore.
  free(accelerators);
#else
  views::FocusManager* focus_manager = GetFocusManager();
  DCHECK(focus_manager);

  // Let's fill our own accelerator table.
  const std::vector<chrome::AcceleratorMapping> accelerator_list(
      chrome::GetAcceleratorList());
  for (std::vector<chrome::AcceleratorMapping>::const_iterator it =
           accelerator_list.begin(); it != accelerator_list.end(); ++it) {
    ui::Accelerator accelerator(it->keycode, it->modifiers);
    accelerator_table_[accelerator] = it->command_id;

    // Also register with the focus manager.
    focus_manager->RegisterAccelerator(
        accelerator, ui::AcceleratorManager::kNormalPriority, this);
  }
#endif
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
#if defined(OS_WIN) && !defined(USE_AURA)
  PrefService* pref_service = g_browser_process->local_state();
  if (!pref_service)
    return;

  int plugin_message_response_timeout =
      pref_service->GetInteger(prefs::kPluginMessageResponseTimeout);
  int hung_plugin_detect_freq =
      pref_service->GetInteger(prefs::kHungPluginDetectFrequency);
  if ((hung_plugin_detect_freq > 0) &&
      hung_window_detector_.Initialize(GetWidget()->GetNativeView(),
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

void BrowserView::UpdateAcceleratorMetrics(
    const ui::Accelerator& accelerator, int command_id) {
  const ui::KeyboardCode key_code = accelerator.key_code();
  if (command_id == IDC_HELP_PAGE_VIA_KEYBOARD && key_code == ui::VKEY_F1)
    content::RecordAction(UserMetricsAction("ShowHelpTabViaF1"));

  if (command_id == IDC_BOOKMARK_PAGE)
    UMA_HISTOGRAM_ENUMERATION("Bookmarks.EntryPoint",
                              bookmark_utils::ENTRY_POINT_ACCELERATOR,
                              bookmark_utils::ENTRY_POINT_LIMIT);

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
    case IDC_FULLSCREEN:
      if (key_code == ui::VKEY_MEDIA_LAUNCH_APP2)
        content::RecordAction(UserMetricsAction("Accel_Fullscreen_F4"));
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

gfx::Size BrowserView::GetResizeCornerSize() const {
  return ResizeCorner::GetSize();
}

void BrowserView::CreateLauncherIcon() {
#if defined(USE_ASH)
  if (chrome::IsNativeWindowInAsh(GetNativeWindow()) &&
      !launcher_item_controller_.get()) {
    launcher_item_controller_.reset(
        BrowserLauncherItemController::Create(browser_.get()));
  }
#endif  // defined(USE_ASH)
}

// static
BrowserWindow* BrowserWindow::CreateBrowserWindow(Browser* browser) {
  // Create the view and the frame. The frame will attach itself via the view
  // so we don't need to do anything with the pointer.
  BrowserView* view = new BrowserView(browser);
  (new BrowserFrame(view))->InitBrowserFrame();
  view->GetWidget()->non_client_view()->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
  return view;
}

void BrowserView::ShowAvatarBubble(WebContents* web_contents,
                                   const gfx::Rect& rect) {
  gfx::Point origin(rect.origin());
  views::View::ConvertPointToScreen(GetTabContentsContainerView(), &origin);
  gfx::Rect bounds(origin, rect.size());

  AvatarMenuBubbleView::ShowBubble(this, views::BubbleBorder::TOP_RIGHT,
      views::BubbleBorder::ALIGN_EDGE_TO_ANCHOR_EDGE, bounds, browser_.get());
}

void BrowserView::ShowAvatarBubbleFromAvatarButton() {
  AvatarMenuButton* button = frame_->GetAvatarMenuButton();
  if (button)
    button->ShowAvatarBubble();
}

void BrowserView::ShowPasswordGenerationBubble(
    const gfx::Rect& rect,
    const content::PasswordForm& form,
    autofill::PasswordGenerator* password_generator) {
  // Create a rect in the content bounds that the bubble will point to.
  gfx::Point origin(rect.origin());
  views::View::ConvertPointToScreen(GetTabContentsContainerView(), &origin);
  gfx::Rect bounds(origin, rect.size());

  // Create the bubble.
  WebContents* web_contents = GetActiveWebContents();
  if (!web_contents)
    return;

  PasswordGenerationBubbleView* bubble =
      new PasswordGenerationBubbleView(
          form,
          bounds,
          this,
          web_contents->GetRenderViewHost(),
          PasswordManager::FromWebContents(web_contents),
          password_generator,
          browser_.get(),
          GetWidget()->GetThemeProvider());

  views::BubbleDelegateView::CreateBubble(bubble);
  bubble->SetAlignment(views::BubbleBorder::ALIGN_ARROW_TO_MID_ANCHOR);
  bubble->Show();
}

void BrowserView::DoCutCopyPaste(void (content::RenderWidgetHost::*method)(),
#if defined(OS_WIN)
                                 int windows_msg_id,
#endif
                                 int command_id) {
  WebContents* contents = browser_->tab_strip_model()->GetActiveWebContents();
  if (!contents)
    return;
  if (DoCutCopyPasteForWebContents(contents, method))
    return;

  DevToolsWindow* devtools_window =
      DevToolsWindow::GetDockedInstanceForInspectedTab(contents);
  if (devtools_window &&
      DoCutCopyPasteForWebContents(devtools_window->web_contents(), method)) {
    return;
  }

  views::FocusManager* focus_manager = GetFocusManager();
  views::View* focused = focus_manager->GetFocusedView();
  if (focused->GetClassName() == views::Textfield::kViewClassName) {
    views::Textfield* textfield = static_cast<views::Textfield*>(focused);
    textfield->ExecuteCommand(command_id);
    return;
  }

#if defined(OS_WIN) && !defined(USE_AURA)
  OmniboxView* omnibox_view = GetLocationBarView()->GetLocationEntry();
  if (omnibox_view->model()->has_focus()) {
    OmniboxViewWin* omnibox_win = GetOmniboxViewWin(omnibox_view);
    ::SendMessage(omnibox_win->GetNativeView(), windows_msg_id, 0, 0);
  }
#endif
}

bool BrowserView::DoCutCopyPasteForWebContents(
    WebContents* contents,
    void (content::RenderWidgetHost::*method)()) {
  gfx::NativeView native_view = contents->GetView()->GetContentNativeView();
  if (!native_view)
    return false;
#if defined(USE_AURA)
  if (native_view->HasFocus()) {
#elif defined(OS_WIN)
  if (native_view == ::GetFocus()) {
#endif
    (contents->GetRenderViewHost()->*method)();
    return true;
  }

  return false;
}

void BrowserView::ActivateAppModalDialog() const {
  // If another browser is app modal, flash and activate the modal browser.
  AppModalDialog* active_dialog =
      AppModalDialogQueue::GetInstance()->active_dialog();
  if (!active_dialog)
    return;

  Browser* modal_browser =
      chrome::FindBrowserWithWebContents(active_dialog->web_contents());
  if (modal_browser && (browser_ != modal_browser)) {
    modal_browser->window()->FlashFrame(true);
    modal_browser->window()->Activate();
  }

  AppModalDialogQueue::GetInstance()->ActivateModalDialog();
}
