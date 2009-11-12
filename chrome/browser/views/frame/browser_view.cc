// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/frame/browser_view.h"

#if defined(OS_LINUX)
#include <gtk/gtk.h>
#endif

#include "app/drag_drop_types.h"
#include "app/gfx/canvas.h"
#include "app/l10n_util.h"
#include "app/os_exchange_data.h"
#include "app/resource_bundle.h"
#include "base/command_line.h"
#include "base/keyboard_codes.h"
#include "base/time.h"
#include "build/build_config.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/app_modal_dialog_queue.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_theme_provider.h"
#include "chrome/browser/debugger/devtools_window.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/find_bar.h"
#include "chrome/browser/find_bar_controller.h"
#include "chrome/browser/ntp_background_util.h"
#include "chrome/browser/page_info_window.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/view_ids.h"
#include "chrome/browser/views/bookmark_bar_view.h"
#include "chrome/browser/views/browser_bubble.h"
#include "chrome/browser/views/browser_dialogs.h"
#include "chrome/browser/views/chrome_views_delegate.h"
#include "chrome/browser/views/download_shelf_view.h"
#include "chrome/browser/views/extensions/extension_shelf.h"
#include "chrome/browser/views/frame/browser_extender.h"
#include "chrome/browser/views/frame/browser_frame.h"
#include "chrome/browser/views/fullscreen_exit_bubble.h"
#include "chrome/browser/views/infobars/infobar_container.h"
#include "chrome/browser/views/status_bubble_views.h"
#include "chrome/browser/views/tab_contents/tab_contents_container.h"
#include "chrome/browser/views/tabs/browser_tab_strip.h"
#include "chrome/browser/views/tabs/tab_strip.h"
#include "chrome/browser/views/toolbar_star_toggle.h"
#include "chrome/browser/views/toolbar_view.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/browser/window_sizer.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/native_web_keyboard_event.h"
#include "chrome/common/native_window_notification_source.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "grit/app_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/webkit_resources.h"
#include "views/controls/single_split_view.h"
#include "views/fill_layout.h"
#include "views/focus/external_focus_tracker.h"
#include "views/grid_layout.h"
#include "views/view.h"
#include "views/widget/root_view.h"
#include "views/window/dialog_delegate.h"
#include "views/window/non_client_view.h"
#include "views/window/window.h"

#if defined(OS_WIN)
#include "app/win_util.h"
#include "chrome/browser/jumplist.h"
#include "chrome/browser/views/theme_install_bubble_view.h"
#include "views/controls/scrollbar/native_scroll_bar.h"
#elif defined(OS_LINUX)
#include "chrome/browser/views/accelerator_table_gtk.h"
#include "views/window/hit_test.h"
#endif

using base::TimeDelta;
using views::ColumnSet;
using views::GridLayout;

// static
SkBitmap BrowserView::default_favicon_;
SkBitmap BrowserView::otr_avatar_;
// The vertical overlap between the TabStrip and the Toolbar.
static const int kToolbarTabStripVerticalOverlap = 3;
// The visible height of the shadow above the tabs. Clicks in this area are
// treated as clicks to the frame, rather than clicks to the tab.
static const int kTabShadowSize = 2;
// The height of the status bubble.
static const int kStatusBubbleHeight = 20;
// An offset distance between certain toolbars and the toolbar that preceded
// them in layout.
static const int kSeparationLineHeight = 1;
// The name of a key to store on the window handle so that other code can
// locate this object using just the handle.
#if defined(OS_WIN)
static const wchar_t* kBrowserViewKey = L"__BROWSER_VIEW__";
#else
static const char* kBrowserViewKey = "__BROWSER_VIEW__";
#endif
// How frequently we check for hung plugin windows.
static const int kDefaultHungPluginDetectFrequency = 2000;
// How long do we wait before we consider a window hung (in ms).
static const int kDefaultPluginMessageResponseTimeout = 30000;
// The number of milliseconds between loading animation frames.
static const int kLoadingAnimationFrameTimeMs = 30;
// The amount of space we expect the window border to take up.
static const int kWindowBorderWidth = 5;

// If not -1, windows are shown with this state.
static int explicit_show_state = -1;

// How round the 'new tab' style bookmarks bar is.
static const int kNewtabBarRoundness = 5;
// ------------

// Returned from BrowserView::GetClassName.
static const char kBrowserViewClassName[] = "browser/views/BrowserView";

///////////////////////////////////////////////////////////////////////////////
// BookmarkExtensionBackground, private:
// This object serves as the views::Background object which is used to layout
// and paint the bookmark bar.
class BookmarkExtensionBackground : public views::Background {
 public:
  explicit BookmarkExtensionBackground(BrowserView* browser_view,
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
  ThemeProvider* tp = host_view_->GetThemeProvider();
  if (host_view_->IsDetached()) {
    // Draw the background to match the new tab page.
    int height = 0;
    TabContents* contents = browser_->GetSelectedTabContents();
    if (contents && contents->view())
      height = contents->view()->GetContainerSize().height();
    NtpBackgroundUtil::PaintBackgroundDetachedMode(
        host_view_->GetThemeProvider(), canvas,
        gfx::Rect(0, 0, host_view_->width(), host_view_->height()),
        height);

    SkRect rect;

    // As 'hidden' according to the animation is the full in-tab state,
    // we invert the value - when current_state is at '0', we expect the
    // bar to be docked.
    double current_state = 1 - host_view_->GetAnimationValue();

    double h_padding = static_cast<double>
        (BookmarkBarView::kNewtabHorizontalPadding) * current_state;
    double v_padding = static_cast<double>
        (BookmarkBarView::kNewtabVerticalPadding) * current_state;
    double roundness = 0;

    DetachableToolbarView::CalculateContentArea(current_state,
                                                h_padding, v_padding,
                                                &rect, &roundness, host_view_);
    DetachableToolbarView::PaintContentAreaBackground(
        canvas, tp, rect, roundness);
    DetachableToolbarView::PaintContentAreaBorder(canvas, tp, rect, roundness);
    DetachableToolbarView::PaintHorizontalBorder(canvas, host_view_);
  } else {
    DetachableToolbarView::PaintBackgroundAttachedMode(canvas, host_view_);
    DetachableToolbarView::PaintHorizontalBorder(canvas, host_view_);
  }
}

///////////////////////////////////////////////////////////////////////////////
// ResizeCorner, private:

class ResizeCorner : public views::View {
 public:
  ResizeCorner() { }

  virtual void Paint(gfx::Canvas* canvas) {
    views::Window* window = GetWindow();
    if (!window || (window->IsMaximized() || window->IsFullscreen()))
      return;

    SkBitmap* bitmap = ResourceBundle::GetSharedInstance().GetBitmapNamed(
        IDR_TEXTAREA_RESIZER);
    bitmap->buildMipMap(false);
    bool rtl_dir = (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT);
    if (rtl_dir) {
      canvas->TranslateInt(width(), 0);
      canvas->ScaleInt(-1, 1);
      canvas->save();
    }
    canvas->DrawBitmapInt(*bitmap, width() - bitmap->width(),
                          height() - bitmap->height());
    if (rtl_dir)
      canvas->restore();
  }

  static gfx::Size GetSize() {
    // This is disabled until we find what makes us slower when we let
    // WebKit know that we have a resizer rect...
    // return gfx::Size(views::NativeScrollBar::GetVerticalScrollBarWidth(),
    //     views::NativeScrollBar::GetHorizontalScrollBarHeight());
    return gfx::Size();
  }

  virtual gfx::Size GetPreferredSize() {
    views::Window* window = GetWindow();
    return (!window || window->IsMaximized() || window->IsFullscreen()) ?
        gfx::Size() : GetSize();
  }

  virtual void Layout() {
    views::View* parent_view = GetParent();
    if (parent_view) {
      gfx::Size ps = GetPreferredSize();
      // No need to handle Right to left text direction here,
      // our parent must take care of it for us...
      SetBounds(parent_view->width() - ps.width(),
                parent_view->height() - ps.height(), ps.width(), ps.height());
    }
  }

 private:
  // Returns the WindowWin we're displayed in. Returns NULL if we're not
  // currently in a window.
  views::Window* GetWindow() {
    views::Widget* widget = GetWidget();
    return widget ? widget->GetWindow() : NULL;
  }

  DISALLOW_COPY_AND_ASSIGN(ResizeCorner);
};

////////////////////////////////////////////////////////////////////////////////
// DownloadInProgressConfirmDialogDelegate

class DownloadInProgressConfirmDialogDelegate : public views::DialogDelegate,
                                                public views::View {
 public:
  explicit DownloadInProgressConfirmDialogDelegate(Browser* browser)
      : browser_(browser),
        product_name_(l10n_util::GetString(IDS_PRODUCT_NAME)) {
    int download_count = browser->profile()->GetDownloadManager()->
        in_progress_count();

    std::wstring warning_text;
    std::wstring explanation_text;
    if (download_count == 1) {
      warning_text =
          l10n_util::GetStringF(IDS_SINGLE_DOWNLOAD_REMOVE_CONFIRM_WARNING,
                                product_name_);
      explanation_text =
          l10n_util::GetStringF(IDS_SINGLE_DOWNLOAD_REMOVE_CONFIRM_EXPLANATION,
                                product_name_);
      ok_button_text_ = l10n_util::GetString(
          IDS_SINGLE_DOWNLOAD_REMOVE_CONFIRM_OK_BUTTON_LABEL);
      cancel_button_text_ = l10n_util::GetString(
          IDS_SINGLE_DOWNLOAD_REMOVE_CONFIRM_CANCEL_BUTTON_LABEL);
    } else {
      warning_text =
          l10n_util::GetStringF(IDS_MULTIPLE_DOWNLOADS_REMOVE_CONFIRM_WARNING,
                                product_name_, IntToWString(download_count));
      explanation_text =
          l10n_util::GetStringF(
              IDS_MULTIPLE_DOWNLOADS_REMOVE_CONFIRM_EXPLANATION, product_name_);
      ok_button_text_ = l10n_util::GetString(
          IDS_MULTIPLE_DOWNLOADS_REMOVE_CONFIRM_OK_BUTTON_LABEL);
      cancel_button_text_ = l10n_util::GetString(
          IDS_MULTIPLE_DOWNLOADS_REMOVE_CONFIRM_CANCEL_BUTTON_LABEL);
    }

    // There are two lines of text: the bold warning label and the text
    // explanation label.
    GridLayout* layout = new GridLayout(this);
    SetLayoutManager(layout);
    const int columnset_id = 0;
    ColumnSet* column_set = layout->AddColumnSet(columnset_id);
    column_set->AddColumn(GridLayout::FILL, GridLayout::LEADING, 1,
                          GridLayout::USE_PREF, 0, 0);

    gfx::Font bold_font =
        ResourceBundle::GetSharedInstance().GetFont(
            ResourceBundle::BaseFont).DeriveFont(0, gfx::Font::BOLD);
    warning_ = new views::Label(warning_text, bold_font);
    warning_->SetMultiLine(true);
    warning_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    warning_->set_border(views::Border::CreateEmptyBorder(10, 10, 10, 10));
    layout->StartRow(0, columnset_id);
    layout->AddView(warning_);

    explanation_ = new views::Label(explanation_text);
    explanation_->SetMultiLine(true);
    explanation_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    explanation_->set_border(views::Border::CreateEmptyBorder(10, 10, 10, 10));
    layout->StartRow(0, columnset_id);
    layout->AddView(explanation_);
  }

  ~DownloadInProgressConfirmDialogDelegate() {
  }

  // View implementation:
  virtual gfx::Size GetPreferredSize() {
    const int kContentWidth = 400;
    const int height = warning_->GetHeightForWidth(kContentWidth) +
                       explanation_->GetHeightForWidth(kContentWidth);
    return gfx::Size(kContentWidth, height);
  }

  // DialogDelegate implementation:
  virtual int GetDefaultDialogButton() const {
    return MessageBoxFlags::DIALOGBUTTON_CANCEL;
  }

  virtual std::wstring GetDialogButtonLabel(
      MessageBoxFlags::DialogButton button) const {
    if (button == MessageBoxFlags::DIALOGBUTTON_OK)
      return ok_button_text_;

    DCHECK_EQ(MessageBoxFlags::DIALOGBUTTON_CANCEL, button);
    return cancel_button_text_;
  }

  virtual bool Accept() {
    browser_->InProgressDownloadResponse(true);
    return true;
  }

  virtual bool Cancel() {
    browser_->InProgressDownloadResponse(false);
    return true;
  }

  // WindowDelegate implementation:
  virtual bool IsModal() const { return true; }

  virtual views::View* GetContentsView() {
    return this;
  }

  virtual std::wstring GetWindowTitle() const {
    return product_name_;
  }

 private:
  Browser* browser_;
  views::Label* warning_;
  views::Label* explanation_;

  std::wstring ok_button_text_;
  std::wstring cancel_button_text_;

  std::wstring product_name_;

  DISALLOW_COPY_AND_ASSIGN(DownloadInProgressConfirmDialogDelegate);
};

///////////////////////////////////////////////////////////////////////////////
// BrowserView, public:

// static
void BrowserView::SetShowState(int state) {
  explicit_show_state = state;
}

BrowserView::BrowserView(Browser* browser)
    : views::ClientView(NULL, NULL),
      frame_(NULL),
      browser_(browser),
      active_bookmark_bar_(NULL),
      tabstrip_(NULL),
      toolbar_(NULL),
      infobar_container_(NULL),
      find_bar_y_(0),
      contents_container_(NULL),
      devtools_container_(NULL),
      contents_split_(NULL),
      initialized_(false),
      ignore_layout_(false),
#if defined(OS_WIN)
      hung_window_detector_(&hung_plugin_action_),
      ticker_(0),
#endif
      extension_shelf_(NULL) {
  InitClass();
  browser_->tabstrip_model()->AddObserver(this);
}

BrowserView::~BrowserView() {
  browser_->tabstrip_model()->RemoveObserver(this);

#if defined(OS_WIN)
  // Stop hung plugin monitoring.
  ticker_.Stop();
  ticker_.UnregisterTickHandler(&hung_window_detector_);
#endif

  // We destroy the download shelf before |browser_| to remove its child
  // download views from the set of download observers (since the observed
  // downloads can be destroyed along with |browser_| and the observer
  // notifications will call back into deleted objects).
  download_shelf_.reset();

  // Explicitly set browser_ to NULL.
  browser_.reset();
}

// static
BrowserView* BrowserView::GetBrowserViewForNativeWindow(
    gfx::NativeWindow window) {
#if defined(OS_WIN)
  if (IsWindow(window)) {
    HANDLE data = GetProp(window, kBrowserViewKey);
    if (data)
      return reinterpret_cast<BrowserView*>(data);
  }
#else
  if (window) {
    return static_cast<BrowserView*>(
        g_object_get_data(G_OBJECT(window), kBrowserViewKey));
  }
#endif
  return NULL;
}

int BrowserView::GetShowState() const {
  if (explicit_show_state != -1)
    return explicit_show_state;

#if defined(OS_WIN)
  STARTUPINFO si = {0};
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESHOWWINDOW;
  GetStartupInfo(&si);
  return si.wShowWindow;
#else
  NOTIMPLEMENTED();
  return 0;
#endif
}

void BrowserView::WindowMoved() {
  // Cancel any tabstrip animations, some of them may be invalidated by the
  // window being repositioned.
  // Comment out for one cycle to see if this fixes dist tests.
  // tabstrip_->DestroyDragController();

  status_bubble_->Reposition();

  // Do safe iteration in case the bubble winds up closing as a result of this
  // message.
  for (BubbleSet::iterator i = browser_bubbles_.begin();
       i != browser_bubbles_.end();) {
    BubbleSet::iterator bubble = i++;
    (*bubble)->BrowserWindowMoved();
  }

  browser::HideBookmarkBubbleView();

  // Close the omnibox popup, if any.
  if (toolbar_->location_bar())
    toolbar_->location_bar()->location_entry()->ClosePopup();
}

void BrowserView::WindowMoveOrResizeStarted() {
  TabContents* tab_contents = GetSelectedTabContents();
  if (tab_contents)
    tab_contents->WindowMoveOrResizeStarted();
}

gfx::Rect BrowserView::GetToolbarBounds() const {
  return toolbar_->bounds();
}

gfx::Rect BrowserView::GetClientAreaBounds() const {
  gfx::Rect container_bounds = contents_container_->bounds();
  gfx::Point container_origin = container_bounds.origin();
  ConvertPointToView(this, GetParent(), &container_origin);
  container_bounds.set_origin(container_origin);
  return container_bounds;
}

bool BrowserView::ShouldFindBarBlendWithBookmarksBar() const {
  if (bookmark_bar_view_.get())
    return bookmark_bar_view_->IsAlwaysShown();
  return false;
}

gfx::Rect BrowserView::GetFindBarBoundingBox() const {
  // This function returns the area the Find Bar can be laid out within. This
  // basically implies the "user-perceived content area" of the browser window
  // excluding the vertical scrollbar. This is not quite so straightforward as
  // positioning based on the TabContentsContainer since the BookmarkBarView
  // may be visible but not persistent (in the New Tab case) and we position
  // the Find Bar over the top of it in that case since the BookmarkBarView is
  // not _visually_ connected to the Toolbar.

  // First determine the bounding box of the content area in Widget coordinates.
  gfx::Rect bounding_box(contents_container_->bounds());

  gfx::Point topleft;
  views::View::ConvertPointToWidget(contents_container_, &topleft);
  bounding_box.set_origin(topleft);

  // Adjust the position and size of the bounding box by the find bar offset
  // calculated during the last Layout.
  int height_delta = find_bar_y_ - bounding_box.y();
  bounding_box.set_y(find_bar_y_);
  bounding_box.set_height(std::max(0, bounding_box.height() + height_delta));

  // Finally decrease the width of the bounding box by the width of the vertical
  // scroll bar.
#if defined(OS_WIN)
  int scrollbar_width = views::NativeScrollBar::GetVerticalScrollBarWidth();
#else
  // This matches the value in ScrollbarThemeChromium::scrollbarThickness.
  int scrollbar_width = 15;
#endif
  bounding_box.set_width(std::max(0, bounding_box.width() - scrollbar_width));
  if (UILayoutIsRightToLeft())
    bounding_box.set_x(bounding_box.x() + scrollbar_width);

  return bounding_box;
}

int BrowserView::GetTabStripHeight() const {
  // We want to return tabstrip_->height(), but we might be called in the midst
  // of layout, when that hasn't yet been updated to reflect the current state.
  // So return what the tabstrip height _ought_ to be right now.
  return IsTabStripVisible() ? tabstrip_->GetView()->GetPreferredSize().height()
                             : 0;
}

gfx::Rect BrowserView::GetTabStripBounds() const {
  return frame_->GetBoundsForTabStrip(tabstrip_);
}

bool BrowserView::IsTabStripVisible() const {
  return browser_->SupportsWindowFeature(Browser::FEATURE_TABSTRIP);
}

bool BrowserView::IsOffTheRecord() const {
  return browser_->profile()->IsOffTheRecord();
}

bool BrowserView::ShouldShowOffTheRecordAvatar() const {
  return IsOffTheRecord() && IsBrowserTypeNormal();
}

bool BrowserView::AcceleratorPressed(const views::Accelerator& accelerator) {
  std::map<views::Accelerator, int>::const_iterator iter =
      accelerator_table_.find(accelerator);
  DCHECK(iter != accelerator_table_.end());

  int command_id = iter->second;
  if (browser_->command_updater()->SupportsCommand(command_id) &&
      browser_->command_updater()->IsCommandEnabled(command_id)) {
    browser_->ExecuteCommand(command_id);
    return true;
  }
  return false;
}

bool BrowserView::GetAccelerator(int cmd_id, views::Accelerator* accelerator) {
  // The standard Ctrl-X, Ctrl-V and Ctrl-C are not defined as accelerators
  // anywhere so we need to check for them explicitly here.
  switch (cmd_id) {
    case IDC_CUT:
      *accelerator = views::Accelerator(base::VKEY_X, false, true, false);
      return true;
    case IDC_COPY:
      *accelerator = views::Accelerator(base::VKEY_C, false, true, false);
      return true;
    case IDC_PASTE:
      *accelerator = views::Accelerator(base::VKEY_V, false, true, false);
      return true;
  }
  // Else, we retrieve the accelerator information from the accelerator table.
  std::map<views::Accelerator, int>::iterator it =
      accelerator_table_.begin();
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
  if (Singleton<AppModalDialogQueue>()->HasActiveDialog()) {
    Browser* active_browser = BrowserList::GetLastActive();
    if (active_browser && (browser_ != active_browser)) {
      active_browser->window()->FlashFrame();
      active_browser->window()->Activate();
    }
    Singleton<AppModalDialogQueue>()->ActivateModalDialog();
    return true;
  }
  return false;
}

void BrowserView::ActivationChanged(bool activated) {
  if (activated)
    BrowserList::SetLastActive(browser_.get());
  browser_extender_->ActivationChanged();
}

TabContents* BrowserView::GetSelectedTabContents() const {
  return browser_->GetSelectedTabContents();
}

SkBitmap BrowserView::GetOTRAvatarIcon() {
  if (otr_avatar_.isNull()) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    otr_avatar_ = *rb.GetBitmapNamed(IDR_OTR_ICON);
  }
  return otr_avatar_;
}

#if defined(OS_WIN)
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

void BrowserView::AttachBrowserBubble(BrowserBubble* bubble) {
  browser_bubbles_.insert(bubble);
}

void BrowserView::DetachBrowserBubble(BrowserBubble* bubble) {
  BubbleSet::iterator it = browser_bubbles_.find(bubble);
  if (it != browser_bubbles_.end())
    browser_bubbles_.erase(it);
}

bool BrowserView::IsPositionInWindowCaption(const gfx::Point& point) {
  gfx::Point tabstrip_point(point);
  View::ConvertPointToView(this, tabstrip()->GetView(), &tabstrip_point);
  return tabstrip()->IsPositionInWindowCaption(tabstrip_point)
      && !browser_extender_->NonClientHitTest(point);
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, BrowserWindow implementation:

void BrowserView::Show() {
  // If the window is already visible, just activate it.
  if (frame_->GetWindow()->IsVisible()) {
    frame_->GetWindow()->Activate();
    return;
  }

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
  TabContents* selected_tab_contents = GetSelectedTabContents();
  if (selected_tab_contents)
    selected_tab_contents->view()->RestoreFocus();

  // Restore split offset.
  int split_offset = g_browser_process->local_state()->GetInteger(
      prefs::kDevToolsSplitLocation);
  if (split_offset == -1) {
    // Initial load, set to default value.
    split_offset = 2 * contents_split_->height() / 3;
  }
  // Make sure user can see both panes.
  int min_split_size = contents_split_->height() / 10;
  split_offset = std::min(contents_split_->height() - min_split_size,
                          std::max(min_split_size, split_offset));
  contents_split_->set_divider_offset(split_offset);

  frame_->GetWindow()->Show();

  browser_extender_->Show();
}

void BrowserView::SetBounds(const gfx::Rect& bounds) {
  GetWidget()->SetBounds(bounds);
}

void BrowserView::Close() {
  // BrowserWindowClosing will usually cause the bubble to remove itself from
  // the set, so we need to iterate in a way that's safe against deletion.
  for (BubbleSet::iterator i = browser_bubbles_.begin();
       i != browser_bubbles_.end();) {
    BubbleSet::iterator bubble = i++;
    (*bubble)->BrowserWindowClosing();
  }

  frame_->GetWindow()->Close();

  browser_extender_->Close();
}

void BrowserView::Activate() {
  frame_->GetWindow()->Activate();
}

bool BrowserView::IsActive() const {
  return frame_->GetWindow()->IsActive();
}

void BrowserView::FlashFrame() {
#if defined(OS_WIN)
  FLASHWINFO fwi;
  fwi.cbSize = sizeof(fwi);
  fwi.hwnd = frame_->GetWindow()->GetNativeWindow();
  fwi.dwFlags = FLASHW_ALL;
  fwi.uCount = 4;
  fwi.dwTimeout = 0;
  FlashWindowEx(&fwi);
#else
  // Doesn't matter for chrome os.
#endif
}

gfx::NativeWindow BrowserView::GetNativeHandle() {
  return GetWidget()->GetWindow()->GetNativeWindow();
}

BrowserWindowTesting* BrowserView::GetBrowserWindowTesting() {
  return this;
}

StatusBubble* BrowserView::GetStatusBubble() {
  return status_bubble_.get();
}

void BrowserView::SelectedTabToolbarSizeChanged(bool is_animating) {
  if (is_animating) {
    contents_container_->SetFastResize(true);
    UpdateUIForContents(browser_->GetSelectedTabContents());
    contents_container_->SetFastResize(false);
  } else {
    UpdateUIForContents(browser_->GetSelectedTabContents());
    contents_split_->Layout();
  }
}

void BrowserView::SelectedTabExtensionShelfSizeChanged() {
  Layout();
}

void BrowserView::UpdateTitleBar() {
  frame_->GetWindow()->UpdateWindowTitle();
  if (ShouldShowWindowIcon())
    frame_->GetWindow()->UpdateWindowIcon();
  browser_extender_->UpdateTitleBar();
}

void BrowserView::ShelfVisibilityChanged() {
  Layout();
}

void BrowserView::UpdateDevTools() {
  UpdateDevToolsForContents(GetSelectedTabContents());
  Layout();
}

void BrowserView::FocusDevTools() {
  if (devtools_container_->IsVisible())
    GetRootView()->FocusView(devtools_container_->GetFocusView());
}

void BrowserView::UpdateLoadingAnimations(bool should_animate) {
  if (should_animate) {
    if (!loading_animation_timer_.IsRunning()) {
      // Loads are happening, and the timer isn't running, so start it.
      loading_animation_timer_.Start(
          TimeDelta::FromMilliseconds(kLoadingAnimationFrameTimeMs), this,
          &BrowserView::LoadingAnimationCallback);
    }
  } else {
    if (loading_animation_timer_.IsRunning()) {
      loading_animation_timer_.Stop();
      // Loads are now complete, update the state if a task was scheduled.
      LoadingAnimationCallback();
    }
  }
}

void BrowserView::SetStarredState(bool is_starred) {
  toolbar_->star_button()->SetToggled(is_starred);
}

gfx::Rect BrowserView::GetRestoredBounds() const {
  return frame_->GetWindow()->GetNormalBounds();
}

bool BrowserView::IsMaximized() const {
  return frame_->GetWindow()->IsMaximized();
}

void BrowserView::SetFullscreen(bool fullscreen) {
  if (IsFullscreen() == fullscreen)
    return;  // Nothing to do.

  // Reduce jankiness during the following position changes by:
  //   * Hiding the window until it's in the final position
  //   * Ignoring all intervening Layout() calls, which resize the webpage and
  //     thus are slow and look ugly
  ignore_layout_ = true;
  LocationBarView* location_bar = toolbar_->location_bar();
#if defined(OS_WIN)
  AutocompleteEditViewWin* edit_view =
      static_cast<AutocompleteEditViewWin*>(location_bar->location_entry());
#endif
  if (IsFullscreen()) {
    // Hide the fullscreen bubble as soon as possible, since the mode toggle can
    // take enough time for the user to notice.
    fullscreen_bubble_.reset();
  } else {
    // Move focus out of the location bar if necessary.
    views::FocusManager* focus_manager = GetFocusManager();
    DCHECK(focus_manager);
    if (focus_manager->GetFocusedView() == location_bar)
      focus_manager->ClearFocus();

#if defined(OS_WIN)
    // If we don't hide the edit and force it to not show until we come out of
    // fullscreen, then if the user was on the New Tab Page, the edit contents
    // will appear atop the web contents once we go into fullscreen mode.  This
    // has something to do with how we move the main window while it's hidden;
    // if we don't hide the main window below, we don't get this problem.
    edit_view->set_force_hidden(true);
    ShowWindow(edit_view->m_hWnd, SW_HIDE);
#endif
  }
#if defined(OS_WIN)
  frame_->GetWindow()->PushForceHidden();
#endif

  // Notify bookmark bar, so it can set itself to the appropriate drawing state.
  if (bookmark_bar_view_.get())
    bookmark_bar_view_->OnFullscreenToggled(fullscreen);

  // Notify extension shelf, so it can set itself to the appropriate drawing
  // state.
  if (extension_shelf_)
    extension_shelf_->OnFullscreenToggled(fullscreen);

  // Toggle fullscreen mode.
  frame_->GetWindow()->SetFullscreen(fullscreen);

  if (fullscreen) {
#if !defined(OS_MACOSX)
    bool is_kosk =
        CommandLine::ForCurrentProcess()->HasSwitch(switches::kKioskMode);
#else
    bool is_kiosk = false;
#endif
    if (!is_kosk) {
      fullscreen_bubble_.reset(new FullscreenExitBubble(GetWidget(),
                                                        browser_.get()));
    }
  } else {
#if defined(OS_WIN)
    // Show the edit again since we're no longer in fullscreen mode.
    edit_view->set_force_hidden(false);
    ShowWindow(edit_view->m_hWnd, SW_SHOW);
#endif
  }

  // Undo our anti-jankiness hacks and force the window to relayout now that
  // it's in its final position.
  ignore_layout_ = false;
  Layout();
#if defined(OS_WIN)
  frame_->GetWindow()->PopForceHidden();
#endif
}

bool BrowserView::IsFullscreen() const {
  return frame_->GetWindow()->IsFullscreen();
}

bool BrowserView::IsFullscreenBubbleVisible() const {
  return fullscreen_bubble_.get() ? true : false;
}

LocationBar* BrowserView::GetLocationBar() const {
  return toolbar_->location_bar();
}

void BrowserView::SetFocusToLocationBar() {
  LocationBarView* location_bar = toolbar_->location_bar();
  if (browser_extender_->SetFocusToCompactNavigationBar()) {
    // Compact navigation bar got focus.
  } else if (location_bar->IsFocusable()) {
    // Location bar got focus.
    location_bar->FocusLocation();
  } else {
    // If none of location bar/compact navigation bar got focus,
    // then clear focus.
    views::FocusManager* focus_manager = GetFocusManager();
    DCHECK(focus_manager);
    focus_manager->ClearFocus();
  }
}

void BrowserView::UpdateStopGoState(bool is_loading, bool force) {
  toolbar_->go_button()->ChangeMode(
      is_loading ? GoButton::MODE_STOP : GoButton::MODE_GO, force);
}

void BrowserView::UpdateToolbar(TabContents* contents,
                                bool should_restore_state) {
  toolbar_->Update(contents, should_restore_state);
}

void BrowserView::FocusToolbar() {
  toolbar_->InitializeTraversal();
}

void BrowserView::DestroyBrowser() {
  // Explicitly delete the BookmarkBarView now. That way we don't have to
  // worry about the BookmarkBarView potentially outliving the Browser &
  // Profile.
  bookmark_bar_view_.reset();
  browser_.reset();
}

bool BrowserView::IsBookmarkBarVisible() const {
  return browser_->SupportsWindowFeature(Browser::FEATURE_BOOKMARKBAR) &&
      active_bookmark_bar_ &&
      (active_bookmark_bar_->GetPreferredSize().height() != 0);
}

bool BrowserView::IsToolbarVisible() const {
  if (browser_extender_->ShouldForceHideToolbar())
    return false;
  return browser_->SupportsWindowFeature(Browser::FEATURE_TOOLBAR) ||
         browser_->SupportsWindowFeature(Browser::FEATURE_LOCATIONBAR);
}

gfx::Rect BrowserView::GetRootWindowResizerRect() const {
  if (frame_->GetWindow()->IsMaximized() || frame_->GetWindow()->IsFullscreen())
    return gfx::Rect();

  // We don't specify a resize corner size if we have a bottom shelf either.
  // This is because we take care of drawing the resize corner on top of that
  // shelf, so we don't want others to do it for us in this case.
  // Currently, the only visible bottom shelf is the download shelf.
  // Other tests should be added here if we add more bottom shelves.
  if (download_shelf_.get() && download_shelf_->IsShowing()) {
    return gfx::Rect();
  }

  gfx::Rect client_rect = contents_split_->bounds();
  gfx::Size resize_corner_size = ResizeCorner::GetSize();
  int x = client_rect.width() - resize_corner_size.width();
  if (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT)
    x = 0;
  return gfx::Rect(x, client_rect.height() - resize_corner_size.height(),
                   resize_corner_size.width(), resize_corner_size.height());
}

void BrowserView::DisableInactiveFrame() {
#if defined(OS_WIN)
  frame_->GetWindow()->DisableInactiveRendering();
#endif  // No tricks are needed to get the right behavior on Linux.
}

void BrowserView::ConfirmAddSearchProvider(const TemplateURL* template_url,
                                           Profile* profile) {
  browser::EditSearchEngine(GetWindow()->GetNativeWindow(), template_url, NULL,
                            profile);
}

void BrowserView::ToggleBookmarkBar() {
  bookmark_utils::ToggleWhenVisible(browser_->profile());
}

void BrowserView::ToggleExtensionShelf() {
  ExtensionShelf::ToggleWhenExtensionShelfVisible(browser_->profile());
}

void BrowserView::ShowAboutChromeDialog() {
  browser::ShowAboutChromeView(GetWindow()->GetNativeWindow(),
                               browser_->profile());
}

void BrowserView::ShowTaskManager() {
  browser::ShowTaskManager();
}

void BrowserView::ShowBookmarkManager() {
  browser::ShowBookmarkManagerView(browser_->profile());
}

void BrowserView::ShowBookmarkBubble(const GURL& url, bool already_bookmarked) {
  toolbar_->star_button()->ShowStarBubble(url, !already_bookmarked);
}

void BrowserView::SetDownloadShelfVisible(bool visible) {
  // This can be called from the superclass destructor, when it destroys our
  // child views. At that point, browser_ is already gone.
  if (browser_ == NULL)
    return;

  if (visible && IsDownloadShelfVisible() != visible)
    // Invoke GetDownloadShelf to force the shelf to be created.
    GetDownloadShelf();

  if (browser_ != NULL)
    browser_->UpdateDownloadShelfVisibility(visible);

  // SetDownloadShelfVisible can force-close the shelf, so make sure we lay out
  // everything correctly, as if the animation had finished. This doesn't
  // matter for showing the shelf, as the show animation will do it.
  SelectedTabToolbarSizeChanged(false);
}

bool BrowserView::IsDownloadShelfVisible() const {
  return download_shelf_.get() && download_shelf_->IsShowing();
}

DownloadShelf* BrowserView::GetDownloadShelf() {
  if (!download_shelf_.get()) {
    download_shelf_.reset(new DownloadShelfView(browser_.get(), this));
    download_shelf_->set_parent_owned(false);
  }
  return download_shelf_.get();
}

void BrowserView::ShowReportBugDialog() {
  // Retrieve the URL for the current tab (if any) and tell the BugReportView
  TabContents* current_tab = browser_->GetSelectedTabContents();
  if (!current_tab)
    return;
  browser::ShowBugReportView(GetWidget(), browser_->profile(), current_tab);
}

void BrowserView::ShowClearBrowsingDataDialog() {
  browser::ShowClearBrowsingDataView(GetWindow()->GetNativeWindow(),
                                     browser_->profile());
}

void BrowserView::ShowImportDialog() {
  browser::ShowImporterView(GetWidget(), browser_->profile());
}

void BrowserView::ShowSearchEnginesDialog() {
  browser::ShowKeywordEditorView(browser_->profile());
}

void BrowserView::ShowPasswordManager() {
  browser::ShowPasswordsExceptionsWindowView(browser_->profile());
}

void BrowserView::ShowSelectProfileDialog() {
  browser::ShowSelectProfileDialog();
}

void BrowserView::ShowNewProfileDialog() {
  browser::ShowNewProfileDialog();
}

void BrowserView::ShowRepostFormWarningDialog(TabContents* tab_contents) {
  browser::ShowRepostFormWarningDialog(GetNativeHandle(), tab_contents);
}

void BrowserView::ShowProfileErrorDialog(int message_id) {
#if defined(OS_WIN)
  std::wstring title = l10n_util::GetString(IDS_PRODUCT_NAME);
  std::wstring message = l10n_util::GetString(message_id);
  win_util::MessageBox(GetNativeHandle(), message, title,
                       MB_OK | MB_ICONWARNING | MB_TOPMOST);
#elif defined(OS_LINUX)
  std::string title = l10n_util::GetStringUTF8(IDS_PRODUCT_NAME);
  std::string message = l10n_util::GetStringUTF8(message_id);
  GtkWidget* dialog = gtk_message_dialog_new(GetNativeHandle(),
      static_cast<GtkDialogFlags>(0), GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
      "%s", message.c_str());
  gtk_window_set_title(GTK_WINDOW(dialog), title.c_str());
  g_signal_connect(dialog, "response", G_CALLBACK(gtk_widget_destroy), NULL);
  gtk_widget_show_all(dialog);
#else
  NOTIMPLEMENTED();
#endif
}

void BrowserView::ShowThemeInstallBubble() {
#if defined(OS_WIN)
  TabContents* tab_contents = browser_->GetSelectedTabContents();
  if (!tab_contents)
    return;
  ThemeInstallBubbleView::Show(tab_contents);
#elif defined(OS_LINUX)
  // Alas, the Views version of ThemeInstallBubbleView is Windows Views only.
  // http://crbug.com/24360
  NOTIMPLEMENTED();
#else
  NOTIMPLEMENTED();
#endif
}

void BrowserView::ConfirmBrowserCloseWithPendingDownloads() {
  DownloadInProgressConfirmDialogDelegate* delegate =
      new DownloadInProgressConfirmDialogDelegate(browser_.get());
  views::Window::CreateChromeWindow(GetNativeHandle(), gfx::Rect(),
                                    delegate)->Show();
}

void BrowserView::ShowHTMLDialog(HtmlDialogUIDelegate* delegate,
                                 gfx::NativeWindow parent_window) {
  // Default to using our window as the parent if the argument is not specified.
  gfx::NativeWindow parent = parent_window ? parent_window
                                           : GetNativeHandle();
  browser::ShowHtmlDialogView(parent, browser_.get(), delegate);
}

void BrowserView::ContinueDraggingDetachedTab(const gfx::Rect& tab_bounds) {
  tabstrip_->SetDraggedTabBounds(0, tab_bounds);
  frame_->ContinueDraggingDetachedTab();
}

void BrowserView::UserChangedTheme() {
  frame_->GetWindow()->FrameTypeChanged();
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
  browser::ShowPageInfo(GetWindow()->GetNativeWindow(), profile, url, ssl,
                        show_history);
}

void BrowserView::ShowPageMenu() {
  toolbar_->page_menu()->Activate();
}

void BrowserView::ShowAppMenu() {
  toolbar_->app_menu()->Activate();
}

int BrowserView::GetCommandId(const NativeWebKeyboardEvent& event) {
  views::Accelerator accelerator(
      static_cast<base::KeyboardCode>(event.windowsKeyCode),
      (event.modifiers & NativeWebKeyboardEvent::ShiftKey) ==
          NativeWebKeyboardEvent::ShiftKey,
      (event.modifiers & NativeWebKeyboardEvent::ControlKey) ==
          NativeWebKeyboardEvent::ControlKey,
      (event.modifiers & NativeWebKeyboardEvent::AltKey) ==
          NativeWebKeyboardEvent::AltKey);

  std::map<views::Accelerator, int>::const_iterator iter =
      accelerator_table_.find(accelerator);
  if (iter == accelerator_table_.end())
    return -1;

  return iter->second;
}

#if defined(TOOLKIT_VIEWS)
void BrowserView::ToggleCompactNavigationBar() {
  browser_extender_->ToggleCompactNavigationBar();
  Layout();
}
#endif

///////////////////////////////////////////////////////////////////////////////
// BrowserView, BrowserWindowTesting implementation:

BookmarkBarView* BrowserView::GetBookmarkBarView() const {
  return bookmark_bar_view_.get();
}

LocationBarView* BrowserView::GetLocationBarView() const {
  return toolbar_->location_bar();
}

views::View* BrowserView::GetTabContentsContainerView() const {
  return contents_container_->GetFocusView();
}

ToolbarView* BrowserView::GetToolbarView() const {
  return toolbar_;
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, NotificationObserver implementation:

void BrowserView::Observe(NotificationType type,
                          const NotificationSource& source,
                          const NotificationDetails& details) {
  if (type == NotificationType::PREF_CHANGED &&
      *Details<std::wstring>(details).ptr() == prefs::kShowBookmarkBar) {
    if (MaybeShowBookmarkBar(browser_->GetSelectedTabContents()))
      Layout();
  } else {
    NOTREACHED() << "Got a notification we didn't register for!";
  }
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, TabStripModelObserver implementation:

void BrowserView::TabDetachedAt(TabContents* contents, int index) {
  // We use index here rather than comparing |contents| because by this time
  // the model has already removed |contents| from its list, so
  // browser_->GetSelectedTabContents() will return NULL or something else.
  if (index == browser_->tabstrip_model()->selected_index()) {
    // We need to reset the current tab contents to NULL before it gets
    // freed. This is because the focus manager performs some operations
    // on the selected TabContents when it is removed.
    contents_container_->ChangeTabContents(NULL);
    infobar_container_->ChangeTabContents(NULL);
    UpdateDevToolsForContents(NULL);
  }
}

void BrowserView::TabDeselectedAt(TabContents* contents, int index) {
  // We do not store the focus when closing the tab to work-around bug 4633.
  // Some reports seem to show that the focus manager and/or focused view can
  // be garbage at that point, it is not clear why.
  if (!contents->is_being_destroyed())
    contents->view()->StoreFocus();
}

void BrowserView::TabSelectedAt(TabContents* old_contents,
                                TabContents* new_contents,
                                int index,
                                bool user_gesture) {
  DCHECK(old_contents != new_contents);

  // Update various elements that are interested in knowing the current
  // TabContents.

  // When we toggle the NTP floating bookmarks bar,
  // we don't want any TabContents to be attached, so that we
  // avoid an unnecessary resize and re-layout of a TabContents.
  contents_container_->ChangeTabContents(NULL);

  infobar_container_->ChangeTabContents(new_contents);

  // Update all the UI bits.
  UpdateTitleBar();
  toolbar_->SetProfile(new_contents->profile());
  UpdateToolbar(new_contents, true);
  UpdateUIForContents(new_contents);

  contents_container_->ChangeTabContents(new_contents);

  UpdateDevToolsForContents(new_contents);
  // TODO(beng): This should be called automatically by ChangeTabContents, but I
  //             am striving for parity now rather than cleanliness. This is
  //             required to make features like Duplicate Tab, Undo Close Tab,
  //             etc not result in sad tab.
  new_contents->DidBecomeSelected();
  if (BrowserList::GetLastActive() == browser_ &&
      !browser_->tabstrip_model()->closing_all() && GetWindow()->IsVisible()) {
    // We only restore focus if our window is visible, to avoid invoking blur
    // handlers when we are eventually shown.
    new_contents->view()->RestoreFocus();
  }
}

void BrowserView::TabStripEmpty() {
  // Make sure all optional UI is removed before we are destroyed, otherwise
  // there will be consequences (since our view hierarchy will still have
  // references to freed views).
  UpdateUIForContents(NULL);
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, views::SimpleMenuModel::Delegate implementation:

bool BrowserView::IsCommandIdChecked(int command_id) const {
  // TODO(beng): encoding menu.
  // No items in our system menu are check-able.
  return false;
}

bool BrowserView::IsCommandIdEnabled(int command_id) const {
  if (command_id == IDC_RESTORE_TAB)
    return browser_->CanRestoreTab();
  return browser_->command_updater()->IsCommandEnabled(command_id);
}

bool BrowserView::GetAcceleratorForCommandId(int command_id,
                                             views::Accelerator* accelerator) {
  // Let's let the ToolbarView own the canonical implementation of this method.
  return toolbar_->GetAcceleratorForCommandId(command_id, accelerator);
}

bool BrowserView::IsLabelForCommandIdDynamic(int command_id) const {
  return command_id == IDC_RESTORE_TAB;
}

string16 BrowserView::GetLabelForCommandId(int command_id) const {
  DCHECK(command_id == IDC_RESTORE_TAB);

  int string_id = IDS_RESTORE_TAB;
  if (IsCommandIdEnabled(command_id)) {
    TabRestoreService* trs = browser_->profile()->GetTabRestoreService();
    if (trs && trs->entries().front()->type == TabRestoreService::WINDOW)
      string_id = IDS_RESTORE_WINDOW;
  }
  return l10n_util::GetStringUTF16(string_id);
}

void BrowserView::ExecuteCommand(int command_id) {
  browser_->ExecuteCommand(command_id);
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, views::WindowDelegate implementation:

bool BrowserView::CanResize() const {
  return true;
}

bool BrowserView::CanMaximize() const {
  return true;
}

bool BrowserView::IsModal() const {
  return false;
}

std::wstring BrowserView::GetWindowTitle() const {
  return UTF16ToWideHack(browser_->GetWindowTitleForCurrentTab());
}

views::View* BrowserView::GetInitiallyFocusedView() {
  // We set the frame not focus on creation so this should never be called.
  NOTREACHED();
  return NULL;
}

bool BrowserView::ShouldShowWindowTitle() const {
  return browser_->SupportsWindowFeature(Browser::FEATURE_TITLEBAR);
}

SkBitmap BrowserView::GetWindowIcon() {
  if (browser_->type() & Browser::TYPE_APP)
    return browser_->GetCurrentPageIcon();
  return SkBitmap();
}

bool BrowserView::ShouldShowWindowIcon() const {
  return browser_->SupportsWindowFeature(Browser::FEATURE_TITLEBAR);
}

bool BrowserView::ExecuteWindowsCommand(int command_id) {
  // This function handles WM_SYSCOMMAND, WM_APPCOMMAND, and WM_COMMAND.

  // Translate WM_APPCOMMAND command ids into a command id that the browser
  // knows how to handle.
  int command_id_from_app_command = GetCommandIDForAppCommandID(command_id);
  if (command_id_from_app_command != -1)
    command_id = command_id_from_app_command;

  if (browser_->command_updater()->SupportsCommand(command_id)) {
    if (browser_->command_updater()->IsCommandEnabled(command_id))
      browser_->ExecuteCommand(command_id);
    return true;
  }
  return false;
}

std::wstring BrowserView::GetWindowName() const {
  return browser_->GetWindowPlacementKey();
}

void BrowserView::SaveWindowPlacement(const gfx::Rect& bounds,
                                      bool maximized) {
  // If IsFullscreen() is true, we've just changed into fullscreen mode, and
  // we're catching the going-into-fullscreen sizing and positioning calls,
  // which we want to ignore.
  if (!IsFullscreen() && browser_->ShouldSaveWindowPlacement()) {
    WindowDelegate::SaveWindowPlacement(bounds, maximized);
    browser_->SaveWindowPlacement(bounds, maximized);
  }
}

bool BrowserView::GetSavedWindowBounds(gfx::Rect* bounds) const {
  *bounds = browser_->GetSavedWindowBounds();
  if (browser_->type() & Browser::TYPE_POPUP) {
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

    gfx::Rect window_rect = frame_->GetWindow()->GetNonClientView()->
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

bool BrowserView::GetSavedMaximizedState(bool* maximized) const {
  *maximized = browser_->GetSavedMaximizedState();
  return true;
}

views::View* BrowserView::GetContentsView() {
  return contents_container_;
}

views::ClientView* BrowserView::CreateClientView(views::Window* window) {
  set_window(window);
  return this;
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, views::ClientView overrides:

bool BrowserView::CanClose() const {
  if (!browser_extender_->can_close())
    return false;

  // You cannot close a frame for which there is an active originating drag
  // session.
  if (tabstrip_->IsDragSessionActive())
    return false;

  // Give beforeunload handlers the chance to cancel the close before we hide
  // the window below.
  if (!browser_->ShouldCloseWindow())
    return false;

  if (!browser_->tabstrip_model()->empty()) {
    // Tab strip isn't empty.  Hide the frame (so it appears to have closed
    // immediately) and close all the tabs, allowing the renderers to shut
    // down. When the tab strip is empty we'll be called back again.
    frame_->GetWindow()->HideWindow();
    browser_->OnWindowClosing();
    return false;
  }

  // Empty TabStripModel, it's now safe to allow the Window to be closed.
  NotificationService::current()->Notify(
      NotificationType::WINDOW_CLOSED,
      Source<gfx::NativeWindow>(frame_->GetWindow()->GetNativeWindow()),
      NotificationService::NoDetails());
  return true;
}

int BrowserView::NonClientHitTest(const gfx::Point& point) {
  // Since the TabStrip only renders in some parts of the top of the window,
  // the un-obscured area is considered to be part of the non-client caption
  // area of the window. So we need to treat hit-tests in these regions as
  // hit-tests of the titlebar.

  if (!frame_->GetWindow()->IsMaximized() &&
      !frame_->GetWindow()->IsFullscreen()) {
#if defined(OS_WIN)
    CRect client_rect;
    ::GetClientRect(frame_->GetWindow()->GetNativeWindow(), &client_rect);
    gfx::Size resize_corner_size = ResizeCorner::GetSize();
    gfx::Rect resize_corner_rect(client_rect.right - resize_corner_size.width(),
        client_rect.bottom - resize_corner_size.height(),
        resize_corner_size.width(), resize_corner_size.height());
    bool rtl_dir = (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT);
    if (rtl_dir)
      resize_corner_rect.set_x(0);
    if (resize_corner_rect.Contains(point)) {
      if (rtl_dir)
        return HTBOTTOMLEFT;
      return HTBOTTOMRIGHT;
    }
#endif
  }

  gfx::Point point_in_browser_view_coords(point);
  View::ConvertPointToView(GetParent(), this, &point_in_browser_view_coords);

  // Determine if the TabStrip exists and is capable of being clicked on. We
  // might be a popup window without a TabStrip.
  if (IsTabStripVisible()) {

    // See if the mouse pointer is within the bounds of the TabStrip.
    gfx::Point point_in_tabstrip_coords(point);
    View::ConvertPointToView(GetParent(), tabstrip_->GetView(),
                             &point_in_tabstrip_coords);
    if (tabstrip_->GetView()->HitTest(point_in_tabstrip_coords)) {
      if (tabstrip_->IsPositionInWindowCaption(point_in_tabstrip_coords))
        return HTCAPTION;
      return HTCLIENT;
    }

    // The top few pixels of the TabStrip are a drop-shadow - as we're pretty
    // starved of dragable area, let's give it to window dragging (this also
    // makes sense visually).
    if (!IsMaximized() &&
        (point_in_browser_view_coords.y() <
         (tabstrip_->GetView()->y() + kTabShadowSize))) {
      // We return HTNOWHERE as this is a signal to our containing
      // NonClientView that it should figure out what the correct hit-test
      // code is given the mouse position...
      return HTNOWHERE;
    }
  }

  if (browser_extender_->NonClientHitTest(point_in_browser_view_coords))
    return HTCLIENT;

  // If the point's y coordinate is below the top of the toolbar and otherwise
  // within the bounds of this view, the point is considered to be within the
  // client area.
  gfx::Rect bv_bounds = bounds();
  bv_bounds.Offset(0, toolbar_->y());
  bv_bounds.set_height(bv_bounds.height() - toolbar_->y());
  if (bv_bounds.Contains(point))
    return HTCLIENT;

  // If the point's y coordinate is above the top of the toolbar, but not in
  // the tabstrip (per previous checking in this function), then we consider it
  // in the window caption (e.g. the area to the right of the tabstrip
  // underneath the window controls). However, note that we DO NOT return
  // HTCAPTION here, because when the window is maximized the window controls
  // will fall into this space (since the BrowserView is sized to entire size
  // of the window at that point), and the HTCAPTION value will cause the
  // window controls not to work. So we return HTNOWHERE so that the caller
  // will hit-test the window controls before finally falling back to
  // HTCAPTION.
  bv_bounds = bounds();
  bv_bounds.set_height(toolbar_->y());
  if (bv_bounds.Contains(point))
    return HTNOWHERE;

  // If the point is somewhere else, delegate to the default implementation.
  return views::ClientView::NonClientHitTest(point);
}

gfx::Size BrowserView::GetMinimumSize() {
  // TODO(noname): In theory the tabstrip width should probably be
  // (OTR + tabstrip + caption buttons) width.
  gfx::Size tabstrip_size(
      browser_->SupportsWindowFeature(Browser::FEATURE_TABSTRIP) ?
      tabstrip_->GetView()->GetMinimumSize() : gfx::Size());
  gfx::Size toolbar_size(
      (browser_->SupportsWindowFeature(Browser::FEATURE_TOOLBAR) ||
       browser_->SupportsWindowFeature(Browser::FEATURE_LOCATIONBAR)) ?
      toolbar_->GetMinimumSize() : gfx::Size());
  if (tabstrip_size.height() && toolbar_size.height())
    toolbar_size.Enlarge(0, -kToolbarTabStripVerticalOverlap);
  gfx::Size bookmark_bar_size;
  if (active_bookmark_bar_ &&
      browser_->SupportsWindowFeature(Browser::FEATURE_BOOKMARKBAR)) {
    bookmark_bar_size = active_bookmark_bar_->GetMinimumSize();
    bookmark_bar_size.Enlarge(0,
        -kSeparationLineHeight - bookmark_bar_view_->GetToolbarOverlap(true));
  }
  gfx::Size contents_size(contents_split_->GetMinimumSize());

  int min_height = tabstrip_size.height() + toolbar_size.height() +
      bookmark_bar_size.height() + contents_size.height();
  int widths[] = { tabstrip_size.width(), toolbar_size.width(),
                   bookmark_bar_size.width(), contents_size.width() };
  int min_width = *std::max_element(&widths[0], &widths[arraysize(widths)]);
  return gfx::Size(min_width, min_height);
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, views::View overrides:

std::string BrowserView::GetClassName() const {
  return kBrowserViewClassName;
}

void BrowserView::Layout() {
  if (ignore_layout_)
    return;

  int top = LayoutTabStrip();
  top = LayoutToolbar(top);
  top = LayoutBookmarkAndInfoBars(top);
  int bottom = LayoutExtensionAndDownloadShelves();
  LayoutTabContents(top, bottom);
  // This must be done _after_ we lay out the TabContents since this code calls
  // back into us to find the bounding box the find bar must be laid out within,
  // and that code depends on the TabContentsContainer's bounds being up to
  // date.
  if (browser_->HasFindBarController()) {
    browser_->GetFindBarController()->find_bar()->MoveWindowIfNecessary(
        gfx::Rect(), true);
  }
  // Align status bubble with the bottom of the contents_container_.
  LayoutStatusBubble(top + contents_container_->bounds().height());
  SchedulePaint();
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

bool BrowserView::GetAccessibleRole(AccessibilityTypes::Role* role) {
  DCHECK(role);

  *role = AccessibilityTypes::ROLE_CLIENT;
  return true;
}

bool BrowserView::GetAccessibleName(std::wstring* name) {
  DCHECK(name);

  if (!accessible_name_.empty()) {
    *name = accessible_name_;
    return true;
  }
  return false;
}

void BrowserView::SetAccessibleName(const std::wstring& name) {
  accessible_name_ = name;
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView, private:

void BrowserView::Init() {
  // Stow a pointer to this object onto the window handle so that we can get
  // at it later when all we have is a native view.
#if defined(OS_WIN)
  SetProp(GetWidget()->GetNativeView(), kBrowserViewKey, this);
#else
  g_object_set_data(G_OBJECT(GetWidget()->GetNativeView()),
                    kBrowserViewKey, this);
#endif

  // Start a hung plugin window detector for this browser object (as long as
  // hang detection is not disabled).
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableHangMonitor)) {
    InitHangMonitor();
  }

  LoadAccelerators();
  SetAccessibleName(l10n_util::GetString(IDS_PRODUCT_NAME));

  tabstrip_ = TabStripWrapper::CreateTabStrip(browser_->tabstrip_model());
  tabstrip_->
      GetView()->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_TABSTRIP));
  AddChildView(tabstrip_->GetView());
  frame_->TabStripCreated(tabstrip_);

  toolbar_ = new ToolbarView(browser_.get());
  AddChildView(toolbar_);
  toolbar_->SetID(VIEW_ID_TOOLBAR);
  toolbar_->Init(browser_->profile());
  toolbar_->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_TOOLBAR));

  infobar_container_ = new InfoBarContainer(this);
  AddChildView(infobar_container_);

  contents_container_ = new TabContentsContainer;
  devtools_container_ = new TabContentsContainer;
  devtools_container_->SetID(VIEW_ID_DEV_TOOLS_DOCKED);
  devtools_container_->SetVisible(false);
  contents_split_ = new views::SingleSplitView(
      contents_container_,
      devtools_container_,
      views::SingleSplitView::VERTICAL_SPLIT);
  SkColor bg_color = GetWidget()->GetThemeProvider()->
      GetColor(BrowserThemeProvider::COLOR_TOOLBAR);
  contents_split_->set_background(
      views::Background::CreateSolidBackground(bg_color));
  AddChildView(contents_split_);
  set_contents_view(contents_split_);

  status_bubble_.reset(new StatusBubbleViews(GetWidget()));

  if (browser_->SupportsWindowFeature(Browser::FEATURE_EXTENSIONSHELF)) {
    extension_shelf_ = new ExtensionShelf(browser_.get());
    extension_shelf_->set_background(
        new BookmarkExtensionBackground(this, extension_shelf_,
                                        browser_.get()));
    extension_shelf_->
        SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_EXTENSIONS));
    AddChildView(extension_shelf_);
  }

#if defined(OS_WIN)
  InitSystemMenu();

  // Create a custom JumpList and add it to an observer of TabRestoreService
  // so we can update the custom JumpList when a tab is added or removed.
  if (JumpList::Enabled()) {
    jumplist_.reset(new JumpList);
    jumplist_->AddObserver(browser_->profile());
  }
#endif

  browser_extender_.reset(BrowserExtender::Create(this));
}

#if defined(OS_WIN)
void BrowserView::InitSystemMenu() {
  system_menu_contents_.reset(new views::SystemMenuModel(this));
  // We add the menu items in reverse order so that insertion_index never needs
  // to change.
  if (IsBrowserTypeNormal())
    BuildSystemMenuForBrowserWindow();
  else
    BuildSystemMenuForPopupWindow();
  system_menu_.reset(
      new views::NativeMenuWin(system_menu_contents_.get(),
                               frame_->GetWindow()->GetNativeWindow()));
  system_menu_->Rebuild();
}
#endif

int BrowserView::LayoutTabStrip() {
  gfx::Rect tabstrip_bounds = frame_->GetBoundsForTabStrip(tabstrip_);
  gfx::Rect toolbar_bounds = GetToolbarBounds();
  tabstrip_->SetBackgroundOffset(
      gfx::Point(tabstrip_bounds.x() - toolbar_bounds.x(),
                 tabstrip_bounds.y()));

  gfx::Point tabstrip_origin = tabstrip_bounds.origin();
  ConvertPointToView(GetParent(), this, &tabstrip_origin);
  tabstrip_bounds.set_origin(tabstrip_origin);

  // Layout extra components.
  tabstrip_bounds = browser_extender_->Layout(tabstrip_bounds);

  bool visible = IsTabStripVisible();
  int y = visible ? tabstrip_bounds.y() : 0;
  int height = visible ? tabstrip_bounds.height() : 0;
  int bottom = y + height;
  tabstrip_->GetView()->SetVisible(visible);
  tabstrip_->GetView()->SetBounds(tabstrip_bounds.x(), y,
                                  tabstrip_bounds.width(), height);
  return bottom;
}

int BrowserView::LayoutToolbar(int top) {
  int browser_view_width = width();
  bool visible = IsToolbarVisible();
  toolbar_->location_bar()->SetFocusable(visible);
  int y = top -
      ((visible && IsTabStripVisible()) ? kToolbarTabStripVerticalOverlap : 0);
  int height = visible ? toolbar_->GetPreferredSize().height() : 0;
  toolbar_->SetVisible(visible);
  toolbar_->SetBounds(0, y, browser_view_width, height);
  return y + height;
}

int BrowserView::LayoutBookmarkAndInfoBars(int top) {
  find_bar_y_ = top + y() - 1;
  if (active_bookmark_bar_) {
    // If we're showing the Bookmark bar in detached style, then we need to show
    // any Info bar _above_ the Bookmark bar, since the Bookmark bar is styled
    // to look like it's part of the page.
    if (bookmark_bar_view_->IsDetached())
      return LayoutTopBar(LayoutInfoBar(top));
    // Otherwise, Bookmark bar first, Info bar second.
    top = LayoutTopBar(top);
  }
  find_bar_y_ = top + y() - 1;
  return LayoutInfoBar(top);
}

int BrowserView::LayoutTopBar(int top) {
  // This method lays out the the bookmark bar, and, if required, the extension
  // shelf by its side. The bookmark bar appears on the right of the extension
  // shelf. If there are too many bookmark items and extension toolstrips to fit
  // in the single bar, some compromises are made as follows:
  // 1. The bookmark bar is shrunk till it reaches the minimum width.
  // 2. After reaching the minimum width, the bookmark bar width is kept fixed -
  //    the extension shelf bar width is reduced.
  DCHECK(active_bookmark_bar_);
  int y = top, x = 0;
  if (!IsBookmarkBarVisible()) {
    bookmark_bar_view_->SetVisible(false);
    bookmark_bar_view_->SetBounds(0, y, width(), 0);
    if (extension_shelf_->IsOnTop())
      extension_shelf_->SetVisible(false);
    return y;
  }

  int bookmark_bar_height = bookmark_bar_view_->GetPreferredSize().height();
  y -= kSeparationLineHeight + (bookmark_bar_view_->IsDetached() ?
      0 : bookmark_bar_view_->GetToolbarOverlap(false));

  if (extension_shelf_->IsOnTop()) {
    if (!bookmark_bar_view_->IsDetached()) {
      int extension_shelf_width =
          extension_shelf_->GetPreferredSize().width();
      int bookmark_bar_given_width = width() - extension_shelf_width;
      int minimum_allowed_bookmark_bar_width =
          bookmark_bar_view_->GetMinimumSize().width();
      if (bookmark_bar_given_width < minimum_allowed_bookmark_bar_width) {
        // The bookmark bar cannot compromise on its width any more. The
        // extension shelf needs to shrink now.
        extension_shelf_width =
            width() - minimum_allowed_bookmark_bar_width;
      }
      extension_shelf_->SetVisible(true);
      extension_shelf_->SetBounds(x, y, extension_shelf_width,
                                  bookmark_bar_height);
      x += extension_shelf_width;
    } else {
      // TODO(sidchat): For detached style bookmark bar, set the extensions
      // shelf in a better position. Issue = 20741.
      extension_shelf_->SetVisible(false);
    }
  }

  bookmark_bar_view_->SetVisible(true);
  bookmark_bar_view_->SetBounds(x, y, width() - x, bookmark_bar_height);
  return y + bookmark_bar_height;
}

int BrowserView::LayoutInfoBar(int top) {
  bool visible = browser_->SupportsWindowFeature(Browser::FEATURE_INFOBAR);
  int height = visible ? infobar_container_->GetPreferredSize().height() : 0;
  infobar_container_->SetVisible(visible);
  infobar_container_->SetBounds(0, top, width(), height);
  return top + height;
}

void BrowserView::LayoutTabContents(int top, int bottom) {
  contents_split_->SetBounds(0, top, width(), bottom - top);
}

int BrowserView::LayoutExtensionAndDownloadShelves() {
  // If we're showing the Bookmark bar in detached style, then we need to show
  // any Info bar _above_ the Bookmark bar, since the Bookmark bar is styled
  // to look like it's part of the page.
  int bottom = height();
  if (extension_shelf_) {
    if (extension_shelf_->IsDetached()) {
      bottom = LayoutDownloadShelf(bottom);
      return LayoutExtensionShelf(bottom);
    }
    // Otherwise, Extension shelf first, Download shelf second.
    bottom = LayoutExtensionShelf(bottom);
  }
  return LayoutDownloadShelf(bottom);
}

int BrowserView::LayoutDownloadShelf(int bottom) {
  // Re-layout the shelf either if it is visible or if it's close animation
  // is currently running.
  if (IsDownloadShelfVisible() ||
      (download_shelf_.get() && download_shelf_->IsClosing())) {
    bool visible = browser_->SupportsWindowFeature(
        Browser::FEATURE_DOWNLOADSHELF);
    DCHECK(download_shelf_.get());
    int height = visible ? download_shelf_->GetPreferredSize().height() : 0;
    download_shelf_->SetVisible(visible);
    download_shelf_->SetBounds(0, bottom - height, width(), height);
    download_shelf_->Layout();
    bottom -= height;
  }
  return bottom;
}

void BrowserView::LayoutStatusBubble(int top) {
  // In restored mode, the client area has a client edge between it and the
  // frame.
  int overlap = StatusBubbleViews::kShadowThickness +
      (IsMaximized() ? 0 : views::NonClientFrameView::kClientEdgeThickness);
  int height = status_bubble_->GetPreferredSize().height();
  gfx::Point origin(-overlap, top - height + overlap);
  ConvertPointToView(this, GetParent(), &origin);
  status_bubble_->SetBounds(origin.x(), origin.y(), width() / 3, height);
}

int BrowserView::LayoutExtensionShelf(int bottom) {
  if (!extension_shelf_ || extension_shelf_->IsOnTop())
    return bottom;

  if (extension_shelf_) {
    bool visible = browser_->SupportsWindowFeature(
        Browser::FEATURE_EXTENSIONSHELF);
    int height =
        visible ? extension_shelf_->GetPreferredSize().height() : 0;
    extension_shelf_->SetVisible(visible);
    extension_shelf_->SetBounds(0, bottom - height, width(), height);
    extension_shelf_->Layout();
    bottom -= height;
  }
  return bottom;
}

bool BrowserView::MaybeShowBookmarkBar(TabContents* contents) {
  views::View* new_bookmark_bar_view = NULL;
  if (browser_->SupportsWindowFeature(Browser::FEATURE_BOOKMARKBAR)
      && contents) {
    if (!bookmark_bar_view_.get()) {
      bookmark_bar_view_.reset(new BookmarkBarView(contents->profile(),
                                                   browser_.get()));
      bookmark_bar_view_->set_parent_owned(false);
      bookmark_bar_view_->set_background(
          new BookmarkExtensionBackground(this, bookmark_bar_view_.get(),
                                          browser_.get()));
    } else {
      bookmark_bar_view_->SetProfile(contents->profile());
    }
    bookmark_bar_view_->SetPageNavigator(contents);
    bookmark_bar_view_->
        SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_BOOKMARKS));
    new_bookmark_bar_view = bookmark_bar_view_.get();
  }
  return UpdateChildViewAndLayout(new_bookmark_bar_view, &active_bookmark_bar_);
}

bool BrowserView::MaybeShowInfoBar(TabContents* contents) {
  // TODO(beng): Remove this function once the interface between
  //             InfoBarContainer, DownloadShelfView and TabContents and this
  //             view is sorted out.
  return true;
}

void BrowserView::UpdateDevToolsForContents(TabContents* tab_contents) {
  TabContents* devtools_contents =
      DevToolsWindow::GetDevToolsContents(tab_contents);

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
    devtools_container_->SetVisible(true);
    contents_split_->Layout();
  } else if (should_hide) {
    // Store split offset when hiding devtools window only.
    g_browser_process->local_state()->SetInteger(
        prefs::kDevToolsSplitLocation, contents_split_->divider_offset());

    // Restore focus to the last focused view when hiding devtools window.
    devtools_focus_tracker_->FocusLastFocusedExternalView();

    devtools_container_->SetVisible(false);
    contents_split_->Layout();
  }
}

void BrowserView::UpdateUIForContents(TabContents* contents) {
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
    new_view->SetBounds((*old_view)->bounds());
    new_view->SchedulePaint();
  } else if (new_view) {
    DCHECK(new_height == 0);
    // The heights are the same, but the old view is null. This only happens
    // when the height is zero. Zero out the bounds.
    new_view->SetBounds(0, 0, 0, 0);
  }
  *old_view = new_view;
  return changed;
}

void BrowserView::LoadAccelerators() {
#if defined(OS_WIN)
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
    views::Accelerator accelerator(
        static_cast<base::KeyboardCode>(accelerators[i].key),
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
    views::Accelerator accelerator(browser::kAcceleratorMap[i].keycode,
                                   browser::kAcceleratorMap[i].shift_pressed,
                                   browser::kAcceleratorMap[i].ctrl_pressed,
                                   browser::kAcceleratorMap[i].alt_pressed);
    accelerator_table_[accelerator] = browser::kAcceleratorMap[i].command_id;

    // Also register with the focus manager.
    focus_manager->RegisterAccelerator(accelerator, this);
  }
#endif
}

#if defined(OS_WIN)
void BrowserView::BuildSystemMenuForBrowserWindow() {
  system_menu_contents_->AddSeparator();
  system_menu_contents_->AddItemWithStringId(IDC_TASK_MANAGER,
                                             IDS_TASK_MANAGER);
  system_menu_contents_->AddSeparator();
  system_menu_contents_->AddItemWithStringId(IDC_RESTORE_TAB, IDS_RESTORE_TAB);
  system_menu_contents_->AddItemWithStringId(IDC_NEW_TAB, IDS_NEW_TAB);
  // If it's a regular browser window with tabs, we don't add any more items,
  // since it already has menus (Page, Chrome).
}

void BrowserView::BuildSystemMenuForPopupWindow() {
  system_menu_contents_->AddSeparator();
  system_menu_contents_->AddItemWithStringId(IDC_TASK_MANAGER,
                                             IDS_TASK_MANAGER);
  system_menu_contents_->AddSeparator();
  encoding_menu_contents_.reset(new EncodingMenuModel(browser_.get()));
  system_menu_contents_->AddSubMenuWithStringId(IDS_ENCODING_MENU,
                                                encoding_menu_contents_.get());
  zoom_menu_contents_.reset(new ZoomMenuModel(this));
  system_menu_contents_->AddSubMenuWithStringId(IDS_ZOOM_MENU,
                                                zoom_menu_contents_.get());
  system_menu_contents_->AddItemWithStringId(IDC_PRINT, IDS_PRINT);
  system_menu_contents_->AddItemWithStringId(IDC_SAVE_PAGE,
                                             IDS_SAVE_PAGE);
  system_menu_contents_->AddItemWithStringId(IDC_FIND, IDS_FIND);
  system_menu_contents_->AddSeparator();
  system_menu_contents_->AddItemWithStringId(IDC_PASTE, IDS_PASTE);
  system_menu_contents_->AddItemWithStringId(IDC_COPY, IDS_COPY);
  system_menu_contents_->AddItemWithStringId(IDC_CUT, IDS_CUT);
  system_menu_contents_->AddSeparator();
  system_menu_contents_->AddItemWithStringId(IDC_NEW_TAB,
                                             IDS_APP_MENU_NEW_WEB_PAGE);
  system_menu_contents_->AddItemWithStringId(IDC_SHOW_AS_TAB, IDS_SHOW_AS_TAB);
  system_menu_contents_->AddItemWithStringId(IDC_COPY_URL,
                                             IDS_APP_MENU_COPY_URL);
  system_menu_contents_->AddItemWithStringId(IDC_DUPLICATE_TAB,
                                             IDS_APP_MENU_DUPLICATE_APP_WINDOW);
  system_menu_contents_->AddItemWithStringId(IDC_RESTORE_TAB, IDS_RESTORE_TAB);
  system_menu_contents_->AddSeparator();
  system_menu_contents_->AddItemWithStringId(IDC_RELOAD, IDS_APP_MENU_RELOAD);
  system_menu_contents_->AddItemWithStringId(IDC_FORWARD,
                                             IDS_CONTENT_CONTEXT_FORWARD);
  system_menu_contents_->AddItemWithStringId(IDC_BACK,
                                             IDS_CONTENT_CONTEXT_BACK);
}
#endif

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

void BrowserView::LoadingAnimationCallback() {
  if (browser_->type() == Browser::TYPE_NORMAL) {
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
    frame_->UpdateThrobber(tab_contents && tab_contents->is_loading());
  }
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

// static
void BrowserView::InitClass() {
  static bool initialized = false;
  if (!initialized) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    default_favicon_ = *rb.GetBitmapNamed(IDR_DEFAULT_FAVICON);
    initialized = true;
  }
}

// static
BrowserWindow* BrowserWindow::CreateBrowserWindow(Browser* browser) {
  // Create the view and the frame. The frame will attach itself via the view
  // so we don't need to do anything with the pointer.
  BrowserView* view = new BrowserView(browser);
  BrowserFrame::Create(view, browser->profile());
  return view;
}

// static
FindBar* BrowserWindow::CreateFindBar(Browser* browser) {
  return browser::CreateFindBar(static_cast<BrowserView*>(browser->window()));
}

// static
void BrowserList::AllBrowsersClosed() {
  views::Window::CloseAllSecondaryWindows();
}
