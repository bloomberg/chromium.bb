// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/browser/content_password_manager_driver_factory.h"

#include "base/bind.h"
#include "base/stl_util.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/content/common/autofill_messages.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/ssl_status.h"
#include "ipc/ipc_message_macros.h"
#include "net/cert/cert_status_flags.h"

namespace password_manager {

namespace {

const char kContentPasswordManagerDriverFactoryWebContentsUserDataKey[] =
    "web_contents_password_manager_driver_factory";

}  // namespace

void ContentPasswordManagerDriverFactory::CreateForWebContents(
    content::WebContents* web_contents,
    PasswordManagerClient* password_client,
    autofill::AutofillClient* autofill_client) {
  if (FromWebContents(web_contents))
    return;

  web_contents->SetUserData(
      kContentPasswordManagerDriverFactoryWebContentsUserDataKey,
      new ContentPasswordManagerDriverFactory(web_contents, password_client,
                                              autofill_client));
}

ContentPasswordManagerDriverFactory::ContentPasswordManagerDriverFactory(
    content::WebContents* web_contents,
    PasswordManagerClient* password_client,
    autofill::AutofillClient* autofill_client)
    : content::WebContentsObserver(web_contents),
      password_client_(password_client),
      autofill_client_(autofill_client) {
  web_contents->ForEachFrame(
      base::Bind(&ContentPasswordManagerDriverFactory::CreateDriverForFrame,
                 base::Unretained(this)));
}

ContentPasswordManagerDriverFactory::~ContentPasswordManagerDriverFactory() {
  STLDeleteContainerPairSecondPointers(frame_driver_map_.begin(),
                                       frame_driver_map_.end());
  frame_driver_map_.clear();
}

// static
ContentPasswordManagerDriverFactory*
ContentPasswordManagerDriverFactory::FromWebContents(
    content::WebContents* contents) {
  return static_cast<ContentPasswordManagerDriverFactory*>(
      contents->GetUserData(
          kContentPasswordManagerDriverFactoryWebContentsUserDataKey));
}

ContentPasswordManagerDriver*
ContentPasswordManagerDriverFactory::GetDriverForFrame(
    content::RenderFrameHost* render_frame_host) {
  auto mapping = frame_driver_map_.find(render_frame_host);
  return mapping == frame_driver_map_.end() ? nullptr : mapping->second;
}

void ContentPasswordManagerDriverFactory::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  // This is called twice for the main frame.
  if (!frame_driver_map_[render_frame_host])
    CreateDriverForFrame(render_frame_host);
}

void ContentPasswordManagerDriverFactory::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  delete frame_driver_map_[render_frame_host];
  frame_driver_map_.erase(render_frame_host);
}

bool ContentPasswordManagerDriverFactory::OnMessageReceived(
    const IPC::Message& message,
    content::RenderFrameHost* render_frame_host) {
  return frame_driver_map_[render_frame_host]->HandleMessage(message);
}

void ContentPasswordManagerDriverFactory::DidNavigateAnyFrame(
    content::RenderFrameHost* render_frame_host,
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  frame_driver_map_[render_frame_host]->DidNavigateFrame(details, params);
}

void ContentPasswordManagerDriverFactory::CreateDriverForFrame(
    content::RenderFrameHost* render_frame_host) {
  DCHECK(!frame_driver_map_[render_frame_host]);
  frame_driver_map_[render_frame_host] = new ContentPasswordManagerDriver(
      render_frame_host, password_client_, autofill_client_);
}

void ContentPasswordManagerDriverFactory::TestingSetDriverForFrame(
    content::RenderFrameHost* render_frame_host,
    scoped_ptr<ContentPasswordManagerDriver> driver) {
  if (frame_driver_map_[render_frame_host])
    delete frame_driver_map_[render_frame_host];
  frame_driver_map_[render_frame_host] = driver.release();
}

}  // namespace password_manager
