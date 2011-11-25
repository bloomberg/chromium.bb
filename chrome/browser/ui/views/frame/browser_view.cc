// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_view.h"

#if defined(TOOLKIT_USES_GTK)
#include <gtk/gtk.h>
#endif

#include <algorithm>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/i18n/rtl.h"
#include "base/metrics/histogram.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/autocomplete/autocomplete_popup_model.h"
#include "chrome/browser/autocomplete/autocomplete_popup_view.h"
#include "chrome/browser/automation/ui_controls.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/debugger/devtools_window.h"
#include "chrome/browser/extensions/extension_tab_helper.h"
#include "chrome/browser/extensions/extension_tts_api.h"
#include "chrome/browser/instant/instant_controller.h"
#include "chrome/browser/native_window_notification_source.h"
#include "chrome/browser/ntp_background_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/avatar_menu_model.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/sidebar/sidebar_container.h"
#include "chrome/browser/sidebar/sidebar_manager.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/app_modal_dialogs/app_modal_dialog_queue.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/tabs/tab_menu_model.h"
#include "chrome/browser/ui/toolbar/wrench_menu_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/avatar_menu_bubble_view.h"
#include "chrome/browser/ui/views/avatar_menu_button.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/browser_dialogs.h"
#include "chrome/browser/ui/views/default_search_view.h"
#include "chrome/browser/ui/views/download/download_in_progress_dialog_view.h"
#include "chrome/browser/ui/views/frame/browser_view_layout.h"
#include "chrome/browser/ui/views/frame/contents_container.h"
#include "chrome/browser/ui/views/fullscreen_exit_bubble_views.h"
#include "chrome/browser/ui/views/infobars/infobar_container_view.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/views/status_bubble_views.h"
#include "chrome/browser/ui/views/tab_contents/tab_contents_container.h"
#include "chrome/browser/ui/views/tabs/browser_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip_factory.h"
#include "chrome/browser/ui/views/toolbar_view.h"
#include "chrome/browser/ui/views/update_recommended_message_box.h"
#include "chrome/browser/ui/views/window.h"
#include "chrome/browser/ui/webui/bug_report_ui.h"
#include "chrome/browser/ui/webui/task_manager_dialog.h"
#include "chrome/browser/ui/window_sizer.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_resource.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/browser/download/download_manager.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_view.h"
#include "content/browser/user_metrics.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/content_switches.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "grit/theme_resources_standard.h"
#include "grit/ui_resources.h"
#include "grit/webkit_resources.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/views/events/event.h"
#include "ui/views/focus/external_focus_tracker.h"
#include "ui/views/focus/view_storage.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/native_widget.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"
#include "views/controls/single_split_view.h"

#if defined(USE_AURA)
#include "chrome/browser/ui/views/accelerator_table.h"
#include "chrome/browser/ui/views/aura/chrome_shell_delegate.h"
#include "chrome/browser/ui/views/aura/launcher_icon_updater.h"
#include "ui/aura_shell/launcher/launcher.h"
#include "ui/aura_shell/launcher/launcher_model.h"
#include "ui/aura_shell/shell.h"
#include "ui/gfx/screen.h"
#elif defined(OS_WIN)
#include "chrome/browser/aeropeek_manager.h"
#include "chrome/browser/jumplist_win.h"
#include "ui/base/message_box_win.h"
#include "ui/views/widget/native_widget_win.h"
#elif defined(TOOLKIT_USES_GTK)
#include "chrome/browser/ui/views/accelerator_table.h"
#if !defined(TOUCH_UI)
#include "chrome/browser/ui/views/handle_web_keyboard_event.h"
#endif
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/views/keyboard_overlay_dialog_view.h"
#include "chrome/browser/ui/webui/chromeos/mobile_setup_dialog.h"
#endif

#if !defined(OS_CHROMEOS) || defined(USE_AURA)
#include "chrome/browser/ui/views/download/download_shelf_view.h"
#endif

#if defined(TOUCH_UI)
#include "chrome/browser/ui/touch/status_bubble_touch.h"
#endif

using base::TimeDelta;
using views::ColumnSet;
using views::GridLayout;

namespace {
// The height of the status bubble.
const int kStatusBubbleHeight = 20;
// The name of a key to store on the window handle so that other code can
// locate this object using just the handle.
const char* const kBrowserViewKey = "__BROWSER_VIEW__";
// How frequently we check for hung plugin windows.
const int kDefaultHungPluginDetectFrequency = 2000;

// Minimal height of devtools pane or content pane when devtools are docked
// to the browser window.
const int kMinDevToolsHeight = 50;
const int kMinContentsHeight = 50;

#if defined(USE_AURA) && defined(OS_CHROMEOS)
// If a popup window is larger than this fraction of the screen, create a tab.
const float kPopupMaxWidthFactor = 0.5;
const float kPopupMaxHeightFactor = 0.6;
#endif

// How long do we wait before we consider a window hung (in ms).
const int kDefaultPluginMessageResponseTimeout = 30000;
// The number of milliseconds between loading animation frames.
const int kLoadingAnimationFrameTimeMs = 30;
// The amount of space we expect the window border to take up.
const int kWindowBorderWidth = 5;

// How round the 'new tab' style bookmarks bar is.
const int kNewtabBarRoundness = 5;
// ------------

}  // namespace

// Returned from BrowserView::GetClassName.
const char BrowserView::kViewClassName[] = "browser/ui/views/BrowserView";

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
  virtual void Paint(gfx::Canvas* canvas, views::View* view) const;

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
    TabContents* contents = browser_->GetSelectedTabContents();
    if (contents && contents->view())
      height = contents->view()->GetContainerSize().height();
    NtpBackgroundUtil::PaintBackgroundDetachedMode(
        host_view_->GetThemeProvider(), canvas,
        gfx::Rect(0, toolbar_overlap, host_view_->width(),
                  host_view_->height() - toolbar_overlap), height);

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
        gfx::Point(host_view_->GetMirroredX(), host_view_->y())));
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

  virtual void OnPaint(gfx::Canvas* canvas) {
    views::Widget* widget = GetWidget();
    if (!widget || (widget->IsMaximized() || widget->IsFullscreen()))
      return;

    SkBitmap* bitmap = ResourceBundle::GetSharedInstance().GetBitmapNamed(
        IDR_TEXTAREA_RESIZER);
    bitmap->buildMipMap(false);
    canvas->DrawBitmapInt(*bitmap, width() - bitmap->width(),
                          height() - bitmap->height());
  }

  static gfx::Size GetSize() {
    // This is disabled until we find what makes us slower when we let
    // WebKit know that we have a resizer rect...
    // int scrollbar_thickness = gfx::scrollbar_size();
    // return gfx::Size(scrollbar_thickness, scrollbar_thickness);
    return gfx::Size();
  }

  virtual gfx::Size GetPreferredSize() {
    views::Widget* widget = GetWidget();
    return (!widget || widget->IsMaximized() || widget->IsFullscreen()) ?
        gfx::Size() : GetSize();
  }

  virtual void Layout() {
    if (parent()) {
      gfx::Size ps = GetPreferredSize();
      // No need to handle Right to left text direction here,
      // our parent must take care of it for us...
      // TODO(alekseys): fix it.
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
      infobar_container_(NULL),
      sidebar_container_(NULL),
      sidebar_split_(NULL),
      contents_container_(NULL),
      devtools_container_(NULL),
      preview_container_(NULL),
      contents_(NULL),
      contents_split_(NULL),
      initialized_(false),
      ignore_layout_(true),
#if defined(OS_WIN) && !defined(USE_AURA)
      hung_window_detector_(&hung_plugin_action_),
      ticker_(0),
#endif
      force_location_bar_focus_(false) {
  browser_->tabstrip_model()->AddObserver(this);

  registrar_.Add(
      this,
      chrome::NOTIFICATION_SIDEBAR_CHANGED,
      content::Source<SidebarManager>(SidebarManager::GetInstance()));
}

BrowserView::~BrowserView() {
#if defined(USE_AURA)
  // Destroy LauncherIconUpdater early on as it listens to the TabstripModel,
  // which is destroyed by the browser.
  icon_updater_.reset();
#endif

  browser_->tabstrip_model()->RemoveObserver(this);

#if defined(OS_WIN) && !defined(USE_AURA)
  // Remove this observer.
  if (aeropeek_manager_.get())
    browser_->tabstrip_model()->RemoveObserver(aeropeek_manager_.get());

  // Stop hung plugin monitoring.
  ticker_.Stop();
  ticker_.UnregisterTickHandler(&hung_window_detector_);

  // Terminate the jumplist (must be called before browser_->profile() is
  // destroyed.
  if (jumplist_) {
    jumplist_->Terminate();
  }
#endif

#if !defined(OS_CHROMEOS) || defined(USE_AURA)
  // We destroy the download shelf before |browser_| to remove its child
  // download views from the set of download observers (since the observed
  // downloads can be destroyed along with |browser_| and the observer
  // notifications will call back into deleted objects).
  download_shelf_.reset();
#endif

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
BrowserView* BrowserView::GetBrowserViewForBrowser(Browser* browser) {
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
  ConvertPointToView(this, parent(), &container_origin);
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
  gfx::Point window_point(point.Add(GetMirroredPosition()));
  window_point.Offset(0, -frame_->GetHorizontalTabStripVerticalOffset(false));
  return window_point;
}

int BrowserView::GetSidebarWidth() const {
  if (!sidebar_container_ || !sidebar_container_->IsVisible())
    return 0;
  return sidebar_split_->divider_offset();
}

bool BrowserView::IsTabStripVisible() const {
  return browser_->SupportsWindowFeature(Browser::FEATURE_TABSTRIP);
}

bool BrowserView::IsOffTheRecord() const {
  return browser_->profile()->IsOffTheRecord();
}

bool BrowserView::ShouldShowAvatar() const {
  if (!IsBrowserTypeNormal())
    return false;
  if (IsOffTheRecord())
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
          prefs::kAccessibilityEnabled)) {
    ExtensionTtsController::GetInstance()->Stop();
    return false;
  }
#endif

  std::map<ui::Accelerator, int>::const_iterator iter =
      accelerator_table_.find(accelerator);
  DCHECK(iter != accelerator_table_.end());
  int command_id = iter->second;

  if (!browser_->block_command_execution())
    UpdateAcceleratorMetrics(accelerator, command_id);
  return browser_->ExecuteCommandIfEnabled(command_id);
}

bool BrowserView::GetAccelerator(int cmd_id, ui::Accelerator* accelerator) {
  // The standard Ctrl-X, Ctrl-V and Ctrl-C are not defined as accelerators
  // anywhere so we need to check for them explicitly here.
  switch (cmd_id) {
    case IDC_CUT:
      *accelerator = ui::Accelerator(ui::VKEY_X, false, true, false);
      return true;
    case IDC_COPY:
      *accelerator = ui::Accelerator(ui::VKEY_C, false, true, false);
      return true;
    case IDC_PASTE:
      *accelerator = ui::Accelerator(ui::VKEY_V, false, true, false);
      return true;
  }
  // Else, we retrieve the accelerator information from the accelerator table.
  std::map<ui::Accelerator, int>::iterator it = accelerator_table_.begin();
  for (; it != accelerator_table_.end(); ++it) {
    if (it->second == cmd_id) {
      *accelerator = it->first;
      return true;
    }
  }
  return false;
}

bool BrowserView::ActivateAppModalDialog() const {
  // If another browser is app modal, flash and activate the modal browser.
  if (AppModalDialogQueue::GetInstance()->HasActiveDialog()) {
    Browser* active_browser = BrowserList::GetLastActive();
    if (active_browser && (browser_ != active_browser)) {
      active_browser->window()->FlashFrame();
      active_browser->window()->Activate();
    }
    AppModalDialogQueue::GetInstance()->ActivateModalDialog();
    return true;
  }
  return false;
}

TabContents* BrowserView::GetSelectedTabContents() const {
  return browser_->GetSelectedTabContents();
}

TabContentsWrapper* BrowserView::GetSelectedTabContentsWrapper() const {
  return browser_->GetSelectedTabContentsWrapper();
}

SkBitmap BrowserView::GetOTRAvatarIcon() {
  static SkBitmap* otr_avatar_ = new SkBitmap();

  if (otr_avatar_->isNull()) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    *otr_avatar_ = *rb.GetBitmapNamed(IDR_OTR_ICON);
  }
  return *otr_avatar_;
}

#if defined(OS_WIN) && !defined(USE_AURA)
void BrowserView::PrepareToRunSystemMenu(HMENU menu) {
  system_menu_->UpdateStates();
}
#endif

// static
void BrowserView::RegisterBrowserViewPrefs(PrefService* prefs) {
  prefs->RegisterIntegerPref(prefs::kPluginMessageResponseTimeout,
                             kDefaultPluginMessageResponseTimeout);
  prefs->RegisterIntegerPref(prefs::kHungPluginDetectFrequency,
                             kDefaultHungPluginDetectFrequency);
}

bool BrowserView::IsPositionInWindowCaption(const gfx::Point& point) {
  return GetBrowserViewLayout()->IsPositionInWindowCaption(point);
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, BrowserWindow implementation:

void BrowserView::Show() {
  // The same fix as in BrowserWindowGtk::Show.
  //
  // The Browser must become the active browser when Show() is called.
  // But, on Gtk, the browser won't be shown until we return to the runloop.
  // Therefore we need to set the active window here explicitly. otherwise
  // any calls to BrowserList::GetLastActive() (for example, in bookmark_util),
  // will return the previous browser.
  BrowserList::SetLastActive(browser());

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
}

void BrowserView::ShowInactive() {
  if (!frame_->IsVisible())
    frame_->ShowInactive();
}

void BrowserView::SetBounds(const gfx::Rect& bounds) {
  ExitFullscreen();
  GetWidget()->SetBounds(bounds);
}

void BrowserView::Close() {
  BrowserBubbleHost::Close();

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

void BrowserView::FlashFrame() {
#if defined(OS_WIN) && !defined(USE_AURA)
  FLASHWINFO fwi;
  fwi.cbSize = sizeof(fwi);
  fwi.hwnd = frame_->GetNativeWindow();
  fwi.dwFlags = FLASHW_ALL;
  fwi.uCount = 4;
  fwi.dwTimeout = 0;
  FlashWindowEx(&fwi);
#else
  // Doesn't matter for chrome os.
#endif
}

gfx::NativeWindow BrowserView::GetNativeHandle() {
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
  // arguments (to AutoReset<>) must have external linkage.
  enum CallState { NORMAL, REENTRANT, REENTRANT_FORCE_FAST_RESIZE };
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
  UpdateUIForContents(browser_->GetSelectedTabContentsWrapper());
  if (use_fast_resize)
    contents_container_->SetFastResize(false);

  // Inform the InfoBarContainer that the distance to the location icon may have
  // changed.  We have to do this after the block above so that the toolbars are
  // laid out correctly for calculating the maximum arrow height below.
  {
    const LocationIconView* location_icon_view =
        toolbar_->location_bar()->location_icon_view();
    // The +1 in the next line creates a 1-px gap between icon and arrow tip.
    gfx::Point icon_bottom(0, location_icon_view->GetImageBounds().bottom() -
        LocationBarView::kIconInternalPadding + 1);
    ConvertPointToView(location_icon_view, this, &icon_bottom);
    gfx::Point infobar_top(0, infobar_container_->GetVerticalOverlap(NULL));
    ConvertPointToView(infobar_container_, this, &infobar_top);

    AutoReset<CallState> resetter(&call_state,
        is_animating ? REENTRANT_FORCE_FAST_RESIZE : REENTRANT);
    infobar_container_->SetMaxTopArrowHeight(infobar_top.y() - icon_bottom.y());
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
  if (MaybeShowBookmarkBar(browser_->GetSelectedTabContentsWrapper()))
    Layout();
}

void BrowserView::UpdateDevTools() {
  UpdateDevToolsForContents(GetSelectedTabContentsWrapper());
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

gfx::Rect BrowserView::GetRestoredBounds() const {
  return frame_->GetRestoredBounds();
}

gfx::Rect BrowserView::GetBounds() const {
  return frame_->GetWindowScreenBounds();
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

#if defined(OS_WIN) || defined(USE_AURA)
  ProcessFullscreen(true, url, bubble_type);
#else
  // On Linux/gtk changing fullscreen is async. Ask the window to change it's
  // fullscreen state, and when done invoke ProcessFullscreen.
  fullscreen_request_.pending = true;
  fullscreen_request_.url = url;
  fullscreen_request_.bubble_type = bubble_type;
  frame_->SetFullscreen(true);
#endif
}

void BrowserView::ExitFullscreen() {
  if (!IsFullscreen())
    return;  // Nothing to do.

#if defined(OS_WIN) || defined(USE_AURA)
  ProcessFullscreen(false, GURL(), FEB_TYPE_NONE);
#else
  fullscreen_request_.pending = false;
  // On Linux changing fullscreen is async. Ask the window to change it's
  // fullscreen state, and when done invoke ProcessFullscreen.
  frame_->SetFullscreen(false);
#endif
}

void BrowserView::UpdateFullscreenExitBubbleContent(
    const GURL& url,
    FullscreenExitBubbleType bubble_type) {
  if (fullscreen_bubble_.get())
    fullscreen_bubble_->UpdateContent(url, bubble_type);
}

bool BrowserView::IsFullscreen() const {
  return frame_->IsFullscreen();
}

bool BrowserView::IsFullscreenBubbleVisible() const {
  return fullscreen_bubble_.get() ? true : false;
}

void BrowserView::FullScreenStateChanged() {
  if (IsFullscreen()) {
    if (fullscreen_request_.pending) {
      fullscreen_request_.pending = false;
      ProcessFullscreen(true, fullscreen_request_.url,
                        fullscreen_request_.bubble_type);
    } else {
      ProcessFullscreen(true, GURL(),
                        FEB_TYPE_BROWSER_FULLSCREEN_EXIT_INSTRUCTION);
    }
  } else {
    ProcessFullscreen(false, GURL(), FEB_TYPE_NONE);
  }
}

void BrowserView::RestoreFocus() {
  TabContents* selected_tab_contents = GetSelectedTabContents();
  if (selected_tab_contents)
    selected_tab_contents->view()->RestoreFocus();
}

LocationBar* BrowserView::GetLocationBar() const {
  return GetLocationBarView();
}

void BrowserView::SetFocusToLocationBar(bool select_all) {
#if defined(OS_WIN)
  // On Windows changing focus to the location bar causes the browser window
  // to become active. This can steal focus if the user has another window
  // open already.
  if (!force_location_bar_focus_ && !IsActive())
    return;
#endif

  LocationBarView* location_bar = GetLocationBarView();
  if (location_bar->IsFocusableInRootView()) {
    // Location bar got focus.
    location_bar->FocusLocation(select_all);
  } else {
    // If none of location bar got focus,
    // then clear focus.
    views::FocusManager* focus_manager = GetFocusManager();
    DCHECK(focus_manager);
    focus_manager->ClearFocus();
  }
}

void BrowserView::UpdateReloadStopState(bool is_loading, bool force) {
  toolbar_->reload_button()->ChangeMode(
      is_loading ? ReloadButton::MODE_STOP : ReloadButton::MODE_RELOAD, force);
}

void BrowserView::UpdateToolbar(TabContentsWrapper* contents,
                                bool should_restore_state) {
  toolbar_->Update(contents->tab_contents(), should_restore_state);
}

void BrowserView::FocusToolbar() {
  // Start the traversal within the main toolbar. SetPaneFocus stores
  // the current focused view before changing focus.
  toolbar_->SetPaneFocus(NULL);
}

void BrowserView::FocusBookmarksToolbar() {
  if (active_bookmark_bar_ && bookmark_bar_view_->IsVisible())
    bookmark_bar_view_->SetPaneFocus(bookmark_bar_view_.get());
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

  std::vector<views::View*> accessible_views(
      accessible_panes.begin(), accessible_panes.end());
  accessible_views.push_back(GetTabContentsContainerView());
  if (sidebar_container_ && sidebar_container_->IsVisible())
    accessible_views.push_back(GetSidebarContainerView());
  if (devtools_container_->IsVisible())
    accessible_views.push_back(devtools_container_->GetFocusView());
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
    GetFocusManager()->StoreFocusedView();

  // Try to focus the next pane; if SetPaneFocusAndFocusDefault returns
  // false it means the pane didn't have any focusable controls, so skip
  // it and try the next one.
  for (;;) {
    if (forwards)
      index = (index + 1) % count;
    else
      index = ((index - 1) + count) % count;

    if (index < pane_count) {
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
  return browser_->SupportsWindowFeature(Browser::FEATURE_TOOLBAR) ||
         browser_->SupportsWindowFeature(Browser::FEATURE_LOCATIONBAR);
}

void BrowserView::DisableInactiveFrame() {
#if defined(OS_WIN) && !defined(USE_AURA)
  frame_->DisableInactiveRendering();
#endif  // No tricks are needed to get the right behavior on Linux.
}

void BrowserView::ConfirmSetDefaultSearchProvider(TabContents* tab_contents,
                                                  TemplateURL* template_url,
                                                  Profile* profile) {
#if defined(OS_WIN) && !defined(USE_AURA)
  DefaultSearchView::Show(tab_contents, template_url, profile);
#else
  // TODO(levin): Implement for other platforms. Right now this is behind
  // a command line flag which is off. http://crbug.com/38475
#endif
}

void BrowserView::ConfirmAddSearchProvider(const TemplateURL* template_url,
                                           Profile* profile) {
#if defined(USE_AURA)
  browser::ConfirmAddSearchProvider(template_url, profile);
#else
  browser::EditSearchEngine(GetWidget()->GetNativeWindow(), template_url, NULL,
                            profile);
#endif
}

void BrowserView::ToggleBookmarkBar() {
  bookmark_utils::ToggleWhenVisible(browser_->profile());
}

void BrowserView::ShowAboutChromeDialog() {
  DoShowAboutChromeDialog();
}

views::Widget* BrowserView::DoShowAboutChromeDialog() {
  return browser::ShowAboutChromeView(GetWidget()->GetNativeWindow(),
                                      browser_->profile());
}

void BrowserView::ShowUpdateChromeDialog() {
  UpdateRecommendedMessageBox::ShowMessageBox(GetWidget()->GetNativeWindow());
}

void BrowserView::ShowTaskManager() {
#if defined(WEBUI_TASK_MANAGER)
  TaskManagerDialog::Show();
#else
  // Uses WebUI TaskManager when swiches is set. It is beta feature.
  if (CommandLine::ForCurrentProcess()
        ->HasSwitch(switches::kEnableWebUITaskManager)) {
    TaskManagerDialog::Show();
  } else {
    browser::ShowTaskManager();
  }
#endif  // defined(WEBUI_TASK_MANAGER)
}

void BrowserView::ShowBackgroundPages() {
#if defined(WEBUI_TASK_MANAGER)
  TaskManagerDialog::ShowBackgroundPages();
#else
  // Uses WebUI TaskManager when swiches is set. It is beta feature.
  if (CommandLine::ForCurrentProcess()
        ->HasSwitch(switches::kEnableWebUITaskManager)) {
    TaskManagerDialog::ShowBackgroundPages();
  } else {
    browser::ShowBackgroundPages();
  }
#endif  // defined(WEBUI_TASK_MANAGER)
}

void BrowserView::ShowBookmarkBubble(const GURL& url, bool already_bookmarked) {
  GetLocationBarView()->ShowStarBubble(url, !already_bookmarked);
}

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
#if defined(OS_CHROMEOS) && !defined(USE_AURA)
  return false;
#else
  return download_shelf_.get() && download_shelf_->IsShowing();
#endif
}

DownloadShelf* BrowserView::GetDownloadShelf() {
#if defined(OS_CHROMEOS) && !defined(USE_AURA)
  NOTREACHED();
  return NULL;
#else
  if (!download_shelf_.get()) {
    download_shelf_.reset(new DownloadShelfView(browser_.get(), this));
    download_shelf_->set_parent_owned(false);
  }
  return download_shelf_.get();
#endif
}

void BrowserView::ShowRepostFormWarningDialog(TabContents* tab_contents) {
  browser::ShowRepostFormWarningDialog(GetNativeHandle(), tab_contents);
}

void BrowserView::ShowCollectedCookiesDialog(TabContentsWrapper* wrapper) {
  browser::ShowCollectedCookiesDialog(GetNativeHandle(), wrapper);
}

void BrowserView::ConfirmBrowserCloseWithPendingDownloads() {
  DownloadInProgressDialogView* view =
      new DownloadInProgressDialogView(browser_.get());
  browser::CreateViewsWindow(GetNativeHandle(), view)->Show();
}

void BrowserView::ShowCreateWebAppShortcutsDialog(
    TabContentsWrapper* tab_contents) {
  browser::ShowCreateWebAppShortcutsDialog(GetNativeHandle(), tab_contents);
}

void BrowserView::ShowCreateChromeAppShortcutsDialog(Profile* profile,
                                                     const Extension* app) {
  browser::ShowCreateChromeAppShortcutsDialog(GetNativeHandle(), profile, app);
}

void BrowserView::UserChangedTheme() {
  frame_->FrameTypeChanged();
}

int BrowserView::GetExtraRenderViewHeight() const {
  // Currently this is only used on linux.
  return 0;
}

void BrowserView::TabContentsFocused(TabContents* tab_contents) {
  contents_container_->TabContentsFocused(tab_contents);
}

void BrowserView::ShowPageInfo(Profile* profile,
                               const GURL& url,
                               const NavigationEntry::SSLStatus& ssl,
                               bool show_history) {
  browser::ShowPageInfoBubble(GetLocationBarView()->location_icon_view(),
                              profile, url, ssl, show_history);
}

void BrowserView::ShowAppMenu() {
  toolbar_->app_menu()->Activate();
}

bool BrowserView::PreHandleKeyboardEvent(const NativeWebKeyboardEvent& event,
                                         bool* is_keyboard_shortcut) {
  if (event.type != WebKit::WebInputEvent::RawKeyDown)
    return false;

#if defined(OS_WIN) && !defined(USE_AURA)
  // As Alt+F4 is the close-app keyboard shortcut, it needs processing
  // immediately.
  if (event.windowsKeyCode == ui::VKEY_F4 &&
      event.modifiers == NativeWebKeyboardEvent::AltKey) {
    DefWindowProc(event.os_event.hwnd, event.os_event.message,
                  event.os_event.wParam, event.os_event.lParam);
    return true;
  }
#endif

  views::FocusManager* focus_manager = GetFocusManager();
  DCHECK(focus_manager);

#if defined(TOOLKIT_USES_GTK) && !defined(TOUCH_UI)
  // Views and WebKit use different tables for GdkEventKey -> views::KeyEvent
  // conversion. We need to use View's conversion table here to keep consistent
  // behavior with views::FocusManager::OnKeyEvent() method.
  // TODO(suzhe): We need to check if Windows code also has this issue, and
  // it'll be best if we can unify these conversion tables.
  // See http://crbug.com/54315
  views::KeyEvent views_event(reinterpret_cast<GdkEvent*>(event.os_event));
  ui::Accelerator accelerator(views_event.key_code(),
                              views_event.IsShiftDown(),
                              views_event.IsControlDown(),
                              views_event.IsAltDown());
#else
  ui::Accelerator accelerator(
      static_cast<ui::KeyboardCode>(event.windowsKeyCode),
      (event.modifiers & NativeWebKeyboardEvent::ShiftKey) ==
          NativeWebKeyboardEvent::ShiftKey,
      (event.modifiers & NativeWebKeyboardEvent::ControlKey) ==
          NativeWebKeyboardEvent::ControlKey,
      (event.modifiers & NativeWebKeyboardEvent::AltKey) ==
          NativeWebKeyboardEvent::AltKey);
#endif

  // We first find out the browser command associated to the |event|.
  // Then if the command is a reserved one, and should be processed
  // immediately according to the |event|, the command will be executed
  // immediately. Otherwise we just set |*is_keyboard_shortcut| properly and
  // return false.

  // This piece of code is based on the fact that accelerators registered
  // into the |focus_manager| may only trigger a browser command execution.
  //
  // Here we need to retrieve the command id (if any) associated to the
  // keyboard event. Instead of looking up the command id in the
  // |accelerator_table_| by ourselves, we block the command execution of
  // the |browser_| object then send the keyboard event to the
  // |focus_manager| as if we are activating an accelerator key.
  // Then we can retrieve the command id from the |browser_| object.
  browser_->SetBlockCommandExecution(true);
  focus_manager->ProcessAccelerator(accelerator);
  int id = browser_->GetLastBlockedCommand(NULL);
  browser_->SetBlockCommandExecution(false);

  if (id == -1)
    return false;

  // Executing the command may cause |this| object to be destroyed.
#if defined(TOOLKIT_USES_GTK) && !defined(TOUCH_UI)
  if (browser_->IsReservedCommandOrKey(id, event) &&
      !event.match_edit_command) {
#else
  if (browser_->IsReservedCommandOrKey(id, event)) {
#endif
    UpdateAcceleratorMetrics(accelerator, id);
    return browser_->ExecuteCommandIfEnabled(id);
  }

  DCHECK(is_keyboard_shortcut != NULL);
  *is_keyboard_shortcut = true;

  return false;
}

void BrowserView::HandleKeyboardEvent(const NativeWebKeyboardEvent& event) {
  // TODO(ben): figure out why are these two code paths so different
#if defined(TOOLKIT_USES_GTK) && !defined(TOUCH_UI)
  HandleWebKeyboardEvent(GetWidget(), event);
#else
  unhandled_keyboard_event_handler_.HandleKeyboardEvent(event,
                                                        GetFocusManager());
#endif
}

// TODO(devint): http://b/issue?id=1117225 Cut, Copy, and Paste are always
// enabled in the page menu regardless of whether the command will do
// anything. When someone selects the menu item, we just act as if they hit
// the keyboard shortcut for the command by sending the associated key press
// to windows. The real fix to this bug is to disable the commands when they
// won't do anything. We'll need something like an overall clipboard command
// manager to do that.
void BrowserView::Cut() {
  ui_controls::SendKeyPress(GetNativeHandle(), ui::VKEY_X,
                            true, false, false, false);
}

void BrowserView::Copy() {
  ui_controls::SendKeyPress(GetNativeHandle(), ui::VKEY_C,
                            true, false, false, false);
}

void BrowserView::Paste() {
  ui_controls::SendKeyPress(GetNativeHandle(), ui::VKEY_V,
                            true, false, false, false);
}

void BrowserView::ShowInstant(TabContentsWrapper* preview) {
  if (!preview_container_)
    preview_container_ = new TabContentsContainer();
  contents_->SetPreview(preview_container_, preview->tab_contents());
  preview_container_->ChangeTabContents(preview->tab_contents());
}

void BrowserView::HideInstant() {
  if (!preview_container_)
    return;

  // The contents must be changed before SetPreview is invoked.
  preview_container_->ChangeTabContents(NULL);
  contents_->SetPreview(NULL, NULL);
  delete preview_container_;
  preview_container_ = NULL;
}

gfx::Rect BrowserView::GetInstantBounds() {
  return contents_->GetPreviewBounds();
}

WindowOpenDisposition BrowserView::GetDispositionForPopupBounds(
    const gfx::Rect& bounds) {
#if defined(USE_AURA) && defined(OS_CHROMEOS)
  // If a popup is larger than a given fraction of the screen, turn it into
  // a foreground tab. Also check for width or height == 0, which would
  // indicate a tab sized popup window.
  gfx::Size size = gfx::Screen::GetMonitorAreaNearestWindow(
      GetWidget()->GetNativeView()).size();
  int max_width = size.width() * kPopupMaxWidthFactor;
  int max_height = size.height() * kPopupMaxHeightFactor;

  if (bounds.width() > max_width ||
      bounds.width() == 0 ||
      bounds.height() > max_height ||
      bounds.height() == 0) {
    return NEW_FOREGROUND_TAB;
  }
#endif
  return NEW_POPUP;
}

FindBar* BrowserView::CreateFindBar() {
  return browser::CreateFindBar(this);
}

#if defined(OS_CHROMEOS)
void BrowserView::ShowMobileSetup() {
  MobileSetupDialog::Show();
}

void BrowserView::ShowKeyboardOverlay(gfx::NativeWindow owning_window) {
#if defined(TOOLKIT_USES_GTK)
  KeyboardOverlayDialogView::ShowDialog(owning_window, this);
#else
  // TODO(saintlou): Add Keyboard Overlay for Aura: crbug.com/99858.
  NOTIMPLEMENTED();
#endif
}
#endif

///////////////////////////////////////////////////////////////////////////////
// BrowserView, BrowserWindowTesting implementation:

BookmarkBarView* BrowserView::GetBookmarkBarView() const {
  return bookmark_bar_view_.get();
}

LocationBarView* BrowserView::GetLocationBarView() const {
  return toolbar_ ? toolbar_->location_bar() : NULL;
}

views::View* BrowserView::GetTabContentsContainerView() const {
  return contents_container_->GetFocusView();
}

views::View* BrowserView::GetSidebarContainerView() const {
  if (!sidebar_container_)
    return NULL;
  return sidebar_container_->GetFocusView();
}

ToolbarView* BrowserView::GetToolbarView() const {
  return toolbar_;
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, content::NotificationObserver implementation:

void BrowserView::Observe(int type,
                          const content::NotificationSource& source,
                          const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_SIDEBAR_CHANGED:
      if (content::Details<SidebarContainer>(details)->tab_contents() ==
          browser_->GetSelectedTabContents()) {
        UpdateSidebar();
      }
      break;

    default:
      NOTREACHED() << "Got a notification we didn't register for!";
      break;
  }
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, TabStripModelObserver implementation:

void BrowserView::TabDetachedAt(TabContentsWrapper* contents, int index) {
  // We use index here rather than comparing |contents| because by this time
  // the model has already removed |contents| from its list, so
  // browser_->GetSelectedTabContents() will return NULL or something else.
  if (index == browser_->tabstrip_model()->active_index()) {
    // We need to reset the current tab contents to NULL before it gets
    // freed. This is because the focus manager performs some operations
    // on the selected TabContents when it is removed.
    contents_container_->ChangeTabContents(NULL);
    infobar_container_->ChangeTabContents(NULL);
    UpdateSidebarForContents(NULL);
    UpdateDevToolsForContents(NULL);
  }
}

void BrowserView::TabDeactivated(TabContentsWrapper* contents) {
  // We do not store the focus when closing the tab to work-around bug 4633.
  // Some reports seem to show that the focus manager and/or focused view can
  // be garbage at that point, it is not clear why.
  if (!contents->tab_contents()->is_being_destroyed())
    contents->view()->StoreFocus();
}

void BrowserView::ActiveTabChanged(TabContentsWrapper* old_contents,
                                   TabContentsWrapper* new_contents,
                                   int index,
                                   bool user_gesture) {
  ProcessTabSelected(new_contents);
}

void BrowserView::TabReplacedAt(TabStripModel* tab_strip_model,
                                TabContentsWrapper* old_contents,
                                TabContentsWrapper* new_contents,
                                int index) {
  if (index != browser_->tabstrip_model()->active_index())
    return;

  if (contents_->preview_tab_contents() == new_contents->tab_contents()) {
    // If 'preview' is becoming active, swap the 'active' and 'preview' and
    // delete what was the active.
    contents_->MakePreviewContentsActiveContents();
    TabContentsContainer* old_container = contents_container_;
    contents_container_ = preview_container_;
    old_container->ChangeTabContents(NULL);
    delete old_container;
    preview_container_ = NULL;
  }
  // Update the UI for the new contents.
  ProcessTabSelected(new_contents);
}

void BrowserView::TabStripEmpty() {
  // Make sure all optional UI is removed before we are destroyed, otherwise
  // there will be consequences (since our view hierarchy will still have
  // references to freed views).
  UpdateUIForContents(NULL);
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, ui::SimpleMenuModel::Delegate implementation:

bool BrowserView::IsCommandIdChecked(int command_id) const {
  // TODO(beng): encoding menu.
  // No items in our system menu are check-able.
  return false;
}

bool BrowserView::IsCommandIdEnabled(int command_id) const {
  return browser_->command_updater()->IsCommandEnabled(command_id);
}

bool BrowserView::GetAcceleratorForCommandId(int command_id,
                                             ui::Accelerator* accelerator) {
  // Let's let the ToolbarView own the canonical implementation of this method.
  return toolbar_->GetAcceleratorForCommandId(command_id, accelerator);
}

bool BrowserView::IsItemForCommandIdDynamic(int command_id) const {
  return command_id == IDC_RESTORE_TAB;
}

string16 BrowserView::GetLabelForCommandId(int command_id) const {
  DCHECK(command_id == IDC_RESTORE_TAB);

  int string_id = IDS_RESTORE_TAB;
  if (IsCommandIdEnabled(command_id)) {
    TabRestoreService* trs =
        TabRestoreServiceFactory::GetForProfile(browser_->profile());
    if (trs && trs->entries().front()->type == TabRestoreService::WINDOW)
      string_id = IDS_RESTORE_WINDOW;
  }
  return l10n_util::GetStringUTF16(string_id);
}

void BrowserView::ExecuteCommand(int command_id) {
  browser_->ExecuteCommandIfEnabled(command_id);
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
  return !ActivateAppModalDialog();
}

bool BrowserView::IsModal() const {
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
  return browser_->SupportsWindowFeature(Browser::FEATURE_TITLEBAR);
}

SkBitmap BrowserView::GetWindowAppIcon() {
  if (browser_->is_app()) {
    TabContentsWrapper* contents = browser_->GetSelectedTabContentsWrapper();
    if (contents && contents->extension_tab_helper()->GetExtensionAppIcon())
      return *contents->extension_tab_helper()->GetExtensionAppIcon();
  }

  return GetWindowIcon();
}

SkBitmap BrowserView::GetWindowIcon() {
  if (browser_->is_app())
    return browser_->GetCurrentPageIcon();
  return SkBitmap();
}

bool BrowserView::ShouldShowWindowIcon() const {
  return browser_->SupportsWindowFeature(Browser::FEATURE_TITLEBAR);
}

bool BrowserView::ExecuteWindowsCommand(int command_id) {
  // This function handles WM_SYSCOMMAND, WM_APPCOMMAND, and WM_COMMAND.
#if defined(OS_WIN) && !defined(USE_AURA)
  if (command_id == IDC_DEBUG_FRAME_TOGGLE)
    GetWidget()->DebugToggleFrameType();
#endif
  // Translate WM_APPCOMMAND command ids into a command id that the browser
  // knows how to handle.
  int command_id_from_app_command = GetCommandIDForAppCommandID(command_id);
  if (command_id_from_app_command != -1)
    command_id = command_id_from_app_command;

  return browser_->ExecuteCommandIfEnabled(command_id);
}

std::string BrowserView::GetWindowName() const {
  return browser_->GetWindowPlacementKey();
}

void BrowserView::SaveWindowPlacement(const gfx::Rect& bounds,
                                      ui::WindowShowState show_state) {
  // If IsFullscreen() is true, we've just changed into fullscreen mode, and
  // we're catching the going-into-fullscreen sizing and positioning calls,
  // which we want to ignore.
  if (!IsFullscreen() &&
      (browser_->ShouldSaveWindowPlacement() || browser_->is_app())) {
    WidgetDelegate::SaveWindowPlacement(bounds, show_state);
    browser_->SaveWindowPlacement(bounds, show_state);
  }
}

bool BrowserView::GetSavedWindowPlacement(
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {
  *bounds = browser_->GetSavedWindowBounds();
  *show_state = browser_->GetSavedWindowShowState();

  if ((browser_->is_type_popup() || browser_->is_type_panel())
      && !browser_->is_devtools() && !browser_->is_app()) {
    // We are a popup window. The value passed in |bounds| represents two
    // pieces of information:
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
  if (active) {
    BrowserList::SetLastActive(browser_.get());
    browser_->OnWindowActivated();
  }
}

void BrowserView::OnWindowBeginUserBoundsChange() {
  TabContents* tab_contents = GetSelectedTabContents();
  if (!tab_contents)
    return;
  tab_contents->render_view_host()->NotifyMoveOrResizeStarted();
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

  BrowserBubbleHost::WindowMoved();

  browser::HideBookmarkBubbleView();

  // Close the omnibox popup, if any.
  LocationBarView* location_bar_view = GetLocationBarView();
  if (location_bar_view)
    location_bar_view->location_entry()->ClosePopup();
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

  if (!browser_->tabstrip_model()->empty()) {
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
#if !defined(OS_CHROMEOS) || defined(USE_AURA)
  if (download_shelf_.get())
    panes->push_back(download_shelf_.get());
#endif
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

#if defined(OS_WIN) && !defined(USE_AURA)
  // Send the margins of the "user-perceived content area" of this
  // browser window so AeroPeekManager can render a background-tab image in
  // the area.
  // TODO(pkasting) correct content inset??
  if (aeropeek_manager_.get()) {
    gfx::Insets insets(GetFindBarBoundingBox().y() + 1,
                       0,
                       0,
                       0);
    aeropeek_manager_->SetContentInsets(insets);
  }
#endif
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
  if (is_add && child == this && GetWidget() && !initialized_) {
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

SkColor BrowserView::GetInfoBarSeparatorColor() const {
  // NOTE: Keep this in sync with ToolbarView::OnPaint()!
  return (IsTabStripVisible() || !frame_->ShouldUseNativeFrame()) ?
      ResourceBundle::toolbar_separator_color : SK_ColorBLACK;
}

void BrowserView::InfoBarContainerStateChanged(bool is_animating) {
  ToolbarSizeChanged(is_animating);
}

bool BrowserView::DrawInfoBarArrows(int* x) const {
  if (x) {
    const LocationIconView* location_icon_view =
        toolbar_->location_bar()->location_icon_view();
    gfx::Point icon_center(location_icon_view->GetImageBounds().CenterPoint());
    ConvertPointToView(location_icon_view, this, &icon_center);
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

views::LayoutManager* BrowserView::CreateLayoutManager() const {
  return new BrowserViewLayout;
}

void BrowserView::InitTabStrip(TabStripModel* model) {
  // Throw away the existing tabstrip if we're switching display modes.
  scoped_ptr<AbstractTabStripView> old_strip(tabstrip_);
  if (tabstrip_)
    tabstrip_->parent()->RemoveChildView(tabstrip_);

  tabstrip_ = CreateTabStrip(browser_.get(), this, model);
}

ToolbarView* BrowserView::CreateToolbar() const {
  return new ToolbarView(browser_.get());
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

  // Start a hung plugin window detector for this browser object (as long as
  // hang detection is not disabled).
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableHangMonitor)) {
    InitHangMonitor();
  }

  LoadAccelerators();

  InitTabStrip(browser_->tabstrip_model());

  SetToolbar(CreateToolbar());

  infobar_container_ = new InfoBarContainerView(this);
  AddChildView(infobar_container_);

  contents_container_ = new TabContentsContainer;
  contents_ = new ContentsContainer(contents_container_);

  SkColor bg_color = GetWidget()->GetThemeProvider()->
      GetColor(ThemeService::COLOR_TOOLBAR);

  bool sidebar_allowed = SidebarManager::IsSidebarAllowed();
  if (sidebar_allowed) {
    sidebar_container_ = new TabContentsContainer;
    sidebar_container_->set_id(VIEW_ID_SIDE_BAR_CONTAINER);
    sidebar_container_->SetVisible(false);

    sidebar_split_ = new views::SingleSplitView(
        contents_,
        sidebar_container_,
        views::SingleSplitView::HORIZONTAL_SPLIT,
        this);
    sidebar_split_->set_id(VIEW_ID_SIDE_BAR_SPLIT);
    sidebar_split_->SetAccessibleName(
        l10n_util::GetStringUTF16(IDS_ACCNAME_SIDE_BAR));
    sidebar_split_->set_background(
        views::Background::CreateSolidBackground(bg_color));
  }

  devtools_container_ = new TabContentsContainer;
  devtools_container_->set_id(VIEW_ID_DEV_TOOLS_DOCKED);
  devtools_container_->SetVisible(false);

  views::View* contents_view = contents_;
  if (sidebar_allowed)
    contents_view = sidebar_split_;

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

#if defined(TOUCH_UI)
  status_bubble_.reset(new StatusBubbleTouch(contents_));
#else
  status_bubble_.reset(new StatusBubbleViews(contents_));
#endif

#if defined(OS_WIN) && !defined(USE_AURA)
  InitSystemMenu();

  // Create a custom JumpList and add it to an observer of TabRestoreService
  // so we can update the custom JumpList when a tab is added or removed.
  if (JumpList::Enabled()) {
    jumplist_ = new JumpList();
    jumplist_->AddObserver(browser_->profile());
  }

  if (AeroPeekManager::Enabled()) {
    aeropeek_manager_.reset(new AeroPeekManager(
        frame_->GetNativeWindow()));
    browser_->tabstrip_model()->AddObserver(aeropeek_manager_.get());
  }
#endif

  // We're now initialized and ready to process Layout requests.
  ignore_layout_ = false;

#if defined(USE_AURA)
  aura_shell::LauncherItemType type;
  if (ChromeShellDelegate::ShouldCreateLauncherItemForBrowser(browser_.get(),
                                                              &type)) {
    icon_updater_.reset(new LauncherIconUpdater(
        browser_->tabstrip_model(),
        aura_shell::Shell::GetInstance()->launcher()->model(),
        frame_->GetNativeWindow()));
  }
#endif
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
    TabContents* tab_contents = browser_->GetSelectedTabContents();
    // GetSelectedTabContents can return NULL for example under Purify when
    // the animations are running slowly and this function is called on a timer
    // through LoadingAnimationCallback.
    frame_->UpdateThrobber(tab_contents && tab_contents->IsLoading());
  }
}

// BrowserView, private --------------------------------------------------------

#if defined(OS_WIN) && !defined(USE_AURA)
void BrowserView::InitSystemMenu() {
  system_menu_contents_.reset(new views::SystemMenuModel(this));
  // We add the menu items in reverse order so that insertion_index never needs
  // to change.
  if (IsBrowserTypeNormal())
    BuildSystemMenuForBrowserWindow();
  else
    BuildSystemMenuForAppOrPopupWindow();
  system_menu_.reset(
      new views::NativeMenuWin(system_menu_contents_.get(),
                               frame_->GetNativeWindow()));
  system_menu_->Rebuild();
}
#endif

BrowserViewLayout* BrowserView::GetBrowserViewLayout() const {
  return static_cast<BrowserViewLayout*>(GetLayoutManager());
}

void BrowserView::LayoutStatusBubble() {
  // In restored mode, the client area has a client edge between it and the
  // frame.
  int overlap = StatusBubbleViews::kShadowThickness +
      (IsMaximized() ? 0 : views::NonClientFrameView::kClientEdgeThickness);
  int height = status_bubble_->GetPreferredSize().height();
  int contents_height = status_bubble_->base_view()->bounds().height();
  gfx::Point origin(-overlap, contents_height - height + overlap);
  status_bubble_->SetBounds(origin.x(), origin.y(), width() / 3, height);
}

bool BrowserView::MaybeShowBookmarkBar(TabContentsWrapper* contents) {
  views::View* new_bookmark_bar_view = NULL;
  if (browser_->SupportsWindowFeature(Browser::FEATURE_BOOKMARKBAR) &&
      contents) {
    if (!bookmark_bar_view_.get()) {
      bookmark_bar_view_.reset(new BookmarkBarView(browser_.get()));
      bookmark_bar_view_->set_parent_owned(false);
      bookmark_bar_view_->set_background(
          new BookmarkExtensionBackground(this, bookmark_bar_view_.get(),
                                          browser_.get()));
      bookmark_bar_view_->SetBookmarkBarState(
          browser_->bookmark_bar_state(),
          BookmarkBar::DONT_ANIMATE_STATE_CHANGE);
    }
    bookmark_bar_view_->SetPageNavigator(contents->tab_contents());
    new_bookmark_bar_view = bookmark_bar_view_.get();
  }
  return UpdateChildViewAndLayout(new_bookmark_bar_view, &active_bookmark_bar_);
}

bool BrowserView::MaybeShowInfoBar(TabContentsWrapper* contents) {
  // TODO(beng): Remove this function once the interface between
  //             InfoBarContainer, DownloadShelfView and TabContents and this
  //             view is sorted out.
  return true;
}

void BrowserView::UpdateSidebar() {
  UpdateSidebarForContents(GetSelectedTabContentsWrapper());
  Layout();
}

void BrowserView::UpdateSidebarForContents(TabContentsWrapper* tab_contents) {
  if (!sidebar_container_)
    return;  // Happens when sidebar is not allowed.
  if (!SidebarManager::GetInstance())
    return;  // Happens only in tests.

  TabContents* sidebar_contents = NULL;
  if (tab_contents) {
    SidebarContainer* client_host = SidebarManager::GetInstance()->
        GetActiveSidebarContainerFor(tab_contents->tab_contents());
    if (client_host)
      sidebar_contents = client_host->sidebar_contents();
  }

  bool visible = NULL != sidebar_contents &&
                 browser_->SupportsWindowFeature(Browser::FEATURE_SIDEBAR);

  bool should_show = visible && !sidebar_container_->IsVisible();
  bool should_hide = !visible && sidebar_container_->IsVisible();

  // Update sidebar content.
  TabContents* old_contents = sidebar_container_->tab_contents();
  sidebar_container_->ChangeTabContents(sidebar_contents);
  SidebarManager::GetInstance()->
      NotifyStateChanges(old_contents, sidebar_contents);

  // Update sidebar UI width.
  if (should_show) {
    // Restore split offset.
    int sidebar_width = g_browser_process->local_state()->GetInteger(
        prefs::kExtensionSidebarWidth);
    if (sidebar_width < 0) {
      // Initial load, set to default value.
      sidebar_width = sidebar_split_->width() / 7;
    }
    // Make sure user can see both panes.
    int min_sidebar_width = sidebar_split_->GetMinimumSize().width();
    sidebar_width = std::min(sidebar_split_->width() - min_sidebar_width,
                             std::max(min_sidebar_width, sidebar_width));

    sidebar_split_->set_divider_offset(
        sidebar_split_->width() - sidebar_width);

    sidebar_container_->SetVisible(true);
    sidebar_split_->InvalidateLayout();
    Layout();
  } else if (should_hide) {
    // Store split offset when hiding sidebar only.
    g_browser_process->local_state()->SetInteger(
        prefs::kExtensionSidebarWidth,
        sidebar_split_->width() - sidebar_split_->divider_offset());

    sidebar_container_->SetVisible(false);
    sidebar_split_->InvalidateLayout();
    Layout();
  }
}

void BrowserView::UpdateDevToolsForContents(TabContentsWrapper* wrapper) {
  TabContents* devtools_contents = NULL;
  if (wrapper) {
    TabContentsWrapper* devtools_contents_wrapper =
        DevToolsWindow::GetDevToolsContents(wrapper->tab_contents());
    if (devtools_contents_wrapper)
      devtools_contents = devtools_contents_wrapper->tab_contents();
  }

  bool should_show = devtools_contents && !devtools_container_->IsVisible();
  bool should_hide = !devtools_contents && devtools_container_->IsVisible();

  devtools_container_->ChangeTabContents(devtools_contents);

  if (should_show) {
    if (!devtools_focus_tracker_.get()) {
      // Install devtools focus tracker when dev tools window is shown for the
      // first time.
      devtools_focus_tracker_.reset(
          new views::ExternalFocusTracker(devtools_container_,
                                          GetFocusManager()));
    }

    // Restore split offset.
    int split_offset = browser_->profile()->GetPrefs()->
        GetInteger(prefs::kDevToolsSplitLocation);
    if (split_offset == -1)
      split_offset = contents_split_->height() * 2 / 3;

    // Make sure user can see both panes.
    split_offset = std::min(contents_split_->height() - kMinDevToolsHeight,
                            split_offset);
    split_offset = std::max(kMinContentsHeight, split_offset);
    if (split_offset < 0)
      split_offset = contents_split_->height() * 2 / 3;
    contents_split_->set_divider_offset(split_offset);

    devtools_container_->SetVisible(true);
    contents_split_->InvalidateLayout();
    Layout();
  } else if (should_hide) {
    // Store split offset when hiding devtools window only.
    browser_->profile()->GetPrefs()->SetInteger(prefs::kDevToolsSplitLocation,
        contents_split_->divider_offset());

    // Restore focus to the last focused view when hiding devtools window.
    devtools_focus_tracker_->FocusLastFocusedExternalView();

    devtools_container_->SetVisible(false);
    contents_split_->InvalidateLayout();
    Layout();
  }
}

void BrowserView::UpdateUIForContents(TabContentsWrapper* contents) {
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
                                    const GURL& url,
                                    FullscreenExitBubbleType bubble_type) {
  // Reduce jankiness during the following position changes by:
  //   * Hiding the window until it's in the final position
  //   * Ignoring all intervening Layout() calls, which resize the webpage and
  //     thus are slow and look ugly
  ignore_layout_ = true;
  LocationBarView* location_bar = GetLocationBarView();
#if defined(OS_WIN) && !defined(USE_AURA)
  OmniboxViewWin* omnibox_view =
      static_cast<OmniboxViewWin*>(location_bar->location_entry());
#endif
  if (!fullscreen) {
    // Hide the fullscreen bubble as soon as possible, since the mode toggle can
    // take enough time for the user to notice.
    fullscreen_bubble_.reset();
  } else {
    // Move focus out of the location bar if necessary.
    views::FocusManager* focus_manager = GetFocusManager();
    DCHECK(focus_manager);
    if (focus_manager->GetFocusedView() == location_bar)
      focus_manager->ClearFocus();

#if defined(OS_WIN) && !defined(USE_AURA)
    // If we don't hide the edit and force it to not show until we come out of
    // fullscreen, then if the user was on the New Tab Page, the edit contents
    // will appear atop the web contents once we go into fullscreen mode.  This
    // has something to do with how we move the main window while it's hidden;
    // if we don't hide the main window below, we don't get this problem.
    omnibox_view->set_force_hidden(true);
    ShowWindow(omnibox_view->m_hWnd, SW_HIDE);
#endif
  }
#if defined(OS_WIN) && !defined(USE_AURA)
  static_cast<views::NativeWidgetWin*>(frame_->native_widget())->
      PushForceHidden();
#endif

  // Toggle fullscreen mode.
#if defined(OS_WIN) || defined(USE_AURA)
  frame_->SetFullscreen(fullscreen);
#endif  // No need to invoke SetFullscreen for linux/gtk as this code
        // is executed once we're already fullscreen on linux.

  browser_->WindowFullscreenStateChanged();

  if (fullscreen) {
    bool is_kiosk =
        CommandLine::ForCurrentProcess()->HasSwitch(switches::kKioskMode);
    if (!is_kiosk) {
      fullscreen_bubble_.reset(new FullscreenExitBubbleViews(
          GetWidget(), browser_.get(), url, bubble_type));
    }
  } else {
#if defined(OS_WIN) && !defined(USE_AURA)
    // Show the edit again since we're no longer in fullscreen mode.
    omnibox_view->set_force_hidden(false);
    ShowWindow(omnibox_view->m_hWnd, SW_SHOW);
#endif
  }

  // Undo our anti-jankiness hacks and force the window to re-layout now that
  // it's in its final position.
  ignore_layout_ = false;
  Layout();
#if defined(OS_WIN) && !defined(USE_AURA)
  static_cast<views::NativeWidgetWin*>(frame_->native_widget())->
      PopForceHidden();
#endif
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
    bool alt_down = (accelerators[i].fVirt & FALT) == FALT;
    bool ctrl_down = (accelerators[i].fVirt & FCONTROL) == FCONTROL;
    bool shift_down = (accelerators[i].fVirt & FSHIFT) == FSHIFT;
    ui::Accelerator accelerator(
        static_cast<ui::KeyboardCode>(accelerators[i].key),
        shift_down, ctrl_down, alt_down);
    accelerator_table_[accelerator] = accelerators[i].cmd;

    // Also register with the focus manager.
    focus_manager->RegisterAccelerator(accelerator, this);
  }

  // We don't need the Windows accelerator table anymore.
  free(accelerators);
#else
  views::FocusManager* focus_manager = GetFocusManager();
  DCHECK(focus_manager);
  // Let's fill our own accelerator table.
  for (size_t i = 0; i < browser::kAcceleratorMapLength; ++i) {
    ui::Accelerator accelerator(browser::kAcceleratorMap[i].keycode,
                                browser::kAcceleratorMap[i].shift_pressed,
                                browser::kAcceleratorMap[i].ctrl_pressed,
                                browser::kAcceleratorMap[i].alt_pressed);
    accelerator_table_[accelerator] = browser::kAcceleratorMap[i].command_id;

    // Also register with the focus manager.
    focus_manager->RegisterAccelerator(accelerator, this);
  }
#endif
}

#if defined(OS_WIN) && !defined(USE_AURA)
void BrowserView::BuildSystemMenuForBrowserWindow() {
  system_menu_contents_->AddSeparator();
  system_menu_contents_->AddItemWithStringId(IDC_TASK_MANAGER,
                                             IDS_TASK_MANAGER);
  system_menu_contents_->AddSeparator();
  system_menu_contents_->AddItemWithStringId(IDC_RESTORE_TAB, IDS_RESTORE_TAB);
  system_menu_contents_->AddItemWithStringId(IDC_NEW_TAB, IDS_NEW_TAB);
  AddFrameToggleItems();
  // If it's a regular browser window with tabs, we don't add any more items,
  // since it already has menus (Page, Chrome).
}

void BrowserView::BuildSystemMenuForAppOrPopupWindow() {
  if (browser_->is_app()) {
    system_menu_contents_->AddSeparator();
    system_menu_contents_->AddItemWithStringId(IDC_TASK_MANAGER,
                                               IDS_TASK_MANAGER);
  }
  system_menu_contents_->AddSeparator();
  encoding_menu_contents_.reset(new EncodingMenuModel(browser_.get()));
  system_menu_contents_->AddSubMenuWithStringId(IDC_ENCODING_MENU,
                                                IDS_ENCODING_MENU,
                                                encoding_menu_contents_.get());
  zoom_menu_contents_.reset(new ZoomMenuModel(this));
  system_menu_contents_->AddSubMenuWithStringId(IDC_ZOOM_MENU, IDS_ZOOM_MENU,
                                                zoom_menu_contents_.get());
  system_menu_contents_->AddItemWithStringId(IDC_PRINT, IDS_PRINT);
  system_menu_contents_->AddItemWithStringId(IDC_FIND, IDS_FIND);
  system_menu_contents_->AddSeparator();
  system_menu_contents_->AddItemWithStringId(IDC_PASTE, IDS_PASTE);
  system_menu_contents_->AddItemWithStringId(IDC_COPY, IDS_COPY);
  system_menu_contents_->AddItemWithStringId(IDC_CUT, IDS_CUT);
  system_menu_contents_->AddSeparator();
  if (browser_->is_app()) {
    system_menu_contents_->AddItemWithStringId(IDC_NEW_TAB,
                                               IDS_APP_MENU_NEW_WEB_PAGE);
  } else {
    system_menu_contents_->AddItemWithStringId(IDC_SHOW_AS_TAB,
                                               IDS_SHOW_AS_TAB);
  }
  system_menu_contents_->AddItemWithStringId(IDC_COPY_URL,
                                             IDS_APP_MENU_COPY_URL);
  system_menu_contents_->AddSeparator();
  system_menu_contents_->AddItemWithStringId(IDC_RELOAD, IDS_APP_MENU_RELOAD);
  system_menu_contents_->AddItemWithStringId(IDC_FORWARD,
                                             IDS_CONTENT_CONTEXT_FORWARD);
  system_menu_contents_->AddItemWithStringId(IDC_BACK,
                                             IDS_CONTENT_CONTEXT_BACK);
  AddFrameToggleItems();
}

void BrowserView::AddFrameToggleItems() {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDebugEnableFrameToggle)) {
    system_menu_contents_->AddSeparator();
    system_menu_contents_->AddItem(IDC_DEBUG_FRAME_TOGGLE,
                                   L"Toggle Frame Type");
  }
}
#endif

int BrowserView::GetCommandIDForAppCommandID(int app_command_id) const {
#if defined(OS_WIN) && !defined(USE_AURA)
  switch (app_command_id) {
    // NOTE: The order here matches the APPCOMMAND declaration order in the
    // Windows headers.
    case APPCOMMAND_BROWSER_BACKWARD: return IDC_BACK;
    case APPCOMMAND_BROWSER_FORWARD:  return IDC_FORWARD;
    case APPCOMMAND_BROWSER_REFRESH:  return IDC_RELOAD;
    case APPCOMMAND_BROWSER_HOME:     return IDC_HOME;
    case APPCOMMAND_BROWSER_STOP:     return IDC_STOP;
    case APPCOMMAND_BROWSER_SEARCH:   return IDC_FOCUS_SEARCH;
    case APPCOMMAND_HELP:             return IDC_HELP_PAGE;
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
  if (command_id == IDC_HELP_PAGE && key_code == ui::VKEY_F1)
    UserMetrics::RecordAction(UserMetricsAction("ShowHelpTabViaF1"));

#if defined(OS_CHROMEOS)
  // Collect information about the relative popularity of various accelerators
  // on Chrome OS.
  switch (command_id) {
    case IDC_BACK:
      if (key_code == ui::VKEY_BACK)
        UserMetrics::RecordAction(UserMetricsAction("Accel_Back_Backspace"));
      else if (key_code == ui::VKEY_F1)
        UserMetrics::RecordAction(UserMetricsAction("Accel_Back_F1"));
      else if (key_code == ui::VKEY_LEFT)
        UserMetrics::RecordAction(UserMetricsAction("Accel_Back_Left"));
      break;
    case IDC_FORWARD:
      if (key_code == ui::VKEY_BACK)
        UserMetrics::RecordAction(UserMetricsAction("Accel_Forward_Backspace"));
      else if (key_code == ui::VKEY_F2)
        UserMetrics::RecordAction(UserMetricsAction("Accel_Forward_F2"));
      else if (key_code == ui::VKEY_RIGHT)
        UserMetrics::RecordAction(UserMetricsAction("Accel_Forward_Right"));
      break;
    case IDC_RELOAD:
    case IDC_RELOAD_IGNORING_CACHE:
      if (key_code == ui::VKEY_R)
        UserMetrics::RecordAction(UserMetricsAction("Accel_Reload_R"));
      else if (key_code == ui::VKEY_F3)
        UserMetrics::RecordAction(UserMetricsAction("Accel_Reload_F3"));
      break;
    case IDC_FULLSCREEN:
      if (key_code == ui::VKEY_F4)
        UserMetrics::RecordAction(UserMetricsAction("Accel_Fullscreen_F4"));
      break;
    case IDC_NEW_TAB:
      if (key_code == ui::VKEY_T)
        UserMetrics::RecordAction(UserMetricsAction("Accel_NewTab_T"));
      break;
    case IDC_SEARCH:
      if (key_code == ui::VKEY_LWIN)
        UserMetrics::RecordAction(UserMetricsAction("Accel_Search_LWin"));
      break;
    case IDC_FOCUS_LOCATION:
      if (key_code == ui::VKEY_D)
        UserMetrics::RecordAction(UserMetricsAction("Accel_FocusLocation_D"));
      else if (key_code == ui::VKEY_L)
        UserMetrics::RecordAction(UserMetricsAction("Accel_FocusLocation_L"));
      break;
    case IDC_FOCUS_SEARCH:
      if (key_code == ui::VKEY_E)
        UserMetrics::RecordAction(UserMetricsAction("Accel_FocusSearch_E"));
      else if (key_code == ui::VKEY_K)
        UserMetrics::RecordAction(UserMetricsAction("Accel_FocusSearch_K"));
      break;
    default:
      // Do nothing.
      break;
  }
#endif
}

void BrowserView::ProcessTabSelected(TabContentsWrapper* new_contents) {
  // If |contents_container_| already has the correct TabContents, we can save
  // some work.  This also prevents extra events from being reported by the
  // Visibility API under Windows, as ChangeTabContents will briefly hide
  // the TabContents window.
  DCHECK(new_contents);
  bool change_tab_contents =
      contents_container_->tab_contents() != new_contents->tab_contents();

  // Update various elements that are interested in knowing the current
  // TabContents.

  // When we toggle the NTP floating bookmarks bar and/or the info bar,
  // we don't want any TabContents to be attached, so that we
  // avoid an unnecessary resize and re-layout of a TabContents.
  if (change_tab_contents)
    contents_container_->ChangeTabContents(NULL);
  infobar_container_->ChangeTabContents(new_contents->infobar_tab_helper());
  if (bookmark_bar_view_.get()) {
    bookmark_bar_view_->SetBookmarkBarState(
        browser_->bookmark_bar_state(),
        BookmarkBar::DONT_ANIMATE_STATE_CHANGE);
  }
  UpdateUIForContents(new_contents);
  if (change_tab_contents)
    contents_container_->ChangeTabContents(new_contents->tab_contents());
  UpdateSidebarForContents(new_contents);

  UpdateDevToolsForContents(new_contents);
  // TODO(beng): This should be called automatically by ChangeTabContents, but I
  //             am striving for parity now rather than cleanliness. This is
  //             required to make features like Duplicate Tab, Undo Close Tab,
  //             etc not result in sad tab.
  new_contents->tab_contents()->DidBecomeSelected();
  if (BrowserList::GetLastActive() == browser_ &&
      !browser_->tabstrip_model()->closing_all() && GetWidget()->IsVisible()) {
    // We only restore focus if our window is visible, to avoid invoking blur
    // handlers when we are eventually shown.
    new_contents->view()->RestoreFocus();
  }

  // Update all the UI bits.
  UpdateTitleBar();
  // No need to update Toolbar because it's already updated in
  // browser.cc.
}

gfx::Size BrowserView::GetResizeCornerSize() const {
  return ResizeCorner::GetSize();
}

void BrowserView::SetToolbar(ToolbarView* toolbar) {
  if (toolbar_) {
    RemoveChildView(toolbar_);
    delete toolbar_;
  }
  toolbar_ = toolbar;
  if (toolbar) {
    AddChildView(toolbar_);
    toolbar_->Init();
  }
}

#if !defined(OS_CHROMEOS) || defined(USE_AURA)
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
#endif

void BrowserView::ShowAvatarBubble(TabContents* tab_contents,
                                   const gfx::Rect& rect) {
  gfx::Point origin(rect.origin());
  views::View::ConvertPointToScreen(GetTabContentsContainerView(), &origin);
  gfx::Rect bounds(origin, rect.size());

  // TODO(msw): Set and support views::BubbleBorder::ALIGN_EDGE_TO_ANCHOR_EDGE.
  AvatarMenuBubbleView* bubble = new AvatarMenuBubbleView(this,
      views::BubbleBorder::TOP_RIGHT, bounds, browser_.get());
  views::BubbleDelegateView::CreateBubble(bubble);
  bubble->Show();
}

void BrowserView::ShowAvatarBubbleFromAvatarButton() {
  AvatarMenuButton* button = frame_->GetAvatarMenuButton();
  if (button)
    button->ShowAvatarBubble();
}
