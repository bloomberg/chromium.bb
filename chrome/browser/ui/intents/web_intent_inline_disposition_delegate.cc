// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/intents/web_intent_inline_disposition_delegate.h"

#include "base/logging.h"
#include "chrome/browser/ui/intents/web_intent_picker.h"
#include "chrome/common/extensions/extension_messages.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/renderer_preferences.h"
#include "ipc/ipc_message_macros.h"

WebIntentInlineDispositionDelegate::WebIntentInlineDispositionDelegate(
    WebIntentPicker* picker,
    content::WebContents* contents,
    Profile* profile)
    : picker_(picker),
      web_contents_(contents),
      ALLOW_THIS_IN_INITIALIZER_LIST(
        extension_function_dispatcher_(profile, this)) {
  content::WebContentsObserver::Observe(web_contents_);
  web_contents_->SetDelegate(this);
  // TODO(groby): Technically, allowing the browser to hande all requests
  // should work just fine. Practically, we're getting a cross-origin warning
  // for a googleapis request.
  // Investigate why OpenURLFromTab delegate misfires on this.
  // web_contents_->GetMutableRendererPrefs()->browser_handles_all_requests =
  //    true;
}

WebIntentInlineDispositionDelegate::~WebIntentInlineDispositionDelegate() {
}

bool WebIntentInlineDispositionDelegate::IsPopupOrPanel(
    const content::WebContents* source) const {
  return true;
}

bool WebIntentInlineDispositionDelegate::ShouldAddNavigationToHistory(
    const history::HistoryAddPageArgs& add_page_args,
    content::NavigationType navigation_type) {
  return false;
}

content::WebContents* WebIntentInlineDispositionDelegate::OpenURLFromTab(
    content::WebContents* source, const content::OpenURLParams& params) {
  DCHECK(source);  // Can only be invoked from inline disposition.

  // Load in place.
  source->GetController().LoadURL(params.url, content::Referrer(),
      content::PAGE_TRANSITION_START_PAGE, std::string());

  // Remove previous history entries - users should not navigate in intents.
  source->GetController().PruneAllButActive();

  return source;
}

void WebIntentInlineDispositionDelegate::LoadingStateChanged(
    content::WebContents* source) {
  if (!source->IsLoading())
    picker_->OnInlineDispositionWebContentsLoaded(source);
}

bool WebIntentInlineDispositionDelegate::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WebIntentInlineDispositionDelegate, message)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_Request, OnRequest)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

content::WebContents* WebIntentInlineDispositionDelegate::
    GetAssociatedWebContents() const {
  return NULL;
}

ExtensionWindowController* WebIntentInlineDispositionDelegate::
    GetExtensionWindowController() const {
  return NULL;
}

void WebIntentInlineDispositionDelegate::OnRequest(
    const ExtensionHostMsg_Request_Params& params) {
  extension_function_dispatcher_.Dispatch(params,
                                          web_contents_->GetRenderViewHost());
}
