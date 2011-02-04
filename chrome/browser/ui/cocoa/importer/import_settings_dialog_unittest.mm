// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#include "chrome/browser/importer/importer.h"
#import "chrome/browser/ui/cocoa/browser_test_helper.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/importer/import_settings_dialog.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using importer::HISTORY;
using importer::FAVORITES;
using importer::COOKIES;
using importer::PASSWORDS;
using importer::SEARCH_ENGINES;
using importer::NONE;

class ImportSettingsDialogTest : public CocoaTest {
 public:
  ImportSettingsDialogController* controller_;

  virtual void SetUp() {
    CocoaTest::SetUp();
    unsigned int safariServices =
        HISTORY | FAVORITES | COOKIES | PASSWORDS | SEARCH_ENGINES;
    ImportSettingsProfile* mockSafari =
        [ImportSettingsProfile
         importSettingsProfileWithBrowserName:@"MockSafari"
                                     services:safariServices];
    unsigned int firefoxServices = HISTORY | FAVORITES | COOKIES | PASSWORDS;
    ImportSettingsProfile* mockFirefox =
        [ImportSettingsProfile
         importSettingsProfileWithBrowserName:@"MockFirefox"
                                     services:firefoxServices];
    unsigned int caminoServices = HISTORY | COOKIES | SEARCH_ENGINES;
    ImportSettingsProfile* mockCamino =
        [ImportSettingsProfile
         importSettingsProfileWithBrowserName:@"MockCamino"
                                     services:caminoServices];
    NSArray* browsers = [NSArray arrayWithObjects:
                         mockSafari, mockFirefox, mockCamino, nil];
    controller_ = [[ImportSettingsDialogController alloc]
                   initWithSourceProfiles:browsers];
  }

  virtual void TearDown() {
    controller_ = NULL;
    CocoaTest::TearDown();
  }
};

TEST_F(ImportSettingsDialogTest, CancelDialog) {
  [controller_ cancel:nil];
}

TEST_F(ImportSettingsDialogTest, ChooseVariousBrowsers) {
  // Initial choice should already be MockSafari with all items enabled.
  [controller_ setSourceBrowserIndex:0];
  EXPECT_TRUE([controller_ importHistory]);
  EXPECT_TRUE([controller_ historyAvailable]);
  EXPECT_TRUE([controller_ importFavorites]);
  EXPECT_TRUE([controller_ favoritesAvailable]);
  EXPECT_TRUE([controller_ importPasswords]);
  EXPECT_TRUE([controller_ passwordsAvailable]);
  EXPECT_TRUE([controller_ importSearchEngines]);
  EXPECT_TRUE([controller_ searchEnginesAvailable]);
  EXPECT_EQ(HISTORY | FAVORITES | PASSWORDS | SEARCH_ENGINES,
            [controller_ servicesToImport]);

  // Next choice we test is MockCamino.
  [controller_ setSourceBrowserIndex:2];
  EXPECT_TRUE([controller_ importHistory]);
  EXPECT_TRUE([controller_ historyAvailable]);
  EXPECT_FALSE([controller_ importFavorites]);
  EXPECT_FALSE([controller_ favoritesAvailable]);
  EXPECT_FALSE([controller_ importPasswords]);
  EXPECT_FALSE([controller_ passwordsAvailable]);
  EXPECT_TRUE([controller_ importSearchEngines]);
  EXPECT_TRUE([controller_ searchEnginesAvailable]);
  EXPECT_EQ(HISTORY | SEARCH_ENGINES, [controller_ servicesToImport]);

  // Next choice we test is MockFirefox.
  [controller_ setSourceBrowserIndex:1];
  EXPECT_TRUE([controller_ importHistory]);
  EXPECT_TRUE([controller_ historyAvailable]);
  EXPECT_TRUE([controller_ importFavorites]);
  EXPECT_TRUE([controller_ favoritesAvailable]);
  EXPECT_TRUE([controller_ importPasswords]);
  EXPECT_TRUE([controller_ passwordsAvailable]);
  EXPECT_FALSE([controller_ importSearchEngines]);
  EXPECT_FALSE([controller_ searchEnginesAvailable]);
  EXPECT_EQ(HISTORY | FAVORITES | PASSWORDS, [controller_ servicesToImport]);

  [controller_ cancel:nil];
}

TEST_F(ImportSettingsDialogTest, SetVariousSettings) {
  // Leave the choice MockSafari, but toggle the settings.
  [controller_ setImportHistory:NO];
  [controller_ setImportFavorites:NO];
  [controller_ setImportPasswords:NO];
  [controller_ setImportSearchEngines:NO];
  EXPECT_EQ(NONE, [controller_ servicesToImport]);
  EXPECT_FALSE([controller_ importSomething]);

  [controller_ setImportHistory:YES];
  EXPECT_EQ(HISTORY, [controller_ servicesToImport]);
  EXPECT_TRUE([controller_ importSomething]);

  [controller_ setImportHistory:NO];
  [controller_ setImportFavorites:YES];
  EXPECT_EQ(FAVORITES, [controller_ servicesToImport]);
  EXPECT_TRUE([controller_ importSomething]);
  [controller_ setImportFavorites:NO];

  [controller_ setImportPasswords:YES];
  EXPECT_EQ(PASSWORDS, [controller_ servicesToImport]);
  EXPECT_TRUE([controller_ importSomething]);

  [controller_ setImportPasswords:NO];
  [controller_ setImportSearchEngines:YES];
  EXPECT_EQ(SEARCH_ENGINES, [controller_ servicesToImport]);
  EXPECT_TRUE([controller_ importSomething]);

  [controller_ cancel:nil];
}
