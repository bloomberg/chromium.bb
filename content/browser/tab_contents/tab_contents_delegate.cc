// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tab_contents/tab_contents_delegate.h"

#include "base/compiler_specific.h"
#include "base/memory/singleton.h"
#include "content/browser/javascript_dialogs.h"
#include "content/common/url_constants.h"
#include "ui/gfx/rect.h"

std::string TabContentsDelegate::GetNavigationHeaders(const GURL& url) {
  return std::string();
}

void TabContentsDelegate::LoadProgressChanged(double progress) {
}

void TabContentsDelegate::DetachContents(TabContents* source) {
}

bool TabContentsDelegate::IsPopupOrPanel(const TabContents* source) const {
  return false;
}

TabContents* TabContentsDelegate::GetConstrainingContents(TabContents* source) {
  return source;
}

bool TabContentsDelegate::ShouldFocusConstrainedWindow() {
  return true;
}

void TabContentsDelegate::WillShowConstrainedWindow(TabContents* source) {
}

void TabContentsDelegate::ContentsMouseEvent(
    TabContents* source, const gfx::Point& location, bool motion) {
}

void TabContentsDelegate::ContentsZoomChange(bool zoom_in) { }

bool TabContentsDelegate::IsApplication() const { return false; }

void TabContentsDelegate::ConvertContentsToApplication(TabContents* source) { }

bool TabContentsDelegate::CanReloadContents(TabContents* source) const {
  return true;
}

void TabContentsDelegate::WillRunBeforeUnloadConfirm() {
}

bool TabContentsDelegate::ShouldSuppressDialogs() {
  return false;
}

void TabContentsDelegate::BeforeUnloadFired(TabContents* tab,
                                            bool proceed,
                                            bool* proceed_to_fire_unload) {
  *proceed_to_fire_unload = true;
}

bool TabContentsDelegate::IsExternalTabContainer() const { return false; }

void TabContentsDelegate::SetFocusToLocationBar(bool select_all) {}

bool TabContentsDelegate::ShouldFocusPageAfterCrash() {
  return true;
}

void TabContentsDelegate::RenderWidgetShowing() {}

bool TabContentsDelegate::TakeFocus(bool reverse) {
  return false;
}

void TabContentsDelegate::LostCapture() {
}

void TabContentsDelegate::SetTabContentBlocked(
    TabContents* contents, bool blocked) {
}

void TabContentsDelegate::TabContentsFocused(TabContents* tab_content) {
}

int TabContentsDelegate::GetExtraRenderViewHeight() const {
  return 0;
}

bool TabContentsDelegate::HandleContextMenu(const ContextMenuParams& params) {
  return false;
}

bool TabContentsDelegate::ExecuteContextMenuCommand(int command) {
  return false;
}

void TabContentsDelegate::ShowPageInfo(Profile* profile,
                                       const GURL& url,
                                       const NavigationEntry::SSLStatus& ssl,
                                       bool show_history) {
}

void TabContentsDelegate::ViewSourceForTab(TabContents* source,
                                           const GURL& page_url) {
  // Fall back implementation based entirely on the view-source scheme.
  // It suffers from http://crbug.com/523 and that is why browser overrides
  // it with proper implementation.
  GURL url = GURL(chrome::kViewSourceScheme + std::string(":") +
                      page_url.spec());
  OpenURLFromTab(source,
                 url,
                 GURL(),
                 NEW_FOREGROUND_TAB,
                 PageTransition::LINK);
}

void TabContentsDelegate::ViewSourceForFrame(TabContents* source,
                                             const GURL& frame_url,
                                             const std::string& content_state) {
  // Same as ViewSourceForTab, but for given subframe.
  GURL url = GURL(chrome::kViewSourceScheme + std::string(":") +
                      frame_url.spec());
  OpenURLFromTab(source,
                 url,
                 GURL(),
                 NEW_FOREGROUND_TAB,
                 PageTransition::LINK);
}

bool TabContentsDelegate::PreHandleKeyboardEvent(
    const NativeWebKeyboardEvent& event,
    bool* is_keyboard_shortcut) {
  return false;
}

void TabContentsDelegate::HandleKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
}

void TabContentsDelegate::HandleMouseUp() {
}

void TabContentsDelegate::HandleMouseActivate() {
}

void TabContentsDelegate::DragEnded() {
}

void TabContentsDelegate::ShowRepostFormWarningDialog(
    TabContents* tab_contents) {
}

bool TabContentsDelegate::OnGoToEntryOffset(int offset) {
  return true;
}

bool TabContentsDelegate::ShouldAddNavigationToHistory(
    const history::HistoryAddPageArgs& add_page_args,
    NavigationType::Type navigation_type) {
  return true;
}

gfx::NativeWindow TabContentsDelegate::GetFrameNativeWindow() {
  return NULL;
}

void TabContentsDelegate::TabContentsCreated(TabContents* new_contents) {
}

bool TabContentsDelegate::ShouldEnablePreferredSizeNotifications() {
  return false;
}

void TabContentsDelegate::UpdatePreferredSize(const gfx::Size& pref_size) {
}

void TabContentsDelegate::ContentRestrictionsChanged(TabContents* source) {
}

void TabContentsDelegate::RendererUnresponsive(TabContents* source) {
}

void TabContentsDelegate::RendererResponsive(TabContents* source) {
}

void TabContentsDelegate::WorkerCrashed(TabContents* source) {
}

TabContentsDelegate::MainFrameCommitDetails*
TabContentsDelegate::CreateMainFrameCommitDetails(TabContents* tab) {
  return NULL;
}

void TabContentsDelegate::DidNavigateMainFramePostCommit(
    TabContents* tab,
    const MainFrameCommitDetails& details) {
}

// A stubbed-out version of JavaScriptDialogCreator that doesn't do anything.
class JavaScriptDialogCreatorStub : public content::JavaScriptDialogCreator {
 public:
  static JavaScriptDialogCreatorStub* GetInstance() {
    return Singleton<JavaScriptDialogCreatorStub>::get();
  }

  virtual void RunJavaScriptDialog(content::JavaScriptDialogDelegate* delegate,
                                   const GURL& frame_url,
                                   int dialog_flags,
                                   const string16& message_text,
                                   const string16& default_prompt_text,
                                   IPC::Message* reply_message,
                                   bool* did_suppress_message,
                                   Profile* profile) OVERRIDE {
    *did_suppress_message = true;
  }

  virtual void RunBeforeUnloadDialog(
      content::JavaScriptDialogDelegate* delegate,
      const string16& message_text,
      IPC::Message* reply_message) OVERRIDE {
    delegate->OnDialogClosed(reply_message, true, string16());
  }

  virtual void ResetJavaScriptState(
      content::JavaScriptDialogDelegate* delegate) OVERRIDE {
  }
 private:
  friend struct DefaultSingletonTraits<JavaScriptDialogCreatorStub>;
};

content::JavaScriptDialogCreator*
TabContentsDelegate::GetJavaScriptDialogCreator() {
  return JavaScriptDialogCreatorStub::GetInstance();
}

void TabContentsDelegate::RenderViewCreated(TabContents* source,
                                            RenderViewHost* host) {
}

TabContentsDelegate::~TabContentsDelegate() {
}
