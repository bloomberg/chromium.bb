// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/extension_view/extension_view_guest.h"

#include "base/metrics/user_metrics.h"
#include "base/strings/string_util.h"
#include "components/crx_file/id_util.h"
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

ExtensionViewGuest::ExtensionViewGuest(
    content::WebContents* owner_web_contents)
    : GuestView<ExtensionViewGuest>(owner_web_contents),
      extension_view_guest_delegate_(
          extensions::ExtensionsAPIClient::Get()
              ->CreateExtensionViewGuestDelegate(this)) {
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
  GURL url = extension_url_.Resolve(src);

  // If the URL is not valid, about:blank, or the same origin as the extension,
  // then navigate to about:blank.
  bool url_not_allowed = (url != GURL(url::kAboutBlankURL)) &&
      (url.GetOrigin() != extension_url_.GetOrigin());
  if (!url.is_valid() || url_not_allowed) {
    NavigateGuest(url::kAboutBlankURL, true /* force_navigation */);
    return;
  }

  if (!force_navigation && (view_page_ == url))
    return;

  web_contents()->GetRenderProcessHost()->FilterURL(false, &url);
  web_contents()->GetController().LoadURL(url, content::Referrer(),
                                          ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                          std::string());

  view_page_ = url;
}

// GuestViewBase implementation.
bool ExtensionViewGuest::CanRunInDetachedState() const {
  return true;
}

void ExtensionViewGuest::CreateWebContents(
    const base::DictionaryValue& create_params,
    const WebContentsCreatedCallback& callback) {
  // Gets the extension ID.
  std::string extension_id;
  create_params.GetString(extensionview::kAttributeExtension, &extension_id);

  if (!crx_file::id_util::IdIsValid(extension_id)) {
    callback.Run(nullptr);
    return;
  }

  // Gets the extension URL.
  extension_url_ =
      extensions::Extension::GetBaseURLFromExtensionId(extension_id);

  if (!extension_url_.is_valid()) {
    callback.Run(nullptr);
    return;
  }

  content::SiteInstance* view_site_instance =
      content::SiteInstance::CreateForURL(browser_context(),
                                          extension_url_);

  WebContents::CreateParams params(browser_context(), view_site_instance);
  params.guest_delegate = this;
  callback.Run(WebContents::Create(params));
}

void ExtensionViewGuest::DidInitialize(
    const base::DictionaryValue& create_params) {
  extension_function_dispatcher_.reset(
      new extensions::ExtensionFunctionDispatcher(browser_context(), this));

  if (extension_view_guest_delegate_)
    extension_view_guest_delegate_->DidInitialize();

  ApplyAttributes(create_params);
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
void ExtensionViewGuest::DidCommitProvisionalLoadForFrame(
    content::RenderFrameHost* render_frame_host,
    const GURL& url,
    ui::PageTransition transition_type) {
  if (render_frame_host->GetParent())
    return;

  view_page_ = url;

  // Gets chrome-extension://extensionid/ prefix.
  std::string prefix = url.GetWithEmptyPath().spec();
  std::string relative_url = url.spec();

  // Removes the prefix.
  ReplaceFirstSubstringAfterOffset(&relative_url, 0, prefix, "");

  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetString(guestview::kUrl, relative_url);
  DispatchEventToView(
      new GuestViewBase::Event(extensionview::kEventLoadCommit, args.Pass()));
}

void ExtensionViewGuest::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  if (attached() && (params.url.GetOrigin() != view_page_.GetOrigin())) {
    base::RecordAction(base::UserMetricsAction("BadMessageTerminate_EVG"));
    web_contents()->GetRenderProcessHost()->Shutdown(
        content::RESULT_CODE_KILLED_BAD_MESSAGE, false /* wait */);
  }
}

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
