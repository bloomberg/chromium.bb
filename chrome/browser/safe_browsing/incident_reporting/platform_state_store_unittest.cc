// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/platform_state_store.h"

#if defined(USE_PLATFORM_STATE_STORE)

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {
namespace platform_state_store {

namespace {

// The serialized form of the dict created by CreateTestIncidentsSentPref.
const uint8_t kTestData[] = {
    0x0a, 0x19, 0x08, 0x00, 0x12, 0x15, 0x0a, 0x09, 0x0a, 0x04, 0x77,
    0x68, 0x61, 0x3f, 0x10, 0x94, 0x4d, 0x0a, 0x08, 0x0a, 0x04, 0x77,
    0x68, 0x61, 0x61, 0x10, 0x00, 0x0a, 0x1a, 0x08, 0x02, 0x12, 0x16,
    0x0a, 0x09, 0x0a, 0x05, 0x62, 0x6c, 0x6f, 0x72, 0x66, 0x10, 0x05,
    0x0a, 0x09, 0x0a, 0x04, 0x73, 0x70, 0x61, 0x6d, 0x10, 0xd2, 0x09
};

// Returns a dict with some sample data in it.
scoped_ptr<base::DictionaryValue> CreateTestIncidentsSentPref() {
  scoped_ptr<base::DictionaryValue> incidents_sent(new base::DictionaryValue);

  scoped_ptr<base::DictionaryValue> type_dict(new base::DictionaryValue);
  type_dict->SetStringWithoutPathExpansion("spam", "1234");
  type_dict->SetStringWithoutPathExpansion("blorf", "5");
  incidents_sent->SetWithoutPathExpansion("2", type_dict.release());

  type_dict.reset(new base::DictionaryValue);
  type_dict->SetStringWithoutPathExpansion("whaa", "0");
  type_dict->SetStringWithoutPathExpansion("wha?", "9876");
  incidents_sent->SetWithoutPathExpansion("0", type_dict.release());

  return incidents_sent.Pass();
}

}  // namespace

// Tests that DeserializeIncidentsSent handles an empty payload properly.
TEST(PlatformStateStoreTest, DeserializeEmpty) {
  scoped_ptr<base::DictionaryValue> deserialized(new base::DictionaryValue);
  PlatformStateStoreLoadResult load_result =
      DeserializeIncidentsSent(std::string(), deserialized.get());
  ASSERT_EQ(PlatformStateStoreLoadResult::SUCCESS, load_result);
  ASSERT_TRUE(deserialized->empty());
}

// Tests that serialize followed by deserialize doesn't lose data.
TEST(PlatformStateStoreTest, RoundTrip) {
  scoped_ptr<base::DictionaryValue> incidents_sent(
      CreateTestIncidentsSentPref());
  std::string data;

  SerializeIncidentsSent(incidents_sent.get(), &data);

  // Make sure the serialized data matches expectations to ensure compatibility.
  ASSERT_EQ(std::string(reinterpret_cast<const char*>(&kTestData[0]),
                        sizeof(kTestData)), data);

  scoped_ptr<base::DictionaryValue> deserialized(new base::DictionaryValue);
  PlatformStateStoreLoadResult load_result =
      DeserializeIncidentsSent(data, deserialized.get());
  ASSERT_EQ(PlatformStateStoreLoadResult::SUCCESS, load_result);
  ASSERT_TRUE(deserialized->Equals(incidents_sent.get()));
}

}  // namespace platform_state_store
}  // namespace safe_browsing

#endif  // USE_PLATFORM_STATE_STORE
