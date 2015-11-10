// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_CONTENT_PASSWORD_MANAGER_DRIVER_FACTORY_H_
#define COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_CONTENT_PASSWORD_MANAGER_DRIVER_FACTORY_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/containers/scoped_ptr_map.h"
#include "base/supports_user_data.h"
#include "components/password_manager/core/browser/password_autofill_manager.h"
#include "components/password_manager/core/browser/password_generation_manager.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "content/public/browser/web_contents_observer.h"

namespace autofill {
class AutofillManager;
struct PasswordForm;
}

namespace content {
class WebContents;
}

namespace password_manager {

class ContentPasswordManagerDriver;

// Creates and owns ContentPasswordManagerDrivers. There is one
// factory per WebContents, and one driver per render frame.
class ContentPasswordManagerDriverFactory
    : public content::WebContentsObserver,
      public base::SupportsUserData::Data {
 public:
  static void CreateForWebContents(content::WebContents* web_contents,
                                   PasswordManagerClient* client,
                                   autofill::AutofillClient* autofill_client);
  ~ContentPasswordManagerDriverFactory() override;

  static ContentPasswordManagerDriverFactory* FromWebContents(
      content::WebContents* web_contents);

  ContentPasswordManagerDriver* GetDriverForFrame(
      content::RenderFrameHost* render_frame_host);

  void TestingSetDriverForFrame(
      content::RenderFrameHost* render_frame_host,
      scoped_ptr<ContentPasswordManagerDriver> driver);

  // content::WebContentsObserver:
  bool OnMessageReceived(const IPC::Message& message,
                         content::RenderFrameHost* render_frame_host) override;
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void DidNavigateAnyFrame(content::RenderFrameHost* render_frame_host,
                           const content::LoadCommittedDetails& details,
                           const content::FrameNavigateParams& params) override;

 private:
  ContentPasswordManagerDriverFactory(
      content::WebContents* web_contents,
      PasswordManagerClient* client,
      autofill::AutofillClient* autofill_client);

  void CreateDriverForFrame(content::RenderFrameHost* render_frame_host);

  base::ScopedPtrMap<content::RenderFrameHost*,
                     scoped_ptr<ContentPasswordManagerDriver>>
      frame_driver_map_;

  PasswordManagerClient* password_client_;
  autofill::AutofillClient* autofill_client_;

  DISALLOW_COPY_AND_ASSIGN(ContentPasswordManagerDriverFactory);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_CONTENT_PASSWORD_MANAGER_DRIVER_FACTORY_H_
