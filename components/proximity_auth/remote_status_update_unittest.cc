// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/remote_status_update.h"

#include "base/json/json_reader.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace proximity_auth {
namespace {

// Parses the |json| into a DictionaryValue.
scoped_ptr<base::DictionaryValue> ParseJson(const std::string& json) {
  base::DictionaryValue* as_dictionary;
  base::JSONReader::Read(json)->GetAsDictionary(&as_dictionary);
  return make_scoped_ptr(as_dictionary);
}

}  // namespace

// Verify that all valid values can be parsed.
TEST(ProximityAuthRemoteStatusUpdateTest, Deserialize_ValidStatuses) {
  {
    const char kValidJson[] =
        "{"
        "  \"type\": \"status_update\","
        "  \"user_presence\": \"present\","
        "  \"secure_screen_lock\": \"enabled\","
        "  \"trust_agent\": \"enabled\""
        "}";
    scoped_ptr<RemoteStatusUpdate> parsed_update =
        RemoteStatusUpdate::Deserialize(*ParseJson(kValidJson));
    ASSERT_TRUE(parsed_update);
    EXPECT_EQ(USER_PRESENT, parsed_update->user_presence);
    EXPECT_EQ(SECURE_SCREEN_LOCK_ENABLED,
              parsed_update->secure_screen_lock_state);
    EXPECT_EQ(TRUST_AGENT_ENABLED, parsed_update->trust_agent_state);
  }

  {
    const char kValidJson[] =
        "{"
        "  \"type\": \"status_update\","
        "  \"user_presence\": \"absent\","
        "  \"secure_screen_lock\": \"disabled\","
        "  \"trust_agent\": \"disabled\""
        "}";
    scoped_ptr<RemoteStatusUpdate> parsed_update =
        RemoteStatusUpdate::Deserialize(*ParseJson(kValidJson));
    ASSERT_TRUE(parsed_update);
    EXPECT_EQ(USER_ABSENT, parsed_update->user_presence);
    EXPECT_EQ(SECURE_SCREEN_LOCK_DISABLED,
              parsed_update->secure_screen_lock_state);
    EXPECT_EQ(TRUST_AGENT_DISABLED, parsed_update->trust_agent_state);
  }

  {
    const char kValidJson[] =
        "{"
        "  \"type\": \"status_update\","
        "  \"user_presence\": \"unknown\","
        "  \"secure_screen_lock\": \"unknown\","
        "  \"trust_agent\": \"unsupported\""
        "}";
    scoped_ptr<RemoteStatusUpdate> parsed_update =
        RemoteStatusUpdate::Deserialize(*ParseJson(kValidJson));
    ASSERT_TRUE(parsed_update);
    EXPECT_EQ(USER_PRESENCE_UNKNOWN, parsed_update->user_presence);
    EXPECT_EQ(SECURE_SCREEN_LOCK_STATE_UNKNOWN,
              parsed_update->secure_screen_lock_state);
    EXPECT_EQ(TRUST_AGENT_UNSUPPORTED, parsed_update->trust_agent_state);
  }
}

TEST(ProximityAuthRemoteStatusUpdateTest, Deserialize_InvalidType) {
  const char kJson[] =
      "{"
      "  \"type\": \"garbage\","
      "  \"user_presence\": \"present\","
      "  \"secure_screen_lock\": \"enabled\","
      "  \"trust_agent\": \"enabled\""
      "}";
  scoped_ptr<RemoteStatusUpdate> parsed_update =
      RemoteStatusUpdate::Deserialize(*ParseJson(kJson));
  EXPECT_FALSE(parsed_update);
}

TEST(ProximityAuthRemoteStatusUpdateTest, Deserialize_MissingValue) {
  {
    const char kJson[] =
        "{"
        "  \"type\": \"status_update\","
        "  \"secure_screen_lock\": \"enabled\","
        "  \"trust_agent\": \"enabled\""
        "}";
    scoped_ptr<RemoteStatusUpdate> parsed_update =
        RemoteStatusUpdate::Deserialize(*ParseJson(kJson));
    EXPECT_FALSE(parsed_update);
  }

  {
    const char kJson[] =
        "{"
        "  \"type\": \"status_update\","
        "  \"user_presence\": \"present\","
        "  \"trust_agent\": \"enabled\""
        "}";
    scoped_ptr<RemoteStatusUpdate> parsed_update =
        RemoteStatusUpdate::Deserialize(*ParseJson(kJson));
    EXPECT_FALSE(parsed_update);
  }

  {
    const char kJson[] =
        "{"
        "  \"type\": \"status_update\","
        "  \"user_presence\": \"present\","
        "  \"secure_screen_lock\": \"enabled\""
        "}";
    scoped_ptr<RemoteStatusUpdate> parsed_update =
        RemoteStatusUpdate::Deserialize(*ParseJson(kJson));
    EXPECT_FALSE(parsed_update);
  }
}

TEST(ProximityAuthRemoteStatusUpdateTest, Deserialize_InvalidValues) {
  {
    const char kJson[] =
        "{"
        "  \"type\": \"status_update\","
        "  \"user_presence\": \"garbage\","
        "  \"secure_screen_lock\": \"enabled\","
        "  \"trust_agent\": \"enabled\""
        "}";
    scoped_ptr<RemoteStatusUpdate> parsed_update =
        RemoteStatusUpdate::Deserialize(*ParseJson(kJson));
    EXPECT_FALSE(parsed_update);
  }

  {
    const char kJson[] =
        "{"
        "  \"type\": \"status_update\","
        "  \"user_presence\": \"present\","
        "  \"secure_screen_lock\": \"garbage\","
        "  \"trust_agent\": \"enabled\""
        "}";
    scoped_ptr<RemoteStatusUpdate> parsed_update =
        RemoteStatusUpdate::Deserialize(*ParseJson(kJson));
    EXPECT_FALSE(parsed_update);
  }

  {
    const char kJson[] =
        "{"
        "  \"type\": \"status_update\","
        "  \"user_presence\": \"present\","
        "  \"secure_screen_lock\": \"enabled\","
        "  \"trust_agent\": \"garbage\""
        "}";
    scoped_ptr<RemoteStatusUpdate> parsed_update =
        RemoteStatusUpdate::Deserialize(*ParseJson(kJson));
    EXPECT_FALSE(parsed_update);
  }
}

// Verify that extra fields do not prevent parsing. This provides
// forward-compatibility.
TEST(ProximityAuthRemoteStatusUpdateTest,
     Deserialize_ValidStatusWithExtraFields) {
  const char kJson[] =
      "{"
      "  \"type\": \"status_update\","
      "  \"user_presence\": \"present\","
      "  \"secure_screen_lock\": \"enabled\","
      "  \"trust_agent\": \"enabled\","
      "  \"secret_sauce\": \"chipotle\""
      "}";
  scoped_ptr<RemoteStatusUpdate> parsed_update =
      RemoteStatusUpdate::Deserialize(*ParseJson(kJson));
  ASSERT_TRUE(parsed_update);
  EXPECT_EQ(USER_PRESENT, parsed_update->user_presence);
  EXPECT_EQ(SECURE_SCREEN_LOCK_ENABLED,
            parsed_update->secure_screen_lock_state);
  EXPECT_EQ(TRUST_AGENT_ENABLED, parsed_update->trust_agent_state);
}

}  // namespace proximity_auth
