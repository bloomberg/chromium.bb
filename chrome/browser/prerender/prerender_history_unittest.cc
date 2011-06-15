// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/prerender/prerender_history.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace prerender {

namespace {

bool ListEntryMatches(ListValue* list,
                      size_t index,
                      const char* expected_url,
                      FinalStatus expected_final_status) {
  if (index >= list->GetSize())
    return false;
  DictionaryValue* dict = NULL;
  if (!list->GetDictionary(index, &dict))
    return false;
  if (dict->size() != 2u)
    return false;
  std::string url;
  if (!dict->GetString("url", &url))
    return false;
  if (url != expected_url)
    return false;
  std::string final_status;
  if (!dict->GetString("final_status", &final_status))
    return false;
  if (final_status != NameFromFinalStatus(expected_final_status))
    return false;
  return true;
}

TEST(PrerenderHistoryTest, GetAsValue)  {
  scoped_ptr<Value> entry_value;
  ListValue* entry_list = NULL;

  // Create a history with only 2 values.
  PrerenderHistory history(2);

  // Make sure an empty list exists when retrieving as value.
  entry_value.reset(history.GetEntriesAsValue());
  ASSERT_TRUE(entry_value.get() != NULL);
  ASSERT_TRUE(entry_value->GetAsList(&entry_list));
  EXPECT_TRUE(entry_list->empty());

  // Add a single entry and make sure that it matches up.
  const char* const kFirstUrl = "http://www.alpha.com/";
  const FinalStatus kFirstFinalStatus = FINAL_STATUS_USED;
  PrerenderHistory::Entry entry_first(GURL(kFirstUrl), kFirstFinalStatus);
  history.AddEntry(entry_first);
  entry_value.reset(history.GetEntriesAsValue());
  ASSERT_TRUE(entry_value.get() != NULL);
  ASSERT_TRUE(entry_value->GetAsList(&entry_list));
  EXPECT_EQ(1u, entry_list->GetSize());
  EXPECT_TRUE(ListEntryMatches(entry_list, 0u, kFirstUrl, kFirstFinalStatus));

  // Add a second entry and make sure both first and second appear.
  const char* const kSecondUrl = "http://www.beta.com/";
  const FinalStatus kSecondFinalStatus = FINAL_STATUS_INVALID_HTTP_METHOD;
  PrerenderHistory::Entry entry_second(GURL(kSecondUrl), kSecondFinalStatus);
  history.AddEntry(entry_second);
  entry_value.reset(history.GetEntriesAsValue());
  ASSERT_TRUE(entry_value.get() != NULL);
  ASSERT_TRUE(entry_value->GetAsList(&entry_list));
  EXPECT_EQ(2u, entry_list->GetSize());
  EXPECT_TRUE(ListEntryMatches(entry_list, 0u, kSecondUrl, kSecondFinalStatus));
  EXPECT_TRUE(ListEntryMatches(entry_list, 1u, kFirstUrl, kFirstFinalStatus));

  // Add a third entry and make sure that the first one drops off.
  const char* const kThirdUrl = "http://www.gamma.com/";
  const FinalStatus kThirdFinalStatus = FINAL_STATUS_AUTH_NEEDED;
  PrerenderHistory::Entry entry_third(GURL(kThirdUrl), kThirdFinalStatus);
  history.AddEntry(entry_third);
  entry_value.reset(history.GetEntriesAsValue());
  ASSERT_TRUE(entry_value.get() != NULL);
  ASSERT_TRUE(entry_value->GetAsList(&entry_list));
  EXPECT_EQ(2u, entry_list->GetSize());
  EXPECT_TRUE(ListEntryMatches(entry_list, 0u, kThirdUrl, kThirdFinalStatus));
  EXPECT_TRUE(ListEntryMatches(entry_list, 1u, kSecondUrl, kSecondFinalStatus));
}

}  // namespace

}  // namespace prerender
