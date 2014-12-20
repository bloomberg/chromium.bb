// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/content_autofill_driver_factory.h"

#include "base/bind.h"
#include "base/stl_util.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ipc/ipc_message_macros.h"

namespace autofill {

const char ContentAutofillDriverFactory::
    kContentAutofillDriverFactoryWebContentsUserDataKey[] =
        "web_contents_autofill_driver_factory";

// static
void ContentAutofillDriverFactory::CreateForWebContentsAndDelegate(
    content::WebContents* contents,
    AutofillClient* client,
    const std::string& app_locale,
    AutofillManager::AutofillDownloadManagerState enable_download_manager) {
  if (FromWebContents(contents))
    return;

  contents->SetUserData(
      kContentAutofillDriverFactoryWebContentsUserDataKey,
      new ContentAutofillDriverFactory(contents, client, app_locale,
                                       enable_download_manager));
}

// static
ContentAutofillDriverFactory* ContentAutofillDriverFactory::FromWebContents(
    content::WebContents* contents) {
  return static_cast<ContentAutofillDriverFactory*>(contents->GetUserData(
      kContentAutofillDriverFactoryWebContentsUserDataKey));
}

ContentAutofillDriverFactory::ContentAutofillDriverFactory(
    content::WebContents* web_contents,
    AutofillClient* client,
    const std::string& app_locale,
    AutofillManager::AutofillDownloadManagerState enable_download_manager)
    : content::WebContentsObserver(web_contents),
      client_(client),
      app_locale_(app_locale),
      enable_download_manager_(enable_download_manager) {
  web_contents->ForEachFrame(
      base::Bind(&ContentAutofillDriverFactory::CreateDriverForFrame,
                 base::Unretained(this)));
}

ContentAutofillDriverFactory::~ContentAutofillDriverFactory() {
  STLDeleteContainerPairSecondPointers(frame_driver_map_.begin(),
                                       frame_driver_map_.end());
  frame_driver_map_.clear();
}

ContentAutofillDriver* ContentAutofillDriverFactory::DriverForFrame(
    content::RenderFrameHost* render_frame_host) {
  auto mapping = frame_driver_map_.find(render_frame_host);
  return mapping == frame_driver_map_.end() ? nullptr : mapping->second;
}

bool ContentAutofillDriverFactory::OnMessageReceived(
    const IPC::Message& message,
    content::RenderFrameHost* render_frame_host) {
  return frame_driver_map_[render_frame_host]->HandleMessage(message);
}

void ContentAutofillDriverFactory::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  // RenderFrameCreated is called more than once for the main frame.
  if (!frame_driver_map_[render_frame_host])
    CreateDriverForFrame(render_frame_host);
}

void ContentAutofillDriverFactory::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  delete frame_driver_map_[render_frame_host];
  frame_driver_map_.erase(render_frame_host);
}

void ContentAutofillDriverFactory::DidNavigateAnyFrame(
    content::RenderFrameHost* rfh,
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  frame_driver_map_[rfh]->DidNavigateFrame(details, params);
}

void ContentAutofillDriverFactory::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  client_->HideAutofillPopup();
}

void ContentAutofillDriverFactory::WasHidden() {
  client_->HideAutofillPopup();
}

void ContentAutofillDriverFactory::CreateDriverForFrame(
    content::RenderFrameHost* render_frame_host) {
  DCHECK(!frame_driver_map_[render_frame_host]);
  frame_driver_map_[render_frame_host] = new ContentAutofillDriver(
      render_frame_host, client_, app_locale_, enable_download_manager_);
}

}  // namespace autofill
