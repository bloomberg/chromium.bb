// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_nsobject.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#include "chrome/browser/cocoa/cocoa_test_helper.h"
#include "chrome/browser/cocoa/extensions/extension_popup_controller.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/test/testing_profile.h"

namespace {

class ExtensionTestingProfile : public TestingProfile {
 public:
  ExtensionTestingProfile() {}

  FilePath GetExtensionsInstallDir() {
    return GetPath().AppendASCII(ExtensionsService::kInstallDirectoryName);
  }

  void InitExtensionProfile() {
    DCHECK(!GetExtensionProcessManager());
    DCHECK(!GetExtensionsService());

    manager_.reset(new ExtensionProcessManager(this));
    service_ = new ExtensionsService(this,
                                     CommandLine::ForCurrentProcess(),
                                     GetPrefs(),
                                     GetExtensionsInstallDir(),
                                     false);
    service_->set_extensions_enabled(true);
    service_->set_show_extensions_prompts(false);
    service_->ClearProvidersForTesting();
    service_->Init();
  }

  void ShutdownExtensionProfile() {
    manager_.reset();
    service_ = NULL;
  }

  virtual ExtensionProcessManager* GetExtensionProcessManager() {
    return manager_.get();
  }

  virtual ExtensionsService* GetExtensionsService() {
    return service_.get();
  }

 private:
  scoped_ptr<ExtensionProcessManager> manager_;
  scoped_refptr<ExtensionsService> service_;

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
                        arrowLocation:kTopRight];
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
