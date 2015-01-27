// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/surface_worker/surface_worker_guest.h"

#include "content/public/common/url_constants.h"
#include "extensions/browser/guest_view/guest_view_manager.h"
#include "extensions/browser/guest_view/surface_worker/surface_worker_constants.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "ipc/ipc_message_macros.h"

using content::RenderFrameHost;
using content::WebContents;

namespace extensions {

// static.
const char SurfaceWorkerGuest::Type[] = "surfaceview";

// static
GuestViewBase* SurfaceWorkerGuest::Create(
    content::WebContents* owner_web_contents) {
  return new SurfaceWorkerGuest(owner_web_contents);
}

SurfaceWorkerGuest::SurfaceWorkerGuest(
    content::WebContents* owner_web_contents)
    : GuestView<SurfaceWorkerGuest>(owner_web_contents),
      weak_ptr_factory_(this) {
}

SurfaceWorkerGuest::~SurfaceWorkerGuest() {
}

bool SurfaceWorkerGuest::HandleContextMenu(
    const content::ContextMenuParams& params) {
  return false;
}

const char* SurfaceWorkerGuest::GetAPINamespace() const {
  return surface_worker::kEmbedderAPINamespace;
}

int SurfaceWorkerGuest::GetTaskPrefix() const {
  return IDS_EXTENSION_TASK_MANAGER_SURFACEWORKER_TAG_PREFIX;
}

void SurfaceWorkerGuest::CreateWebContents(
    const base::DictionaryValue& create_params,
    const WebContentsCreatedCallback& callback) {
  std::string url;
  if (!create_params.GetString(surface_worker::kURL, &url)) {
    callback.Run(NULL);
    return;
  }

  url_ = GURL(url);
  if (!url_.is_valid()) {
    callback.Run(NULL);
    return;
  }

  GURL guest_site(base::StringPrintf("%s://surface-%s",
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

void SurfaceWorkerGuest::DidAttachToEmbedder() {
  web_contents()->GetController().LoadURL(
      url_, content::Referrer(), ui::PAGE_TRANSITION_LINK, std::string());
  url_ = GURL();
}

}  // namespace extensions
