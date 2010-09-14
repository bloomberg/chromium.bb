// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/match_preview.h"

#include <algorithm>

#include "base/command_line.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/page_transition_types.h"
#include "chrome/common/renderer_preferences.h"
#include "ipc/ipc_message.h"

class MatchPreview::TabContentsDelegateImpl : public TabContentsDelegate {
 public:
  explicit TabContentsDelegateImpl(MatchPreview* match_preview)
      : match_preview_(match_preview) {
  }

  virtual void OpenURLFromTab(TabContents* source,
                              const GURL& url, const GURL& referrer,
                              WindowOpenDisposition disposition,
                              PageTransition::Type transition) {}
  virtual void NavigationStateChanged(const TabContents* source,
                                      unsigned changed_flags) {}
  virtual void AddNewContents(TabContents* source,
                              TabContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture) {}
  virtual void ActivateContents(TabContents* contents) {
    match_preview_->CommitCurrentPreview();
  }
  virtual void DeactivateContents(TabContents* contents) {}
  virtual void LoadingStateChanged(TabContents* source) {}
  virtual void CloseContents(TabContents* source) {}
  virtual void MoveContents(TabContents* source, const gfx::Rect& pos) {}
  virtual void DetachContents(TabContents* source) {}
  virtual bool IsPopup(const TabContents* source) const {
    return false;
  }
  virtual TabContents* GetConstrainingContents(TabContents* source) {
    return NULL;
  }
  virtual void ToolbarSizeChanged(TabContents* source, bool is_animating) {}
  virtual void URLStarredChanged(TabContents* source, bool starred) {}
  virtual void UpdateTargetURL(TabContents* source, const GURL& url) {}
  virtual void ContentsMouseEvent(
      TabContents* source, const gfx::Point& location, bool motion) {}
  virtual void ContentsZoomChange(bool zoom_in) {}
  virtual void OnContentSettingsChange(TabContents* source) {}
  virtual bool IsApplication() const { return false; }
  virtual void ConvertContentsToApplication(TabContents* source) {}
  virtual bool CanReloadContents(TabContents* source) const { return true; }
  virtual gfx::Rect GetRootWindowResizerRect() const {
    return match_preview_->host_->delegate() ?
        match_preview_->host_->delegate()->GetRootWindowResizerRect() :
        gfx::Rect();
  }
  virtual void ShowHtmlDialog(HtmlDialogUIDelegate* delegate,
                              gfx::NativeWindow parent_window) {}
  virtual void BeforeUnloadFired(TabContents* tab,
                                 bool proceed,
                                 bool* proceed_to_fire_unload) {}
  virtual void ForwardMessageToExternalHost(const std::string& message,
                                            const std::string& origin,
                                            const std::string& target) {}
  virtual bool IsExternalTabContainer() const { return false; }
  virtual void SetFocusToLocationBar(bool select_all) {}
  virtual void RenderWidgetShowing() {}
  virtual ExtensionFunctionDispatcher* CreateExtensionFunctionDispatcher(
      RenderViewHost* render_view_host,
      const std::string& extension_id) {
    return NULL;
  }
  virtual bool TakeFocus(bool reverse) { return false; }
  virtual void SetTabContentBlocked(TabContents* contents, bool blocked) {}
  virtual void TabContentsFocused(TabContents* tab_content) {
    match_preview_->CommitCurrentPreview();
  }
  virtual int GetExtraRenderViewHeight() const { return 0; }
  virtual bool CanDownload(int request_id) { return false; }
  virtual void OnStartDownload(DownloadItem* download, TabContents* tab) {}
  virtual bool HandleContextMenu(const ContextMenuParams& params) {
    return false;
  }
  virtual bool ExecuteContextMenuCommand(int command) {
    return false;
  }
  virtual void ConfirmAddSearchProvider(const TemplateURL* template_url,
                                        Profile* profile) {}
  virtual void ShowPageInfo(Profile* profile,
                            const GURL& url,
                            const NavigationEntry::SSLStatus& ssl,
                            bool show_history) {}
  virtual bool PreHandleKeyboardEvent(const NativeWebKeyboardEvent& event,
                                      bool* is_keyboard_shortcut) {
    return false;
  }
  virtual void HandleKeyboardEvent(const NativeWebKeyboardEvent& event) {}
  virtual void ShowRepostFormWarningDialog(TabContents* tab_contents) {}
  virtual void ShowContentSettingsWindow(ContentSettingsType content_type) {}
  virtual void ShowCollectedCookiesDialog(TabContents* tab_contents) {}
  virtual bool OnGoToEntryOffset(int offset) { return false; }
  virtual bool ShouldAddNavigationToHistory(
      const history::HistoryAddPageArgs& add_page_args,
      NavigationType::Type navigation_type) {
    return false;
  }
  virtual void OnDidGetApplicationInfo(TabContents* tab_contents,
                                       int32 page_id) {}
  virtual gfx::NativeWindow GetFrameNativeWindow() {
    return match_preview_->host_->delegate() ?
        match_preview_->host_->delegate()->GetFrameNativeWindow() : NULL;
  }
  virtual void TabContentsCreated(TabContents* new_contents) {}
  virtual bool infobars_enabled() { return false; }
  virtual bool ShouldEnablePreferredSizeNotifications() { return false; }
  virtual void UpdatePreferredSize(const gfx::Size& pref_size) {}
  virtual void ContentTypeChanged(TabContents* source) {}

 private:
  MatchPreview* match_preview_;

  DISALLOW_COPY_AND_ASSIGN(TabContentsDelegateImpl);
};

MatchPreview::MatchPreview(TabContents* host) : host_(host) {
  delegate_.reset(new TabContentsDelegateImpl(this));
}

MatchPreview::~MatchPreview() {
  // Delete the TabContents before the delegate as the TabContents holds a
  // reference to the delegate.
  preview_contents_.reset(NULL);
}

// static
bool MatchPreview::IsEnabled() {
  static bool enabled = false;
  static bool checked = false;
  if (!checked) {
    checked = true;
    enabled = CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kEnableMatchPreview);
  }
  return enabled;
}

void MatchPreview::Update(const GURL& url) {
  if (url_ == url)
    return;

  url_ = url;

  if (url_.is_empty() || !url_.is_valid()) {
    DestroyPreviewContents();
    return;
  }

  if (!preview_contents_.get()) {
    preview_contents_.reset(
        new TabContents(host_->profile(), NULL, MSG_ROUTING_NONE, NULL, NULL));
    preview_contents_->set_delegate(delegate_.get());
    NotificationService::current()->Notify(
        NotificationType::MATCH_PREVIEW_TAB_CONTENTS_CREATED,
        Source<TabContents>(host_),
        NotificationService::NoDetails());
  }

  // TODO: figure out transition type.
  preview_contents_->controller().LoadURL(url, GURL(),
                                          PageTransition::GENERATED);
}

void MatchPreview::DestroyPreviewContents() {
  url_ = GURL();
  preview_contents_.reset(NULL);
}

void MatchPreview::CommitCurrentPreview() {
  DCHECK(preview_contents_.get());
  if (host_->delegate())
    host_->delegate()->CommitMatchPreview(host_);
}

TabContents* MatchPreview::ReleasePreviewContents() {
  url_ = GURL();
  return preview_contents_.release();
}
