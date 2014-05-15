// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"
#include "base/md5.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/prefs/testing_pref_service.h"
#include "base/sha1.h"
#include "base/values.h"
#include "components/metrics/persisted_logs.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

namespace {

const char kTestPrefName[] = "TestPref";
const size_t kLogCountLimit = 3;
const size_t kLogByteLimit = 1000;


class PersistedLogsTest : public testing::Test {
 public:
  PersistedLogsTest() {
    prefs_.registry()->RegisterListPref(kTestPrefName);
  }

 protected:
  TestingPrefServiceSimple prefs_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PersistedLogsTest);
};

class TestPersistedLogs : public PersistedLogs {
 public:
  TestPersistedLogs(PrefService* service)
    : PersistedLogs(service, kTestPrefName, kLogCountLimit, kLogByteLimit, 0) {
    }

  // Make a copy of the string and store the copy.
  void StoreLogCopy(std::string tmp) {
    StoreLog(&tmp);
  }

  // Stages and removes the next log, while testing it's value.
  void ExpectNextLog(const std::string& expected_log) {
    StageLog();
    EXPECT_EQ(staged_log(), expected_log);
    DiscardStagedLog();
  }
};

}  // namespace

// Store and retrieve empty list_value.
TEST_F(PersistedLogsTest, EmptyLogList) {
  TestPersistedLogs persisted_logs(&prefs_);

  persisted_logs.SerializeLogs();
  const base::ListValue* list_value = prefs_.GetList(kTestPrefName);
  EXPECT_EQ(0U, list_value->GetSize());

  TestPersistedLogs result_persisted_logs(&prefs_);
  EXPECT_EQ(PersistedLogs::LIST_EMPTY, result_persisted_logs.DeserializeLogs());
  EXPECT_EQ(0U, result_persisted_logs.size());
}

// Store and retrieve a single log value.
TEST_F(PersistedLogsTest, SingleElementLogList) {
  TestPersistedLogs persisted_logs(&prefs_);

  persisted_logs.StoreLogCopy("Hello world!");
  persisted_logs.SerializeLogs();

  const base::ListValue* list_value = prefs_.GetList(kTestPrefName);
  // |list_value| will now contain the following:
  // [1, Base64Encode("Hello world!"), MD5("Hello world!")].
  ASSERT_EQ(3U, list_value->GetSize());

  // Examine each element.
  base::ListValue::const_iterator it = list_value->begin();
  int size = 0;
  (*it)->GetAsInteger(&size);
  EXPECT_EQ(1, size);

  ++it;
  std::string str;
  (*it)->GetAsString(&str);  // Base64 encoded "Hello world!" string.
  std::string encoded;
  base::Base64Encode("Hello world!", &encoded);
  EXPECT_TRUE(encoded == str);

  ++it;
  (*it)->GetAsString(&str);  // MD5 for encoded "Hello world!" string.
  EXPECT_TRUE(base::MD5String(encoded) == str);

  ++it;
  EXPECT_TRUE(it == list_value->end());  // Reached end of list_value.

  TestPersistedLogs result_persisted_logs(&prefs_);
  EXPECT_EQ(PersistedLogs::RECALL_SUCCESS,
            result_persisted_logs.DeserializeLogs());
  EXPECT_EQ(1U, result_persisted_logs.size());
}

// Store a set of logs over the length limit, but smaller than the min number of
// bytes.
TEST_F(PersistedLogsTest, LongButTinyLogList) {
  TestPersistedLogs persisted_logs(&prefs_);

  size_t log_count = kLogCountLimit * 5;
  for (size_t i = 0; i < log_count; ++i)
    persisted_logs.StoreLogCopy("x");

  persisted_logs.SerializeLogs();

  TestPersistedLogs result_persisted_logs(&prefs_);
  EXPECT_EQ(PersistedLogs::RECALL_SUCCESS,
            result_persisted_logs.DeserializeLogs());
  EXPECT_EQ(persisted_logs.size(), result_persisted_logs.size());

  result_persisted_logs.ExpectNextLog("x");
}

// Store a set of logs over the length limit, but that doesn't reach the minimum
// number of bytes until after passing the length limit.
TEST_F(PersistedLogsTest, LongButSmallLogList) {
  TestPersistedLogs persisted_logs(&prefs_);

  // Make log_count logs each slightly larger than
  // kLogByteLimit / (log_count - 2)
  // so that the minimum is reached before the oldest (first) two logs.
  size_t log_count = kLogCountLimit * 5;
  size_t log_size = (kLogByteLimit / (log_count - 2)) + 2;
  persisted_logs.StoreLogCopy("one");
  persisted_logs.StoreLogCopy("two");
  std::string first_kept = "First to keep";
  first_kept.resize(log_size, ' ');
  persisted_logs.StoreLogCopy(first_kept);
  std::string blank_log = std::string(log_size, ' ');
  for (size_t i = persisted_logs.size(); i < log_count - 1; ++i) {
    persisted_logs.StoreLogCopy(blank_log);
  }
  std::string last_kept = "Last to keep";
  last_kept.resize(log_size, ' ');
  persisted_logs.StoreLogCopy(last_kept);

  persisted_logs.SerializeLogs();

  TestPersistedLogs result_persisted_logs(&prefs_);
  EXPECT_EQ(PersistedLogs::RECALL_SUCCESS,
            result_persisted_logs.DeserializeLogs());
  EXPECT_EQ(persisted_logs.size() - 2, result_persisted_logs.size());

  result_persisted_logs.ExpectNextLog(last_kept);
  while (result_persisted_logs.size() > 1) {
    result_persisted_logs.ExpectNextLog(blank_log);
  }
  result_persisted_logs.ExpectNextLog(first_kept);
}

// Store a set of logs within the length limit, but well over the minimum
// number of bytes.
TEST_F(PersistedLogsTest, ShortButLargeLogList) {
  TestPersistedLogs persisted_logs(&prefs_);

  // Make the total byte count about twice the minimum.
  size_t log_count = kLogCountLimit;
  size_t log_size = (kLogByteLimit / log_count) * 2;
  std::string blank_log = std::string(log_size, ' ');
  for (size_t i = 0; i < log_count; ++i) {
    persisted_logs.StoreLogCopy(blank_log);
  }

  persisted_logs.SerializeLogs();

  TestPersistedLogs result_persisted_logs(&prefs_);
  EXPECT_EQ(PersistedLogs::RECALL_SUCCESS,
            result_persisted_logs.DeserializeLogs());
  EXPECT_EQ(persisted_logs.size(), result_persisted_logs.size());
}

// Store a set of logs over the length limit, and over the minimum number of
// bytes.
TEST_F(PersistedLogsTest, LongAndLargeLogList) {
  TestPersistedLogs persisted_logs(&prefs_);

  // Include twice the max number of logs.
  size_t log_count = kLogCountLimit * 2;
  // Make the total byte count about four times the minimum.
  size_t log_size = (kLogByteLimit / log_count) * 4;
  std::string target_log = "First to keep";
  target_log.resize(log_size, ' ');
  std::string blank_log = std::string(log_size, ' ');
  for (size_t i = 0; i < log_count; ++i) {
    if (i == log_count - kLogCountLimit)
      persisted_logs.StoreLogCopy(target_log);
    else
      persisted_logs.StoreLogCopy(blank_log);
  }

  persisted_logs.SerializeLogs();

  TestPersistedLogs result_persisted_logs(&prefs_);
  EXPECT_EQ(PersistedLogs::RECALL_SUCCESS,
            result_persisted_logs.DeserializeLogs());
  EXPECT_EQ(kLogCountLimit, result_persisted_logs.size());

  while (result_persisted_logs.size() > 1) {
    result_persisted_logs.ExpectNextLog(blank_log);
  }
  result_persisted_logs.ExpectNextLog(target_log);
}

// Induce LIST_SIZE_TOO_SMALL corruption
TEST_F(PersistedLogsTest, SmallRecoveredListSize) {
  TestPersistedLogs persisted_logs(&prefs_);

  persisted_logs.StoreLogCopy("Hello world!");

  persisted_logs.SerializeLogs();

  {
    ListPrefUpdate update(&prefs_, kTestPrefName);
    base::ListValue* list_value = update.Get();
    EXPECT_EQ(3U, list_value->GetSize());

    // Remove last element.
    list_value->Remove(list_value->GetSize() - 1, NULL);
    EXPECT_EQ(2U, list_value->GetSize());
  }

  TestPersistedLogs result_persisted_logs(&prefs_);
  EXPECT_EQ(PersistedLogs::LIST_SIZE_TOO_SMALL,
            result_persisted_logs.DeserializeLogs());
}

// Remove size from the stored list_value.
TEST_F(PersistedLogsTest, RemoveSizeFromLogList) {
  TestPersistedLogs persisted_logs(&prefs_);

  persisted_logs.StoreLogCopy("one");
  persisted_logs.StoreLogCopy("two");
  EXPECT_EQ(2U,  persisted_logs.size());

  persisted_logs.SerializeLogs();

  {
    ListPrefUpdate update(&prefs_, kTestPrefName);
    base::ListValue* list_value = update.Get();
    EXPECT_EQ(4U, list_value->GetSize());

    list_value->Remove(0, NULL);  // Delete size (1st element).
    EXPECT_EQ(3U, list_value->GetSize());
  }

  TestPersistedLogs result_persisted_logs(&prefs_);
  EXPECT_EQ(PersistedLogs::LIST_SIZE_MISSING,
            result_persisted_logs.DeserializeLogs());
}

// Corrupt size of stored list_value.
TEST_F(PersistedLogsTest, CorruptSizeOfLogList) {
  TestPersistedLogs persisted_logs(&prefs_);

  persisted_logs.StoreLogCopy("Hello world!");

  persisted_logs.SerializeLogs();

  {
    ListPrefUpdate update(&prefs_, kTestPrefName);
    base::ListValue* list_value = update.Get();
    EXPECT_EQ(3U, list_value->GetSize());

    // Change list_value size from 1 to 2.
    EXPECT_TRUE(list_value->Set(0, base::Value::CreateIntegerValue(2)));
    EXPECT_EQ(3U, list_value->GetSize());
  }

  TestPersistedLogs result_persisted_logs(&prefs_);
  EXPECT_EQ(PersistedLogs::LIST_SIZE_CORRUPTION,
            result_persisted_logs.DeserializeLogs());
}

// Corrupt checksum of stored list_value.
TEST_F(PersistedLogsTest, CorruptChecksumOfLogList) {
  TestPersistedLogs persisted_logs(&prefs_);

  persisted_logs.StoreLogCopy("Hello world!");

  persisted_logs.SerializeLogs();

  {
    ListPrefUpdate update(&prefs_, kTestPrefName);
    base::ListValue* list_value = update.Get();
    EXPECT_EQ(3U, list_value->GetSize());

    // Fetch checksum (last element) and change it.
    std::string checksum;
    EXPECT_TRUE((*(list_value->end() - 1))->GetAsString(&checksum));
    checksum[0] = (checksum[0] == 'a') ? 'b' : 'a';
    EXPECT_TRUE(list_value->Set(2, base::Value::CreateStringValue(checksum)));
    EXPECT_EQ(3U, list_value->GetSize());
  }

  TestPersistedLogs result_persisted_logs(&prefs_);
  EXPECT_EQ(PersistedLogs::CHECKSUM_CORRUPTION,
            result_persisted_logs.DeserializeLogs());
}

// Check that the store/stage/discard functions work as expected.
TEST_F(PersistedLogsTest, Staging) {
  TestPersistedLogs persisted_logs(&prefs_);
  std::string tmp;

  EXPECT_FALSE(persisted_logs.has_staged_log());
  persisted_logs.StoreLogCopy("one");
  EXPECT_FALSE(persisted_logs.has_staged_log());
  persisted_logs.StoreLogCopy("two");
  persisted_logs.StageLog();
  EXPECT_TRUE(persisted_logs.has_staged_log());
  EXPECT_EQ(persisted_logs.staged_log(), std::string("two"));
  persisted_logs.StoreLogCopy("three");
  EXPECT_EQ(persisted_logs.staged_log(), std::string("two"));
  EXPECT_EQ(persisted_logs.size(), 2U);
  persisted_logs.DiscardStagedLog();
  EXPECT_FALSE(persisted_logs.has_staged_log());
  EXPECT_EQ(persisted_logs.size(), 2U);
  persisted_logs.StageLog();
  EXPECT_EQ(persisted_logs.staged_log(), std::string("three"));
  persisted_logs.DiscardStagedLog();
  persisted_logs.StageLog();
  EXPECT_EQ(persisted_logs.staged_log(), std::string("one"));
  persisted_logs.DiscardStagedLog();
  EXPECT_FALSE(persisted_logs.has_staged_log());
  EXPECT_EQ(persisted_logs.size(), 0U);
}

TEST_F(PersistedLogsTest, ProvisionalStoreStandardFlow) {
  // Ensure that provisional store works, and discards the correct log.
  TestPersistedLogs persisted_logs(&prefs_);

  persisted_logs.StoreLogCopy("one");
  persisted_logs.StageLog();
  persisted_logs.StoreStagedLogAsUnsent(PersistedLogs::PROVISIONAL_STORE);
  persisted_logs.StoreLogCopy("two");
  persisted_logs.DiscardLastProvisionalStore();
  persisted_logs.SerializeLogs();

  TestPersistedLogs result_persisted_logs(&prefs_);
  EXPECT_EQ(PersistedLogs::RECALL_SUCCESS,
            result_persisted_logs.DeserializeLogs());
  EXPECT_EQ(1U, result_persisted_logs.size());
  result_persisted_logs.ExpectNextLog("two");
}

TEST_F(PersistedLogsTest, ProvisionalStoreNoop1) {
  // Ensure that trying to drop a sent log is a no-op, even if another log has
  // since been staged.
  TestPersistedLogs persisted_logs(&prefs_);
  persisted_logs.DeserializeLogs();
  persisted_logs.StoreLogCopy("one");
  persisted_logs.StageLog();
  persisted_logs.StoreStagedLogAsUnsent(PersistedLogs::PROVISIONAL_STORE);
  persisted_logs.StageLog();
  persisted_logs.DiscardStagedLog();
  persisted_logs.StoreLogCopy("two");
  persisted_logs.StageLog();
  persisted_logs.StoreStagedLogAsUnsent(PersistedLogs::NORMAL_STORE);
  persisted_logs.DiscardLastProvisionalStore();
  persisted_logs.SerializeLogs();

  TestPersistedLogs result_persisted_logs(&prefs_);
  EXPECT_EQ(PersistedLogs::RECALL_SUCCESS,
            result_persisted_logs.DeserializeLogs());
  EXPECT_EQ(1U, result_persisted_logs.size());
  result_persisted_logs.ExpectNextLog("two");
}

TEST_F(PersistedLogsTest, ProvisionalStoreNoop2) {
  // Ensure that trying to drop more than once is a no-op
  TestPersistedLogs persisted_logs(&prefs_);
  persisted_logs.DeserializeLogs();
  persisted_logs.StoreLogCopy("one");
  persisted_logs.StageLog();
  persisted_logs.StoreStagedLogAsUnsent(PersistedLogs::NORMAL_STORE);
  persisted_logs.StoreLogCopy("two");
  persisted_logs.StageLog();
  persisted_logs.StoreStagedLogAsUnsent(PersistedLogs::PROVISIONAL_STORE);
  persisted_logs.DiscardLastProvisionalStore();
  persisted_logs.DiscardLastProvisionalStore();
  persisted_logs.SerializeLogs();

  TestPersistedLogs result_persisted_logs(&prefs_);
  EXPECT_EQ(PersistedLogs::RECALL_SUCCESS,
            result_persisted_logs.DeserializeLogs());
  EXPECT_EQ(1U, result_persisted_logs.size());
  result_persisted_logs.ExpectNextLog("one");
}

TEST_F(PersistedLogsTest, Hashes) {
  const char kFooText[] = "foo";
  const std::string foo_hash = base::SHA1HashString(kFooText);
  TestingPrefServiceSimple prefs_;
  prefs_.registry()->RegisterListPref(kTestPrefName);
  TestPersistedLogs persisted_logs(&prefs_);
  persisted_logs.StoreLogCopy(kFooText);
  persisted_logs.StageLog();
  EXPECT_EQ(foo_hash, persisted_logs.staged_log_hash());
}

}  // namespace metrics
