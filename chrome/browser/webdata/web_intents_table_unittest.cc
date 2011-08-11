// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/file_util.h"
#include "base/scoped_temp_dir.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/webdata/web_database.h"
#include "chrome/browser/webdata/web_intents_table.h"
#include "chrome/common/chrome_paths.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

GURL test_url("http://google.com/");
string16 test_action = ASCIIToUTF16("http://webintents.org/intents/share");
string16 mime_image = ASCIIToUTF16("image/*");
string16 mime_video = ASCIIToUTF16("video/*");

class WebIntentsTableTest : public testing::Test {
 protected:
  virtual void SetUp() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_EQ(sql::INIT_OK,
              db_.Init(temp_dir_.path().AppendASCII("TestWebDatabase.db")));
  }

  WebIntentsTable* IntentsTable() {
    return db_.GetWebIntentsTable();
  }

  WebDatabase db_;
  ScopedTempDir temp_dir_;
};

// Test we can add, retrieve, and remove intents from the database.
TEST_F(WebIntentsTableTest, SetGetDeleteIntent) {
  std::vector<WebIntentData> intents;

  // By default, no intents exist.
  EXPECT_TRUE(IntentsTable()->GetWebIntents(test_action, &intents));
  EXPECT_EQ(0U, intents.size());

  // Now adding one.
  WebIntentData intent;
  intent.service_url = test_url;
  intent.action = test_action;
  intent.type = mime_image;
  EXPECT_TRUE(IntentsTable()->SetWebIntent(intent));

  // Make sure that intent can now be fetched
  EXPECT_TRUE(IntentsTable()->GetWebIntents(test_action, &intents));
  ASSERT_EQ(1U, intents.size());
  EXPECT_EQ(intent, intents[0]);

  // Remove the intent.
  EXPECT_TRUE(IntentsTable()->RemoveWebIntent(intent));

  // Intent should now be gone.
  intents.clear();
  EXPECT_TRUE(IntentsTable()->GetWebIntents(test_action, &intents));
  EXPECT_EQ(0U, intents.size());
}

// Test we support multiple intents for the same MIME type
TEST_F(WebIntentsTableTest, SetMultipleIntents) {
  std::vector<WebIntentData> intents;

  WebIntentData intent;
  intent.service_url = test_url;
  intent.action = test_action;
  intent.type = mime_image;
  EXPECT_TRUE(IntentsTable()->SetWebIntent(intent));

  intent.type = mime_video;
  EXPECT_TRUE(IntentsTable()->SetWebIntent(intent));

  // Recover stored intents from DB.
  EXPECT_TRUE(IntentsTable()->GetWebIntents(test_action, &intents));
  ASSERT_EQ(2U, intents.size());

  // WebIntentsTable does not guarantee order, so ensure order here.
  if (intents[0].type == mime_video)
    std::swap(intents[0], intents[1]);

  EXPECT_EQ(intent, intents[1]);

  intent.type = mime_image;
  EXPECT_EQ(intent, intents[0]);
}
} // namespace
