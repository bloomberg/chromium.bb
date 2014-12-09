// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/worker_frame/worker_frame_guest.h"

#include "content/public/common/url_constants.h"
#include "extensions/browser/guest_view/guest_view_manager.h"
#include "extensions/browser/guest_view/worker_frame/worker_frame_constants.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "ipc/ipc_message_macros.h"

using content::RenderFrameHost;
using content::WebContents;

namespace extensions {

// static.
const char WorkerFrameGuest::Type[] = "workerframe";

// static
GuestViewBase* WorkerFrameGuest::Create(
    content::BrowserContext* browser_context,
    content::WebContents* owner_web_contents,
    int guest_instance_id) {
  return new WorkerFrameGuest(browser_context,
                              owner_web_contents,
                              guest_instance_id);
}

WorkerFrameGuest::WorkerFrameGuest(
    content::BrowserContext* browser_context,
    content::WebContents* owner_web_contents,
    int guest_instance_id)
    : GuestView<WorkerFrameGuest>(browser_context,
                                        owner_web_contents,
                                        guest_instance_id),
      weak_ptr_factory_(this) {
}

WorkerFrameGuest::~WorkerFrameGuest() {
}

bool WorkerFrameGuest::HandleContextMenu(
    const content::ContextMenuParams& params) {
  return false;
}

const char* WorkerFrameGuest::GetAPINamespace() const {
  return worker_frame::kEmbedderAPINamespace;
}

int WorkerFrameGuest::GetTaskPrefix() const {
  return IDS_EXTENSION_TASK_MANAGER_WORKER_FRAME_TAG_PREFIX;
}

void WorkerFrameGuest::CreateWebContents(
    const base::DictionaryValue& create_params,
    const WebContentsCreatedCallback& callback) {
  std::string url;
  if (!create_params.GetString(worker_frame::kURL, &url)) {
    callback.Run(NULL);
    return;
  }

  url_ = GURL(url);
  if (!url_.is_valid()) {
    callback.Run(NULL);
    return;
  }

  GURL guest_site(base::StringPrintf("%s://wtf-%s",
                                     content::kGuestScheme,
                                     GetOwnerSiteURL().host().c_str()));

  GuestViewManager* guest_view_manager =
      GuestViewManager::FromBrowserContext(
          owner_web_contents()->GetBrowserContext());
  content::SiteInstance* guest_site_instance =
      guest_view_manager->GetGuestSiteInstance(guest_site);
  WebContents::CreateParams params(
      owner_web_contents()->GetBrowserContext(),
      guest_site_instance);
  params.guest_delegate = this;
  callback.Run(WebContents::Create(params));
}

void WorkerFrameGuest::DidAttachToEmbedder() {
  web_contents()->GetController().LoadURL(
      url_, content::Referrer(), ui::PAGE_TRANSITION_LINK, std::string());
  url_ = GURL();
}

}  // namespace extensions
