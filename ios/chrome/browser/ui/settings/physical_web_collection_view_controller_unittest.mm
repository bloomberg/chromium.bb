// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/physical_web_collection_view_controller.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/pref_service_mock_factory.h"
#include "ios/chrome/browser/physical_web/physical_web_constants.h"
#include "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_footer_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_switch_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_controller_test.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"

@interface PhysicalWebCollectionViewController (ExposedForTesting)
- (void)updatePhysicalWebEnabled:(BOOL)enabled;
@end

namespace {

class PhysicalWebCollectionViewControllerTest
    : public CollectionViewControllerTest {
 protected:
  void SetUp() override {
    CollectionViewControllerTest::SetUp();
    pref_service_ = CreateLocalState();
    CreateController();
  }

  CollectionViewController* NewController() override {
    physicalWebController_.reset([[PhysicalWebCollectionViewController alloc]
        initWithPrefs:pref_service_.get()]);
    return [physicalWebController_ retain];
  }

  std::unique_ptr<PrefService> CreateLocalState() {
    scoped_refptr<PrefRegistrySimple> registry(new PrefRegistrySimple());
    registry->RegisterIntegerPref(prefs::kIosPhysicalWebEnabled,
                                  physical_web::kPhysicalWebOnboarding);

    sync_preferences::PrefServiceMockFactory factory;
    base::FilePath path("PhysicalWebCollectionViewControllerTest.pref");
    factory.SetUserPrefsFile(path, message_loop_.task_runner().get());
    return factory.Create(registry.get());
  }

  base::MessageLoopForUI message_loop_;
  std::unique_ptr<PrefService> pref_service_;
  base::scoped_nsobject<PhysicalWebCollectionViewController>
      physicalWebController_;
};

// Tests PhysicalWebCollectionViewController is set up with all appropriate
// items and sections.
TEST_F(PhysicalWebCollectionViewControllerTest, TestModel) {
  CheckController();
  EXPECT_EQ(2, NumberOfSections());

  // First section should have no section header and one row (switch item).
  EXPECT_EQ(1, NumberOfItemsInSection(0));
  CheckSwitchCellStateAndTitleWithId(NO, IDS_IOS_OPTIONS_ENABLE_PHYSICAL_WEB, 0,
                                     0);

  // Section section should have no section header and one row (footer item).
  EXPECT_EQ(1, NumberOfItemsInSection(1));
  CheckSectionFooterWithId(IDS_IOS_OPTIONS_ENABLE_PHYSICAL_WEB_DETAILS, 1);
}

TEST_F(PhysicalWebCollectionViewControllerTest, TestUpdateCheckedState) {
  CheckController();
  ASSERT_EQ(2, NumberOfSections());
  ASSERT_EQ(1, NumberOfItemsInSection(0));

  [physicalWebController_
      updatePhysicalWebEnabled:physical_web::kPhysicalWebOn];
}

}  // namespace
