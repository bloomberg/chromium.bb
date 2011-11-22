// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tab_contents/tab_contents_delegate.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "content/browser/javascript_dialogs.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/common/url_constants.h"
#include "ui/gfx/rect.h"
#include "webkit/glue/web_intent_data.h"

TabContentsDelegate::TabContentsDelegate() {
}

TabContents* TabContentsDelegate::OpenURLFromTab(
    TabContents* source,
    const GURL& url,
    const GURL& referrer,
    WindowOpenDisposition disposition,
    content::PageTransition transition) {
  return OpenURLFromTab(source,
                        OpenURLParams(url, referrer, disposition, transition,
                                      false));
}

TabContents* TabContentsDelegate::OpenURLFromTab(TabContents* source,
                                                 const OpenURLParams& params) {
  return NULL;
}

void TabContentsDelegate::NavigationStateChanged(const TabContents* source,
                                                 unsigned changed_flags) {
}

void TabContentsDelegate::AddNavigationHeaders(const GURL& url,
                                               std::string* headers) {
}

void TabContentsDelegate::AddNewContents(TabContents* source,
                                         TabContents* new_contents,
                                         WindowOpenDisposition disposition,
                                         const gfx::Rect& initial_pos,
                                         bool user_gesture) {
}

void TabContentsDelegate::ActivateContents(TabContents* contents) {
}

void TabContentsDelegate::DeactivateContents(TabContents* contents) {
}

void TabContentsDelegate::LoadingStateChanged(TabContents* source) {
}

void TabContentsDelegate::LoadProgressChanged(double progress) {
}

void TabContentsDelegate::CloseContents(TabContents* source) {
}

void TabContentsDelegate::SwappedOut(TabContents* source) {
}

void TabContentsDelegate::MoveContents(TabContents* source,
                                       const gfx::Rect& pos) {
}

void TabContentsDelegate::DetachContents(TabContents* source) {
}

bool TabContentsDelegate::IsPopupOrPanel(const TabContents* source) const {
  return false;
}

void TabContentsDelegate::UpdateTargetURL(TabContents* source,
                                          int32 page_id,
                                          const GURL& url) {
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

void TabContentsDelegate::TabContentsFocused(TabContents* tab_content) {
}

int TabContentsDelegate::GetExtraRenderViewHeight() const {
  return 0;
}

bool TabContentsDelegate::CanDownload(TabContents* source, int request_id) {
  return true;
}

void TabContentsDelegate::OnStartDownload(TabContents* source,
                                          DownloadItem* download) {
}

bool TabContentsDelegate::HandleContextMenu(const ContextMenuParams& params) {
  return false;
}

bool TabContentsDelegate::ExecuteContextMenuCommand(int command) {
  return false;
}

void TabContentsDelegate::ShowPageInfo(content::BrowserContext* browser_context,
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
                 content::PAGE_TRANSITION_LINK);
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
                 content::PAGE_TRANSITION_LINK);
}

bool TabContentsDelegate::PreHandleKeyboardEvent(
    const NativeWebKeyboardEvent& event,
    bool* is_keyboard_shortcut) {
  return false;
}

void TabContentsDelegate::HandleKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
}

void TabContentsDelegate::HandleMouseDown() {
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
    content::NavigationType navigation_type) {
  return true;
}

gfx::NativeWindow TabContentsDelegate::GetFrameNativeWindow() {
  return NULL;
}

void TabContentsDelegate::TabContentsCreated(TabContents* new_contents) {
}

void TabContentsDelegate::ContentRestrictionsChanged(TabContents* source) {
}

void TabContentsDelegate::RendererUnresponsive(TabContents* source) {
}

void TabContentsDelegate::RendererResponsive(TabContents* source) {
}

void TabContentsDelegate::WorkerCrashed(TabContents* source) {
}

void TabContentsDelegate::DidNavigateMainFramePostCommit(
    TabContents* tab) {
}

void TabContentsDelegate::DidNavigateToPendingEntry(TabContents* tab) {
}

// A stubbed-out version of JavaScriptDialogCreator that doesn't do anything.
class JavaScriptDialogCreatorStub : public content::JavaScriptDialogCreator {
 public:
  static JavaScriptDialogCreatorStub* GetInstance() {
    return Singleton<JavaScriptDialogCreatorStub>::get();
  }

  virtual void RunJavaScriptDialog(content::JavaScriptDialogDelegate* delegate,
                                   TitleType title_type,
                                   const string16& title,
                                   int dialog_flags,
                                   const string16& message_text,
                                   const string16& default_prompt_text,
                                   IPC::Message* reply_message,
                                   bool* did_suppress_message) OVERRIDE {
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

void TabContentsDelegate::RunFileChooser(
  TabContents* tab, const content::FileChooserParams& params) {
}

void TabContentsDelegate::EnumerateDirectory(TabContents* tab, int request_id,
                                             const FilePath& path) {
}

void TabContentsDelegate::ToggleFullscreenModeForTab(TabContents* tab,
                                                     bool enter_fullscreen) {
}

bool TabContentsDelegate::IsFullscreenForTab(const TabContents* tab) const {
  return false;
}

void TabContentsDelegate::JSOutOfMemory(TabContents* tab) {
}

void TabContentsDelegate::RegisterProtocolHandler(TabContents* tab,
                                                  const std::string& protocol,
                                                  const GURL& url,
                                                  const string16& title) {
}

void TabContentsDelegate::RegisterIntentHandler(TabContents* tab,
                                                const string16& action,
                                                const string16& type,
                                                const string16& href,
                                                const string16& title,
                                                const string16& disposition) {
}

void TabContentsDelegate::WebIntentDispatch(
    TabContents* tab,
    int routing_id,
    const webkit_glue::WebIntentData& intent,
    int intent_id) {
}

void TabContentsDelegate::FindReply(TabContents* tab,
                                    int request_id,
                                    int number_of_matches,
                                    const gfx::Rect& selection_rect,
                                    int active_match_ordinal,
                                    bool final_update) {
}

void TabContentsDelegate::CrashedPlugin(TabContents* tab,
                                        const FilePath& plugin_path) {
}

void TabContentsDelegate::UpdatePreferredSize(TabContents* tab,
                                              const gfx::Size& pref_size) {
}

void TabContentsDelegate::WebUISend(TabContents* tab,
                                    const GURL& source_url,
                                    const std::string& name,
                                    const base::ListValue& args) {
}

void TabContentsDelegate::RequestToLockMouse(TabContents* tab) {
}

TabContentsDelegate::~TabContentsDelegate() {
  while (!attached_contents_.empty()) {
    TabContents* tab_contents = *attached_contents_.begin();
    tab_contents->set_delegate(NULL);
  }
  DCHECK(attached_contents_.empty());
}

void TabContentsDelegate::Attach(TabContents* tab_contents) {
  DCHECK(attached_contents_.find(tab_contents) == attached_contents_.end());
  attached_contents_.insert(tab_contents);
}

void TabContentsDelegate::Detach(TabContents* tab_contents) {
  DCHECK(attached_contents_.find(tab_contents) != attached_contents_.end());
  attached_contents_.erase(tab_contents);
}

void TabContentsDelegate::LostMouseLock() {
}
