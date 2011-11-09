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
#include "webkit/glue/web_intent_service_data.h"

using webkit_glue::WebIntentServiceData;

namespace {

GURL test_url("http://google.com/");
string16 test_action = ASCIIToUTF16("http://webintents.org/intents/share");
string16 test_action_2 = ASCIIToUTF16("http://webintents.org/intents/view");
string16 test_title = ASCIIToUTF16("Test WebIntent");
string16 test_title_2 = ASCIIToUTF16("Test WebIntent #2");
string16 mime_image = ASCIIToUTF16("image/*");
string16 mime_video = ASCIIToUTF16("video/*");

WebIntentServiceData MakeIntentService(const GURL& url,
                                       const string16& action,
                                       const string16& type,
                                       const string16& title) {
  WebIntentServiceData service;
  service.service_url = url;
  service.action = action;
  service.type = type;
  service.title = title;
  service.disposition = WebIntentServiceData::DISPOSITION_INLINE;
  return service;
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

// Test we can add, retrieve, and remove intent services from the database.
TEST_F(WebIntentsTableTest, SetGetDeleteIntent) {
  std::vector<WebIntentServiceData> services;

  // By default, no intent services exist.
  EXPECT_TRUE(IntentsTable()->GetWebIntentServices(test_action, &services));
  EXPECT_EQ(0U, services.size());

  // Now adding one.
  WebIntentServiceData service =
      MakeIntentService(test_url, test_action, mime_image, test_title);
  EXPECT_TRUE(IntentsTable()->SetWebIntentService(service));

  // Make sure that service can now be fetched
  EXPECT_TRUE(IntentsTable()->GetWebIntentServices(test_action, &services));
  ASSERT_EQ(1U, services.size());
  EXPECT_EQ(service, services[0]);

  // Remove the service.
  EXPECT_TRUE(IntentsTable()->RemoveWebIntentService(service));

  // Should now be gone.
  services.clear();
  EXPECT_TRUE(IntentsTable()->GetWebIntentServices(test_action, &services));
  EXPECT_EQ(0U, services.size());
}

// Test we support multiple intent services for the same MIME type
TEST_F(WebIntentsTableTest, SetMultipleIntents) {
  std::vector<WebIntentServiceData> services;

  WebIntentServiceData service =
      MakeIntentService(test_url, test_action, mime_image, test_title);
  EXPECT_TRUE(IntentsTable()->SetWebIntentService(service));

  service.type = mime_video;
  service.title = test_title_2;
  EXPECT_TRUE(IntentsTable()->SetWebIntentService(service));

  // Recover stored intent services from DB.
  EXPECT_TRUE(IntentsTable()->GetWebIntentServices(test_action, &services));
  ASSERT_EQ(2U, services.size());

  // WebIntentsTable does not guarantee order, so ensure order here.
  if (services[0].type == mime_video)
    std::swap(services[0], services[1]);

  EXPECT_EQ(service, services[1]);

  service.type = mime_image;
  service.title = test_title;
  EXPECT_EQ(service, services[0]);
}

// Test we support getting all intent services independent of action.
TEST_F(WebIntentsTableTest, GetAllIntents) {
  std::vector<WebIntentServiceData> services;

  WebIntentServiceData service =
      MakeIntentService(test_url, test_action, mime_image, test_title);
  EXPECT_TRUE(IntentsTable()->SetWebIntentService(service));

  service.action = test_action_2;
  service.title = test_title_2;
  EXPECT_TRUE(IntentsTable()->SetWebIntentService(service));

  // Recover stored services from DB.
  EXPECT_TRUE(IntentsTable()->GetAllWebIntentServices(&services));
  ASSERT_EQ(2U, services.size());

  // WebIntentsTable does not guarantee order, so ensure order here.
  if (services[0].type == test_action_2)
    std::swap(services[0], services[1]);

  EXPECT_EQ(service, services[1]);

  service.action = test_action;
  service.title = test_title;
  EXPECT_EQ(service, services[0]);
}

TEST_F(WebIntentsTableTest, DispositionToStringMapping) {
  WebIntentServiceData service =
      MakeIntentService(test_url, test_action, mime_image, test_title);
  service.disposition = WebIntentServiceData::DISPOSITION_WINDOW;
  EXPECT_TRUE(IntentsTable()->SetWebIntentService(service));

  service = MakeIntentService(test_url, test_action, mime_video, test_title);
  service.disposition = WebIntentServiceData::DISPOSITION_INLINE;
  EXPECT_TRUE(IntentsTable()->SetWebIntentService(service));

  std::vector<WebIntentServiceData> services;
  EXPECT_TRUE(IntentsTable()->GetAllWebIntentServices(&services));
  ASSERT_EQ(2U, services.size());

  if (services[0].disposition == WebIntentServiceData::DISPOSITION_WINDOW)
    std::swap(services[0], services[1]);

  EXPECT_EQ(WebIntentServiceData::DISPOSITION_INLINE, services[0].disposition);
  EXPECT_EQ(WebIntentServiceData::DISPOSITION_WINDOW, services[1].disposition);
}
} // namespace
