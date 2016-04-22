// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/md_downloads/md_downloads_dom_handler.h"

#include "chrome/test/base/testing_profile.h"
#include "content/public/test/mock_download_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(MdDownloadsDOMHandlerTest, ChecksForRemovedFiles) {
  content::TestBrowserThreadBundle thread_bundle;
  TestingProfile profile;

  testing::NiceMock<content::MockDownloadManager> manager;
  ON_CALL(manager, GetBrowserContext()).WillByDefault(
      testing::Return(&profile));

  content::TestWebUI web_ui;

  EXPECT_CALL(manager, CheckForHistoryFilesRemoval());
  MdDownloadsDOMHandler handler(&manager, &web_ui);

  testing::Mock::VerifyAndClear(&manager);

  EXPECT_CALL(manager, CheckForHistoryFilesRemoval());
  handler.OnJavascriptDisallowed();
}
