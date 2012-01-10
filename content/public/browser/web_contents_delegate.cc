// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/web_contents_delegate.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "content/browser/javascript_dialogs.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_intents_dispatcher.h"
#include "content/public/common/url_constants.h"
#include "ui/gfx/rect.h"
#include "webkit/glue/web_intent_data.h"

namespace content {

WebContentsDelegate::WebContentsDelegate() {
}

WebContents* WebContentsDelegate::OpenURLFromTab(WebContents* source,
                                                 const OpenURLParams& params) {
  return NULL;
}

bool WebContentsDelegate::IsPopupOrPanel(const WebContents* source) const {
  return false;
}

bool WebContentsDelegate::IsApplication() const { return false; }

bool WebContentsDelegate::CanReloadContents(WebContents* source) const {
  return true;
}

bool WebContentsDelegate::ShouldSuppressDialogs() {
  return false;
}

void WebContentsDelegate::BeforeUnloadFired(WebContents* tab,
                                            bool proceed,
                                            bool* proceed_to_fire_unload) {
  *proceed_to_fire_unload = true;
}

bool WebContentsDelegate::ShouldFocusPageAfterCrash() {
  return true;
}

bool WebContentsDelegate::TakeFocus(bool reverse) {
  return false;
}

int WebContentsDelegate::GetExtraRenderViewHeight() const {
  return 0;
}

bool WebContentsDelegate::CanDownload(WebContents* source, int request_id) {
  return true;
}

bool WebContentsDelegate::HandleContextMenu(const ContextMenuParams& params) {
  return false;
}

bool WebContentsDelegate::ExecuteContextMenuCommand(int command) {
  return false;
}

void WebContentsDelegate::ViewSourceForTab(WebContents* source,
                                           const GURL& page_url) {
  // Fall back implementation based entirely on the view-source scheme.
  // It suffers from http://crbug.com/523 and that is why browser overrides
  // it with proper implementation.
  GURL url = GURL(chrome::kViewSourceScheme + std::string(":") +
                      page_url.spec());
  OpenURLFromTab(source, OpenURLParams(url, Referrer(),
                                       NEW_FOREGROUND_TAB,
                                       PAGE_TRANSITION_LINK, false));
}

void WebContentsDelegate::ViewSourceForFrame(WebContents* source,
                                             const GURL& frame_url,
                                             const std::string& content_state) {
  // Same as ViewSourceForTab, but for given subframe.
  GURL url = GURL(chrome::kViewSourceScheme + std::string(":") +
                      frame_url.spec());
  OpenURLFromTab(source, OpenURLParams(url, Referrer(),
                                       NEW_FOREGROUND_TAB,
                                       PAGE_TRANSITION_LINK, false));
}

bool WebContentsDelegate::PreHandleKeyboardEvent(
    const NativeWebKeyboardEvent& event,
    bool* is_keyboard_shortcut) {
  return false;
}

bool WebContentsDelegate::OnGoToEntryOffset(int offset) {
  return true;
}

bool WebContentsDelegate::ShouldAddNavigationToHistory(
    const history::HistoryAddPageArgs& add_page_args,
    NavigationType navigation_type) {
  return true;
}

gfx::NativeWindow WebContentsDelegate::GetFrameNativeWindow() {
  return NULL;
}

// A stubbed-out version of JavaScriptDialogCreator that doesn't do anything.
class JavaScriptDialogCreatorStub : public JavaScriptDialogCreator {
 public:
  static JavaScriptDialogCreatorStub* GetInstance() {
    return Singleton<JavaScriptDialogCreatorStub>::get();
  }

  virtual void RunJavaScriptDialog(
      JavaScriptDialogDelegate* delegate,
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
      JavaScriptDialogDelegate* delegate,
      const string16& message_text,
      IPC::Message* reply_message) OVERRIDE {
    delegate->OnDialogClosed(reply_message, true, string16());
  }

  virtual void ResetJavaScriptState(
      JavaScriptDialogDelegate* delegate) OVERRIDE {
  }
 private:
  friend struct DefaultSingletonTraits<JavaScriptDialogCreatorStub>;
};

JavaScriptDialogCreator* WebContentsDelegate::GetJavaScriptDialogCreator() {
  return JavaScriptDialogCreatorStub::GetInstance();
}

bool WebContentsDelegate::IsFullscreenForTab(const WebContents* tab) const {
  return false;
}

void WebContentsDelegate::WebIntentDispatch(
    WebContents* tab,
    WebIntentsDispatcher* intents_dispatcher) {
  // The caller passes this method ownership of the |intents_dispatcher|, but
  // this empty implementation will not use it, so we delete it immediately.
  delete intents_dispatcher;
}

WebContentsDelegate::~WebContentsDelegate() {
  while (!attached_contents_.empty()) {
    WebContents* web_contents = *attached_contents_.begin();
    web_contents->SetDelegate(NULL);
  }
  DCHECK(attached_contents_.empty());
}

void WebContentsDelegate::Attach(WebContents* web_contents) {
  DCHECK(attached_contents_.find(web_contents) == attached_contents_.end());
  attached_contents_.insert(web_contents);
}

void WebContentsDelegate::Detach(WebContents* web_contents) {
  DCHECK(attached_contents_.find(web_contents) != attached_contents_.end());
  attached_contents_.erase(web_contents);
}

}  // namespace content
