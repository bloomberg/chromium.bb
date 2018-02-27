// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/request_header/offline_page_header.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

class OfflinePageHeaderTest : public testing::Test {
 public:
  OfflinePageHeaderTest() {}
  ~OfflinePageHeaderTest() override {}

  bool ParseFromHeaderValue(const std::string& header_value,
                            bool* need_to_persist,
                            OfflinePageHeader::Reason* reason,
                            std::string* id,
                            GURL* intent_url) {
    OfflinePageHeader header(header_value);
    *need_to_persist = header.need_to_persist;
    *reason = header.reason;
    *id = header.id;
    *intent_url = header.intent_url;
    return !header.did_fail_parsing_for_test;
  }
};

TEST_F(OfflinePageHeaderTest, Parse) {
  bool need_to_persist;
  OfflinePageHeader::Reason reason;
  std::string id;
  GURL intent_url;

  // Unstructured data.
  EXPECT_FALSE(
      ParseFromHeaderValue("", &need_to_persist, &reason, &id, &intent_url));
  EXPECT_FALSE(
      ParseFromHeaderValue("   ", &need_to_persist, &reason, &id, &intent_url));
  EXPECT_FALSE(
      ParseFromHeaderValue(" , ", &need_to_persist, &reason, &id, &intent_url));
  EXPECT_FALSE(ParseFromHeaderValue("reason", &need_to_persist, &reason, &id,
                                    &intent_url));
  EXPECT_FALSE(ParseFromHeaderValue("a b c", &need_to_persist, &reason, &id,
                                    &intent_url));
  EXPECT_FALSE(
      ParseFromHeaderValue("a=b", &need_to_persist, &reason, &id, &intent_url));

  // Parse persist field.
  EXPECT_FALSE(ParseFromHeaderValue("persist=aa", &need_to_persist, &reason,
                                    &id, &intent_url));

  EXPECT_TRUE(ParseFromHeaderValue("persist=1", &need_to_persist, &reason, &id,
                                   &intent_url));
  EXPECT_TRUE(need_to_persist);
  EXPECT_EQ(OfflinePageHeader::Reason::NONE, reason);
  EXPECT_EQ("", id);
  EXPECT_TRUE(intent_url.is_empty());

  EXPECT_TRUE(ParseFromHeaderValue("persist=0", &need_to_persist, &reason, &id,
                                   &intent_url));
  EXPECT_FALSE(need_to_persist);
  EXPECT_EQ(OfflinePageHeader::Reason::NONE, reason);
  EXPECT_EQ("", id);
  EXPECT_TRUE(intent_url.is_empty());

  // Parse reason field.
  EXPECT_FALSE(ParseFromHeaderValue("reason=foo", &need_to_persist, &reason,
                                    &id, &intent_url));

  EXPECT_TRUE(ParseFromHeaderValue("reason=error", &need_to_persist, &reason,
                                   &id, &intent_url));
  EXPECT_FALSE(need_to_persist);
  EXPECT_EQ(OfflinePageHeader::Reason::NET_ERROR, reason);
  EXPECT_EQ("", id);
  EXPECT_TRUE(intent_url.is_empty());

  EXPECT_TRUE(ParseFromHeaderValue("reason=download", &need_to_persist, &reason,
                                   &id, &intent_url));
  EXPECT_FALSE(need_to_persist);
  EXPECT_EQ(OfflinePageHeader::Reason::DOWNLOAD, reason);
  EXPECT_EQ("", id);
  EXPECT_TRUE(intent_url.is_empty());

  EXPECT_TRUE(ParseFromHeaderValue("reason=reload", &need_to_persist, &reason,
                                   &id, &intent_url));
  EXPECT_FALSE(need_to_persist);
  EXPECT_EQ(OfflinePageHeader::Reason::RELOAD, reason);
  EXPECT_EQ("", id);
  EXPECT_TRUE(intent_url.is_empty());

  EXPECT_TRUE(ParseFromHeaderValue("reason=notification", &need_to_persist,
                                   &reason, &id, &intent_url));
  EXPECT_FALSE(need_to_persist);
  EXPECT_EQ(OfflinePageHeader::Reason::NOTIFICATION, reason);
  EXPECT_EQ("", id);
  EXPECT_TRUE(intent_url.is_empty());

  EXPECT_TRUE(ParseFromHeaderValue("reason=file_url_intent", &need_to_persist,
                                   &reason, &id, &intent_url));
  EXPECT_FALSE(need_to_persist);
  EXPECT_EQ(OfflinePageHeader::Reason::FILE_URL_INTENT, reason);
  EXPECT_EQ("", id);
  EXPECT_TRUE(intent_url.is_empty());

  EXPECT_TRUE(ParseFromHeaderValue("reason=content_url_intent",
                                   &need_to_persist, &reason, &id,
                                   &intent_url));
  EXPECT_FALSE(need_to_persist);
  EXPECT_EQ(OfflinePageHeader::Reason::CONTENT_URL_INTENT, reason);
  EXPECT_EQ("", id);
  EXPECT_TRUE(intent_url.is_empty());

  // Parse id field.
  EXPECT_TRUE(ParseFromHeaderValue("id=a1b2", &need_to_persist, &reason, &id,
                                   &intent_url));
  EXPECT_FALSE(need_to_persist);
  EXPECT_EQ(OfflinePageHeader::Reason::NONE, reason);
  EXPECT_EQ("a1b2", id);
  EXPECT_TRUE(intent_url.is_empty());

  // Parse intent_url field.
  EXPECT_FALSE(ParseFromHeaderValue("intent_url=://foo/bar", &need_to_persist,
                                    &reason, &id, &intent_url));

  EXPECT_TRUE(ParseFromHeaderValue("intent_url=file://foo/bar",
                                   &need_to_persist, &reason, &id,
                                   &intent_url));
  EXPECT_FALSE(need_to_persist);
  EXPECT_EQ(OfflinePageHeader::Reason::NONE, reason);
  EXPECT_EQ("", id);
  EXPECT_EQ(GURL("file://foo/bar"), intent_url);

  // Parse multiple fields.
  EXPECT_TRUE(ParseFromHeaderValue(
      "persist=1 reason=download id=a1b2 intent_url=file://foo/bar",
      &need_to_persist, &reason, &id, &intent_url));
  EXPECT_TRUE(need_to_persist);
  EXPECT_EQ(OfflinePageHeader::Reason::DOWNLOAD, reason);
  EXPECT_EQ("a1b2", id);
  EXPECT_EQ(GURL("file://foo/bar"), intent_url);
}

TEST_F(OfflinePageHeaderTest, ToString) {
  OfflinePageHeader header;
  header.need_to_persist = true;
  header.reason = OfflinePageHeader::Reason::DOWNLOAD;
  header.id = "a1b2";
  header.intent_url = GURL("file://foo/bar");
  EXPECT_EQ(
      "X-Chrome-offline: persist=1 reason=download id=a1b2 "
      "intent_url=file://foo/bar",
      header.GetCompleteHeaderString());
}

}  // namespace offline_pages
