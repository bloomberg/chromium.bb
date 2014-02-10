// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_CONTENT_PASSWORD_MANAGER_DRIVER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_CONTENT_PASSWORD_MANAGER_DRIVER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/password_manager/password_generation_manager.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/password_manager/password_manager_driver.h"
#include "content/public/browser/web_contents_observer.h"

namespace autofill {
class AutofillManager;
struct PasswordForm;
}

namespace content {
class WebContents;
}

class ContentPasswordManagerDriver : public PasswordManagerDriver,
                                     public content::WebContentsObserver {
 public:
  explicit ContentPasswordManagerDriver(content::WebContents* web_contents,
                                        PasswordManagerClient* client);
  virtual ~ContentPasswordManagerDriver();

  // PasswordManagerDriver implementation.
  virtual void FillPasswordForm(const autofill::PasswordFormFillData& form_data)
      OVERRIDE;
  virtual bool DidLastPageLoadEncounterSSLErrors() OVERRIDE;
  virtual bool IsOffTheRecord() OVERRIDE;
  virtual PasswordGenerationManager* GetPasswordGenerationManager() OVERRIDE;
  virtual PasswordManager* GetPasswordManager() OVERRIDE;
  virtual autofill::AutofillManager* GetAutofillManager() OVERRIDE;
  virtual void AllowPasswordGenerationForForm(autofill::PasswordForm* form)
      OVERRIDE;

  // content::WebContentsObserver overrides.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;

 private:
  // Must outlive this instance.
  PasswordManagerClient* client_;

  PasswordManager password_manager_;
  PasswordGenerationManager password_generation_manager_;

  DISALLOW_COPY_AND_ASSIGN(ContentPasswordManagerDriver);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_CONTENT_PASSWORD_MANAGER_DRIVER_H_
