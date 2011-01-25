// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/scoped_nsobject.h"
#include "chrome/browser/extensions/extension_pref_value_map.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/cocoa/browser_test_helper.h"
#include "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "chrome/browser/ui/cocoa/extensions/extension_popup_controller.h"
#include "chrome/test/testing_profile.h"

namespace {

class ExtensionTestingProfile : public TestingProfile {
 public:
  ExtensionTestingProfile() {}

  FilePath GetExtensionsInstallDir() {
    return GetPath().AppendASCII(ExtensionService::kInstallDirectoryName);
  }

  void InitExtensionProfile() {
    DCHECK(!GetExtensionProcessManager());
    DCHECK(!GetExtensionService());

    manager_.reset(ExtensionProcessManager::Create(this));
    extension_pref_value_map_.reset(new ExtensionPrefValueMap);
    extension_prefs_.reset(new ExtensionPrefs(GetPrefs(),
                                              GetExtensionsInstallDir(),
                                              extension_pref_value_map_.get()));
    service_ = new ExtensionService(this,
                                     CommandLine::ForCurrentProcess(),
                                     GetExtensionsInstallDir(),
                                     extension_prefs_.get(),
                                     false);
    service_->set_extensions_enabled(true);
    service_->set_show_extensions_prompts(false);
    service_->ClearProvidersForTesting();
    service_->Init();
  }

  void ShutdownExtensionProfile() {
    manager_.reset();
    service_ = NULL;
    extension_prefs_.reset();
  }

  virtual ExtensionProcessManager* GetExtensionProcessManager() {
    return manager_.get();
  }

  virtual ExtensionService* GetExtensionService() {
    return service_.get();
  }

 private:
  scoped_ptr<ExtensionProcessManager> manager_;
  scoped_ptr<ExtensionPrefs> extension_prefs_;
  scoped_refptr<ExtensionService> service_;
  scoped_ptr<ExtensionPrefValueMap> extension_pref_value_map_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionTestingProfile);
};

class ExtensionPopupControllerTest : public CocoaTest {
 public:
  virtual void SetUp() {
    CocoaTest::SetUp();
    profile_.reset(new ExtensionTestingProfile());
    profile_->InitExtensionProfile();
    browser_.reset(new Browser(Browser::TYPE_NORMAL, profile_.get()));
    [ExtensionPopupController showURL:GURL("http://google.com")
                            inBrowser:browser_.get()
                           anchoredAt:NSZeroPoint
                        arrowLocation:info_bubble::kTopRight
                              devMode:NO];
  }
  virtual void TearDown() {
    profile_->ShutdownExtensionProfile();
    [[ExtensionPopupController popup] close];
    CocoaTest::TearDown();
  }

 protected:
  scoped_ptr<Browser> browser_;
  scoped_ptr<ExtensionTestingProfile> profile_;
};

TEST_F(ExtensionPopupControllerTest, DISABLED_Basics) {
  // TODO(andybons): Better mechanisms for mocking out the extensions service
  // and extensions for easy testing need to be implemented.
  // http://crbug.com/28316
  EXPECT_TRUE([ExtensionPopupController popup]);
}

}  // namespace
