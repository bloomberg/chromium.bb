// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/extension_view/extension_view_guest.h"

#include "base/metrics/user_metrics.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/result_codes.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/guest_view/extension_view/extension_view_constants.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_messages.h"
#include "extensions/strings/grit/extensions_strings.h"

using content::WebContents;
using namespace extensions::core_api;

namespace extensions {

// static
const char ExtensionViewGuest::Type[] = "extensionview";

ExtensionViewGuest::ExtensionViewGuest(content::WebContents* owner_web_contents)
    : GuestView<ExtensionViewGuest>(owner_web_contents) {
}

ExtensionViewGuest::~ExtensionViewGuest() {
}

// static
extensions::GuestViewBase* ExtensionViewGuest::Create(
    content::WebContents* owner_web_contents) {
  return new ExtensionViewGuest(owner_web_contents);
}

void ExtensionViewGuest::NavigateGuest(const std::string& src,
                                       bool force_navigation) {
  if (src.empty())
    return;

  GURL url(src);
  if (!url.is_valid() && !force_navigation && (url == view_page_))
    return;

  web_contents()->GetRenderProcessHost()->FilterURL(false, &url);
  web_contents()->GetController().LoadURL(url, content::Referrer(),
                                          ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                          std::string());

  view_page_ = url;
}

// GuestViewBase implementation.
void ExtensionViewGuest::CreateWebContents(
    const base::DictionaryValue& create_params,
    const WebContentsCreatedCallback& callback) {
  std::string str;
  if (!create_params.GetString(extensionview::kAttributeSrc, &str)) {
    callback.Run(nullptr);
    return;
  }

  GURL source(str);
  if (!source.is_valid()) {
    callback.Run(nullptr);
    return;
  }

  content::SiteInstance* view_site_instance =
      content::SiteInstance::CreateForURL(browser_context(), source);

  WebContents::CreateParams params(browser_context(), view_site_instance);
  params.guest_delegate = this;
  callback.Run(WebContents::Create(params));
}

void ExtensionViewGuest::DidAttachToEmbedder() {
  ApplyAttributes(*attach_params());
}

const char* ExtensionViewGuest::GetAPINamespace() const {
  return extensionview::kAPINamespace;
}

int ExtensionViewGuest::GetTaskPrefix() const {
  return IDS_EXTENSION_TASK_MANAGER_EXTENSIONVIEW_TAG_PREFIX;
}

// content::WebContentsObserver implementation.
bool ExtensionViewGuest::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ExtensionViewGuest, message)
  IPC_MESSAGE_HANDLER(ExtensionHostMsg_Request, OnRequest)
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

// Private
void ExtensionViewGuest::OnRequest(
    const ExtensionHostMsg_Request_Params& params) {
  extension_function_dispatcher_->Dispatch(params,
                                           web_contents()->GetRenderViewHost());
}

void ExtensionViewGuest::ApplyAttributes(const base::DictionaryValue& params) {
  std::string src;
  params.GetString(extensionview::kAttributeSrc, &src);
  NavigateGuest(src, false /* force_navigation */);
}

}  // namespace extensions
