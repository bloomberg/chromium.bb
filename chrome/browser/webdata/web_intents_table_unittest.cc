// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/file_util.h"
#include "base/scoped_temp_dir.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/intents/web_intent_data.h"
#include "chrome/browser/webdata/web_database.h"
#include "chrome/browser/webdata/web_intents_table.h"
#include "chrome/common/chrome_paths.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

GURL test_url("http://google.com/");
string16 test_action = ASCIIToUTF16("http://webintents.org/intents/share");
string16 test_action_2 = ASCIIToUTF16("http://webintents.org/intents/view");
string16 test_title = ASCIIToUTF16("Test WebIntent");
string16 test_title_2 = ASCIIToUTF16("Test WebIntent #2");
string16 mime_image = ASCIIToUTF16("image/*");
string16 mime_video = ASCIIToUTF16("video/*");

WebIntentData MakeIntent(const GURL& url, const string16& action,
                         const string16& type, const string16& title) {
  WebIntentData intent;
  intent.service_url = url;
  intent.action = action;
  intent.type = type;
  intent.title = title;
  intent.disposition = WebIntentData::DISPOSITION_INLINE;
  return intent;
}

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
  WebIntentData intent = MakeIntent(test_url, test_action, mime_image,
                                    test_title);
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

  WebIntentData intent = MakeIntent(test_url, test_action, mime_image,
                                    test_title);
  EXPECT_TRUE(IntentsTable()->SetWebIntent(intent));

  intent.type = mime_video;
  intent.title = test_title_2;
  EXPECT_TRUE(IntentsTable()->SetWebIntent(intent));

  // Recover stored intents from DB.
  EXPECT_TRUE(IntentsTable()->GetWebIntents(test_action, &intents));
  ASSERT_EQ(2U, intents.size());

  // WebIntentsTable does not guarantee order, so ensure order here.
  if (intents[0].type == mime_video)
    std::swap(intents[0], intents[1]);

  EXPECT_EQ(intent, intents[1]);

  intent.type = mime_image;
  intent.title = test_title;
  EXPECT_EQ(intent, intents[0]);
}

// Test we support getting all intents independent of action.
TEST_F(WebIntentsTableTest, GetAllIntents) {
  std::vector<WebIntentData> intents;

  WebIntentData intent = MakeIntent(test_url, test_action, mime_image,
                                    test_title);
  EXPECT_TRUE(IntentsTable()->SetWebIntent(intent));

  intent.action = test_action_2;
  intent.title = test_title_2;
  EXPECT_TRUE(IntentsTable()->SetWebIntent(intent));

  // Recover stored intents from DB.
  EXPECT_TRUE(IntentsTable()->GetAllWebIntents(&intents));
  ASSERT_EQ(2U, intents.size());

  // WebIntentsTable does not guarantee order, so ensure order here.
  if (intents[0].type == test_action_2)
    std::swap(intents[0], intents[1]);

  EXPECT_EQ(intent, intents[1]);

  intent.action = test_action;
  intent.title = test_title;
  EXPECT_EQ(intent, intents[0]);
}

TEST_F(WebIntentsTableTest, DispositionToStringMapping) {
  WebIntentData intent = MakeIntent(test_url, test_action, mime_image,
                                    test_title);
  intent.disposition = WebIntentData::DISPOSITION_WINDOW;
  EXPECT_TRUE(IntentsTable()->SetWebIntent(intent));

  intent = MakeIntent(test_url, test_action, mime_video,
                                    test_title);
  intent.disposition = WebIntentData::DISPOSITION_INLINE;
  EXPECT_TRUE(IntentsTable()->SetWebIntent(intent));

  std::vector<WebIntentData> intents;
  EXPECT_TRUE(IntentsTable()->GetAllWebIntents(&intents));
  ASSERT_EQ(2U, intents.size());

  if (intents[0].disposition == WebIntentData::DISPOSITION_WINDOW)
    std::swap(intents[0], intents[1]);

  EXPECT_EQ(WebIntentData::DISPOSITION_INLINE, intents[0].disposition);
  EXPECT_EQ(WebIntentData::DISPOSITION_WINDOW, intents[1].disposition);
}
} // namespace
