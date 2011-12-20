// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/web_contents_delegate.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "content/browser/javascript_dialogs.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/intents_host.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/common/url_constants.h"
#include "ui/gfx/rect.h"
#include "webkit/glue/web_intent_data.h"

namespace content {

WebContentsDelegate::WebContentsDelegate() {
}

TabContents* WebContentsDelegate::OpenURLFromTab(TabContents* source,
                                                 const OpenURLParams& params) {
  return NULL;
}

void WebContentsDelegate::NavigationStateChanged(const TabContents* source,
                                                 unsigned changed_flags) {
}

void WebContentsDelegate::AddNavigationHeaders(const GURL& url,
                                               std::string* headers) {
}

void WebContentsDelegate::AddNewContents(TabContents* source,
                                         TabContents* new_contents,
                                         WindowOpenDisposition disposition,
                                         const gfx::Rect& initial_pos,
                                         bool user_gesture) {
}

void WebContentsDelegate::ActivateContents(TabContents* contents) {
}

void WebContentsDelegate::DeactivateContents(TabContents* contents) {
}

void WebContentsDelegate::LoadingStateChanged(TabContents* source) {
}

void WebContentsDelegate::LoadProgressChanged(double progress) {
}

void WebContentsDelegate::CloseContents(TabContents* source) {
}

void WebContentsDelegate::SwappedOut(TabContents* source) {
}

void WebContentsDelegate::MoveContents(TabContents* source,
                                       const gfx::Rect& pos) {
}

void WebContentsDelegate::DetachContents(TabContents* source) {
}

bool WebContentsDelegate::IsPopupOrPanel(const TabContents* source) const {
  return false;
}

void WebContentsDelegate::UpdateTargetURL(TabContents* source,
                                          int32 page_id,
                                          const GURL& url) {
}

void WebContentsDelegate::ContentsMouseEvent(
    TabContents* source, const gfx::Point& location, bool motion) {
}

void WebContentsDelegate::ContentsZoomChange(bool zoom_in) { }

bool WebContentsDelegate::IsApplication() const { return false; }

void WebContentsDelegate::ConvertContentsToApplication(TabContents* source) { }

bool WebContentsDelegate::CanReloadContents(TabContents* source) const {
  return true;
}

void WebContentsDelegate::WillRunBeforeUnloadConfirm() {
}

bool WebContentsDelegate::ShouldSuppressDialogs() {
  return false;
}

void WebContentsDelegate::BeforeUnloadFired(TabContents* tab,
                                            bool proceed,
                                            bool* proceed_to_fire_unload) {
  *proceed_to_fire_unload = true;
}

void WebContentsDelegate::SetFocusToLocationBar(bool select_all) {}

bool WebContentsDelegate::ShouldFocusPageAfterCrash() {
  return true;
}

void WebContentsDelegate::RenderWidgetShowing() {}

bool WebContentsDelegate::TakeFocus(bool reverse) {
  return false;
}

void WebContentsDelegate::LostCapture() {
}

void WebContentsDelegate::TabContentsFocused(TabContents* tab_content) {
}

int WebContentsDelegate::GetExtraRenderViewHeight() const {
  return 0;
}

bool WebContentsDelegate::CanDownload(TabContents* source, int request_id) {
  return true;
}

void WebContentsDelegate::OnStartDownload(TabContents* source,
                                          DownloadItem* download) {
}

bool WebContentsDelegate::HandleContextMenu(const ContextMenuParams& params) {
  return false;
}

bool WebContentsDelegate::ExecuteContextMenuCommand(int command) {
  return false;
}

void WebContentsDelegate::ShowPageInfo(content::BrowserContext* browser_context,
                                       const GURL& url,
                                       const NavigationEntry::SSLStatus& ssl,
                                       bool show_history) {
}

void WebContentsDelegate::ViewSourceForTab(TabContents* source,
                                           const GURL& page_url) {
  // Fall back implementation based entirely on the view-source scheme.
  // It suffers from http://crbug.com/523 and that is why browser overrides
  // it with proper implementation.
  GURL url = GURL(chrome::kViewSourceScheme + std::string(":") +
                      page_url.spec());
  OpenURLFromTab(source, OpenURLParams(url, content::Referrer(),
                                       NEW_FOREGROUND_TAB,
                                       content::PAGE_TRANSITION_LINK, false));
}

void WebContentsDelegate::ViewSourceForFrame(TabContents* source,
                                             const GURL& frame_url,
                                             const std::string& content_state) {
  // Same as ViewSourceForTab, but for given subframe.
  GURL url = GURL(chrome::kViewSourceScheme + std::string(":") +
                      frame_url.spec());
  OpenURLFromTab(source, OpenURLParams(url, content::Referrer(),
                                       NEW_FOREGROUND_TAB,
                                       content::PAGE_TRANSITION_LINK, false));
}

bool WebContentsDelegate::PreHandleKeyboardEvent(
    const NativeWebKeyboardEvent& event,
    bool* is_keyboard_shortcut) {
  return false;
}

void WebContentsDelegate::HandleKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
}

void WebContentsDelegate::HandleMouseDown() {
}

void WebContentsDelegate::HandleMouseUp() {
}

void WebContentsDelegate::HandleMouseActivate() {
}

void WebContentsDelegate::DragEnded() {
}

void WebContentsDelegate::ShowRepostFormWarningDialog(
    TabContents* tab_contents) {
}

bool WebContentsDelegate::OnGoToEntryOffset(int offset) {
  return true;
}

bool WebContentsDelegate::ShouldAddNavigationToHistory(
    const history::HistoryAddPageArgs& add_page_args,
    content::NavigationType navigation_type) {
  return true;
}

gfx::NativeWindow WebContentsDelegate::GetFrameNativeWindow() {
  return NULL;
}

void WebContentsDelegate::TabContentsCreated(TabContents* new_contents) {
}

void WebContentsDelegate::ContentRestrictionsChanged(TabContents* source) {
}

void WebContentsDelegate::RendererUnresponsive(TabContents* source) {
}

void WebContentsDelegate::RendererResponsive(TabContents* source) {
}

void WebContentsDelegate::WorkerCrashed(TabContents* source) {
}

void WebContentsDelegate::DidNavigateMainFramePostCommit(
    TabContents* tab) {
}

void WebContentsDelegate::DidNavigateToPendingEntry(TabContents* tab) {
}

// A stubbed-out version of JavaScriptDialogCreator that doesn't do anything.
class JavaScriptDialogCreatorStub : public content::JavaScriptDialogCreator {
 public:
  static JavaScriptDialogCreatorStub* GetInstance() {
    return Singleton<JavaScriptDialogCreatorStub>::get();
  }

  virtual void RunJavaScriptDialog(
      content::JavaScriptDialogDelegate* delegate,
      TitleType title_type,
      const string16& title,
      ui::JavascriptMessageType javascript_message_type,
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
WebContentsDelegate::GetJavaScriptDialogCreator() {
  return JavaScriptDialogCreatorStub::GetInstance();
}

void WebContentsDelegate::RunFileChooser(
  TabContents* tab, const content::FileChooserParams& params) {
}

void WebContentsDelegate::EnumerateDirectory(TabContents* tab, int request_id,
                                             const FilePath& path) {
}

void WebContentsDelegate::ToggleFullscreenModeForTab(TabContents* tab,
                                                     bool enter_fullscreen) {
}

bool WebContentsDelegate::IsFullscreenForTab(const TabContents* tab) const {
  return false;
}

void WebContentsDelegate::JSOutOfMemory(TabContents* tab) {
}

void WebContentsDelegate::RegisterProtocolHandler(TabContents* tab,
                                                  const std::string& protocol,
                                                  const GURL& url,
                                                  const string16& title) {
}

void WebContentsDelegate::RegisterIntentHandler(TabContents* tab,
                                                const string16& action,
                                                const string16& type,
                                                const string16& href,
                                                const string16& title,
                                                const string16& disposition) {
}

void WebContentsDelegate::WebIntentDispatch(
    TabContents* tab,
    content::IntentsHost* intents_host) {
  // The caller passes this method ownership of the |intents_host|, but this
  // empty implementation will not use it, so we delete it immediately.
  delete intents_host;
}

void WebContentsDelegate::FindReply(TabContents* tab,
                                    int request_id,
                                    int number_of_matches,
                                    const gfx::Rect& selection_rect,
                                    int active_match_ordinal,
                                    bool final_update) {
}

void WebContentsDelegate::CrashedPlugin(TabContents* tab,
                                        const FilePath& plugin_path) {
}

void WebContentsDelegate::UpdatePreferredSize(TabContents* tab,
                                              const gfx::Size& pref_size) {
}

void WebContentsDelegate::WebUISend(TabContents* tab,
                                    const GURL& source_url,
                                    const std::string& name,
                                    const base::ListValue& args) {
}

void WebContentsDelegate::RequestToLockMouse(TabContents* tab) {
}

void WebContentsDelegate::LostMouseLock() {
}

WebContentsDelegate::~WebContentsDelegate() {
  while (!attached_contents_.empty()) {
    TabContents* tab_contents = *attached_contents_.begin();
    tab_contents->SetDelegate(NULL);
  }
  DCHECK(attached_contents_.empty());
  content::NotificationService::current()->Notify(
      content::NOTIFICATION_TAB_CONTENTS_DELEGATE_DESTROYED,
      content::Source<WebContentsDelegate>(this),
      content::NotificationService::NoDetails());
}

void WebContentsDelegate::Attach(TabContents* tab_contents) {
  DCHECK(attached_contents_.find(tab_contents) == attached_contents_.end());
  attached_contents_.insert(tab_contents);
}

void WebContentsDelegate::Detach(TabContents* tab_contents) {
  DCHECK(attached_contents_.find(tab_contents) != attached_contents_.end());
  attached_contents_.erase(tab_contents);
}

}  // namespace content
