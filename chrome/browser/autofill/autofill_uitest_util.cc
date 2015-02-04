// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_uitest_util.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "content/public/test/test_utils.h"

namespace autofill {

// This class is used to wait for asynchronous updates to PersonalDataManager
// to complete.
class PdmChangeWaiter
    : public PersonalDataManagerObserver,
      public infobars::InfoBarManager::Observer {
 public:
  explicit PdmChangeWaiter(Browser* browser)
      : alerted_(false),
        has_run_message_loop_(false),
        browser_(browser),
        infobar_service_(InfoBarService::FromWebContents(
            browser_->tab_strip_model()->GetActiveWebContents())) {
    PersonalDataManagerFactory::GetForProfile(browser_->profile())->
        AddObserver(this);
    infobar_service_->AddObserver(this);
  }

  ~PdmChangeWaiter() override {
    while (infobar_service_->infobar_count() > 0) {
      infobar_service_->RemoveInfoBar(infobar_service_->infobar_at(0));
    }
    infobar_service_->RemoveObserver(this);
  }

  // PersonalDataManagerObserver:
  void OnPersonalDataChanged() override {
    if (has_run_message_loop_) {
      base::MessageLoopForUI::current()->Quit();
      has_run_message_loop_ = false;
    }
    alerted_ = true;
  }

  void OnInsufficientFormData() override { OnPersonalDataChanged(); }


  void Wait() {
    if (!alerted_) {
      has_run_message_loop_ = true;
      content::RunMessageLoop();
    }
    PersonalDataManagerFactory::GetForProfile(browser_->profile())->
        RemoveObserver(this);
  }

 private:
  // infobars::InfoBarManager::Observer:
  void OnInfoBarAdded(infobars::InfoBar* infobar) override {
    infobar_service_->infobar_at(0)->delegate()->AsConfirmInfoBarDelegate()->
        Accept();
  }

  bool alerted_;
  bool has_run_message_loop_;
  Browser* browser_;
  InfoBarService* infobar_service_;

  DISALLOW_COPY_AND_ASSIGN(PdmChangeWaiter);
};

static PersonalDataManager* GetPersonalDataManager(Profile* profile) {
  return PersonalDataManagerFactory::GetForProfile(profile);
}

void AddTestProfile(Browser* browser, const AutofillProfile& profile) {
    PdmChangeWaiter observer(browser);
  GetPersonalDataManager(browser->profile())->AddProfile(profile);

  // AddProfile is asynchronous. Wait for it to finish before continuing the
  // tests.
  observer.Wait();
}

void SetTestProfile(Browser* browser, const AutofillProfile& profile) {
  std::vector<AutofillProfile> profiles;
  profiles.push_back(profile);
  SetTestProfiles(browser, &profiles);
}

void SetTestProfiles(Browser* browser, std::vector<AutofillProfile>* profiles) {
  PdmChangeWaiter observer(browser);
  GetPersonalDataManager(browser->profile())->SetProfiles(profiles);
  observer.Wait();
}

}  // namespace autofill
