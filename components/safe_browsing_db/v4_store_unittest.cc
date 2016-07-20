// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "components/safe_browsing_db/v4_store.h"
#include "components/safe_browsing_db/v4_store.pb.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/platform_test.h"

namespace safe_browsing {

using ::google::protobuf::RepeatedField;
using ::google::protobuf::int32;

class V4StoreTest : public PlatformTest {
 public:
  V4StoreTest() : task_runner_(new base::TestSimpleTaskRunner) {}

  void SetUp() override {
    PlatformTest::SetUp();

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    store_path_ = temp_dir_.path().AppendASCII("V4StoreTest.store");
    DVLOG(1) << "store_path_: " << store_path_.value();
  }

  void TearDown() override {
    base::DeleteFile(store_path_, false);
    PlatformTest::TearDown();
  }

  void WriteFileFormatProtoToFile(uint32_t magic,
                                  uint32_t version = 0,
                                  ListUpdateResponse* response = nullptr) {
    V4StoreFileFormat file_format;
    file_format.set_magic_number(magic);
    file_format.set_version_number(version);
    if (response != nullptr) {
      ListUpdateResponse* list_update_response =
          file_format.mutable_list_update_response();
      *list_update_response = *response;
    }

    std::string file_format_string;
    file_format.SerializeToString(&file_format_string);
    base::WriteFile(store_path_, file_format_string.data(),
                    file_format_string.size());
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath store_path_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  content::TestBrowserThreadBundle thread_bundle_;
};

TEST_F(V4StoreTest, TestReadFromEmptyFile) {
  base::CloseFile(base::OpenFile(store_path_, "wb+"));

  EXPECT_EQ(FILE_EMPTY_FAILURE,
            V4Store(task_runner_, store_path_).ReadFromDisk());
}

TEST_F(V4StoreTest, TestReadFromAbsentFile) {
  EXPECT_EQ(FILE_UNREADABLE_FAILURE,
            V4Store(task_runner_, store_path_).ReadFromDisk());
}

TEST_F(V4StoreTest, TestReadFromInvalidContentsFile) {
  const char kInvalidContents[] = "Chromium";
  base::WriteFile(store_path_, kInvalidContents, strlen(kInvalidContents));
  EXPECT_EQ(PROTO_PARSING_FAILURE,
            V4Store(task_runner_, store_path_).ReadFromDisk());
}

TEST_F(V4StoreTest, TestReadFromFileWithUnknownProto) {
  Checksum checksum;
  checksum.set_sha256("checksum");
  std::string checksum_string;
  checksum.SerializeToString(&checksum_string);
  base::WriteFile(store_path_, checksum_string.data(), checksum_string.size());

  // Even though we wrote a completely different proto to file, the proto
  // parsing method does not fail. This shows the importance of a magic number.
  EXPECT_EQ(UNEXPECTED_MAGIC_NUMBER_FAILURE,
            V4Store(task_runner_, store_path_).ReadFromDisk());
}

TEST_F(V4StoreTest, TestReadFromUnexpectedMagicFile) {
  WriteFileFormatProtoToFile(111);
  EXPECT_EQ(UNEXPECTED_MAGIC_NUMBER_FAILURE,
            V4Store(task_runner_, store_path_).ReadFromDisk());
}

TEST_F(V4StoreTest, TestReadFromLowVersionFile) {
  WriteFileFormatProtoToFile(0x600D71FE, 2);
  EXPECT_EQ(FILE_VERSION_INCOMPATIBLE_FAILURE,
            V4Store(task_runner_, store_path_).ReadFromDisk());
}

TEST_F(V4StoreTest, TestReadFromNoHashPrefixInfoFile) {
  WriteFileFormatProtoToFile(0x600D71FE, 9);
  EXPECT_EQ(HASH_PREFIX_INFO_MISSING_FAILURE,
            V4Store(task_runner_, store_path_).ReadFromDisk());
}

TEST_F(V4StoreTest, TestReadFromNoHashPrefixesFile) {
  ListUpdateResponse list_update_response;
  list_update_response.set_platform_type(LINUX_PLATFORM);
  WriteFileFormatProtoToFile(0x600D71FE, 9, &list_update_response);
  EXPECT_EQ(READ_SUCCESS, V4Store(task_runner_, store_path_).ReadFromDisk());
}

TEST_F(V4StoreTest, TestWriteNoResponseType) {
  EXPECT_EQ(INVALID_RESPONSE_TYPE_FAILURE,
            V4Store(task_runner_, store_path_)
                .WriteToDisk(base::WrapUnique(new ListUpdateResponse)));
}

TEST_F(V4StoreTest, TestWritePartialResponseType) {
  std::unique_ptr<ListUpdateResponse> list_update_response(
      new ListUpdateResponse);
  list_update_response->set_response_type(ListUpdateResponse::PARTIAL_UPDATE);
  EXPECT_EQ(INVALID_RESPONSE_TYPE_FAILURE,
            V4Store(task_runner_, store_path_)
                .WriteToDisk(std::move(list_update_response)));
}

TEST_F(V4StoreTest, TestWriteFullResponseType) {
  std::unique_ptr<ListUpdateResponse> list_update_response(
      new ListUpdateResponse);
  list_update_response->set_response_type(ListUpdateResponse::FULL_UPDATE);
  list_update_response->set_new_client_state("test_client_state");
  EXPECT_EQ(WRITE_SUCCESS, V4Store(task_runner_, store_path_)
                               .WriteToDisk(std::move(list_update_response)));

  V4Store read_store(task_runner_, store_path_);
  EXPECT_EQ(READ_SUCCESS, read_store.ReadFromDisk());
  EXPECT_EQ("test_client_state", read_store.state_);
  EXPECT_TRUE(read_store.hash_prefix_map_.empty());
}

TEST_F(V4StoreTest, TestAddUnlumpedHashesWithInvalidAddition) {
  HashPrefixMap prefix_map;
  EXPECT_EQ(ADDITIONS_SIZE_UNEXPECTED_FAILURE,
            V4Store::AddUnlumpedHashes(5, "a", &prefix_map));
  EXPECT_TRUE(prefix_map.empty());
}

TEST_F(V4StoreTest, TestAddUnlumpedHashesWithEmptyString) {
  HashPrefixMap prefix_map;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(5, "", &prefix_map));
  EXPECT_TRUE(prefix_map[5].empty());
}

TEST_F(V4StoreTest, TestAddUnlumpedHashes) {
  HashPrefixMap prefix_map;
  PrefixSize prefix_size = 5;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(prefix_size, "abcde5432100000-----",
                                       &prefix_map));
  EXPECT_EQ(1u, prefix_map.size());
  HashPrefixes hash_prefixes = prefix_map.at(prefix_size);
  EXPECT_EQ(4 * prefix_size, hash_prefixes.size());
  EXPECT_EQ("abcde5432100000-----", hash_prefixes);

  prefix_size = 4;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(prefix_size, "abcde5432100000-----",
                                       &prefix_map));
  EXPECT_EQ(2u, prefix_map.size());
  hash_prefixes = prefix_map.at(prefix_size);
  EXPECT_EQ(5 * prefix_size, hash_prefixes.size());
  EXPECT_EQ("abcde5432100000-----", hash_prefixes);
}

TEST_F(V4StoreTest, TestGetNextSmallestUnmergedPrefixWithEmptyPrefixMap) {
  HashPrefixMap prefix_map;
  IteratorMap iterator_map;
  V4Store::InitializeIteratorMap(prefix_map, &iterator_map);

  HashPrefix prefix;
  EXPECT_FALSE(V4Store::GetNextSmallestUnmergedPrefix(prefix_map, iterator_map,
                                                      &prefix));
}

TEST_F(V4StoreTest, TestGetNextSmallestUnmergedPrefix) {
  HashPrefixMap prefix_map;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(5, "-----0000054321abcde", &prefix_map));
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(4, "*****0000054321abcde", &prefix_map));
  IteratorMap iterator_map;
  V4Store::InitializeIteratorMap(prefix_map, &iterator_map);

  HashPrefix prefix;
  EXPECT_TRUE(V4Store::GetNextSmallestUnmergedPrefix(prefix_map, iterator_map,
                                                     &prefix));
  EXPECT_EQ("****", prefix);
}

TEST_F(V4StoreTest, TestMergeUpdatesWithSameSizesInEachMap) {
  HashPrefixMap prefix_map_old;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(4, "abcdefgh", &prefix_map_old));
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(5, "54321abcde", &prefix_map_old));
  HashPrefixMap prefix_map_additions;
  EXPECT_EQ(
      APPLY_UPDATE_SUCCESS,
      V4Store::AddUnlumpedHashes(4, "----1111bbbb", &prefix_map_additions));
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(5, "22222bcdef", &prefix_map_additions));

  V4Store store(task_runner_, store_path_);
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            store.MergeUpdate(prefix_map_old, prefix_map_additions, nullptr));
  const HashPrefixMap& prefix_map = store.hash_prefix_map_;
  EXPECT_EQ(2u, prefix_map.size());

  PrefixSize prefix_size = 4;
  HashPrefixes hash_prefixes = prefix_map.at(prefix_size);
  EXPECT_EQ(5 * prefix_size, hash_prefixes.size());
  EXPECT_EQ("----", hash_prefixes.substr(0 * prefix_size, prefix_size));
  EXPECT_EQ("1111", hash_prefixes.substr(1 * prefix_size, prefix_size));
  EXPECT_EQ("abcd", hash_prefixes.substr(2 * prefix_size, prefix_size));
  EXPECT_EQ("bbbb", hash_prefixes.substr(3 * prefix_size, prefix_size));
  EXPECT_EQ("efgh", hash_prefixes.substr(4 * prefix_size, prefix_size));

  prefix_size = 5;
  hash_prefixes = prefix_map.at(prefix_size);
  EXPECT_EQ(4 * prefix_size, hash_prefixes.size());
  EXPECT_EQ("22222", hash_prefixes.substr(0 * prefix_size, prefix_size));
  EXPECT_EQ("54321", hash_prefixes.substr(1 * prefix_size, prefix_size));
  EXPECT_EQ("abcde", hash_prefixes.substr(2 * prefix_size, prefix_size));
  EXPECT_EQ("bcdef", hash_prefixes.substr(3 * prefix_size, prefix_size));
}

TEST_F(V4StoreTest, TestMergeUpdatesWithDifferentSizesInEachMap) {
  HashPrefixMap prefix_map_old;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(4, "1111abcdefgh", &prefix_map_old));
  HashPrefixMap prefix_map_additions;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(5, "22222bcdef", &prefix_map_additions));

  V4Store store(task_runner_, store_path_);
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            store.MergeUpdate(prefix_map_old, prefix_map_additions, nullptr));
  const HashPrefixMap& prefix_map = store.hash_prefix_map_;
  EXPECT_EQ(2u, prefix_map.size());

  PrefixSize prefix_size = 4;
  HashPrefixes hash_prefixes = prefix_map.at(prefix_size);
  EXPECT_EQ(3 * prefix_size, hash_prefixes.size());
  EXPECT_EQ("1111abcdefgh", hash_prefixes);

  prefix_size = 5;
  hash_prefixes = prefix_map.at(prefix_size);
  EXPECT_EQ(2 * prefix_size, hash_prefixes.size());
  EXPECT_EQ("22222bcdef", hash_prefixes);
}

TEST_F(V4StoreTest, TestMergeUpdatesOldMapRunsOutFirst) {
  HashPrefixMap prefix_map_old;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(4, "00001111", &prefix_map_old));
  HashPrefixMap prefix_map_additions;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(4, "2222", &prefix_map_additions));

  V4Store store(task_runner_, store_path_);
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            store.MergeUpdate(prefix_map_old, prefix_map_additions, nullptr));
  const HashPrefixMap& prefix_map = store.hash_prefix_map_;
  EXPECT_EQ(1u, prefix_map.size());

  PrefixSize prefix_size = 4;
  HashPrefixes hash_prefixes = prefix_map.at(prefix_size);
  EXPECT_EQ(3 * prefix_size, hash_prefixes.size());
  EXPECT_EQ("0000", hash_prefixes.substr(0 * prefix_size, prefix_size));
  EXPECT_EQ("1111", hash_prefixes.substr(1 * prefix_size, prefix_size));
  EXPECT_EQ("2222", hash_prefixes.substr(2 * prefix_size, prefix_size));
}

TEST_F(V4StoreTest, TestMergeUpdatesAdditionsMapRunsOutFirst) {
  HashPrefixMap prefix_map_old;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(4, "2222", &prefix_map_old));
  HashPrefixMap prefix_map_additions;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(4, "00001111", &prefix_map_additions));

  V4Store store(task_runner_, store_path_);
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            store.MergeUpdate(prefix_map_old, prefix_map_additions, nullptr));
  const HashPrefixMap& prefix_map = store.hash_prefix_map_;
  EXPECT_EQ(1u, prefix_map.size());

  PrefixSize prefix_size = 4;
  HashPrefixes hash_prefixes = prefix_map.at(prefix_size);
  EXPECT_EQ(3 * prefix_size, hash_prefixes.size());
  EXPECT_EQ("0000", hash_prefixes.substr(0 * prefix_size, prefix_size));
  EXPECT_EQ("1111", hash_prefixes.substr(1 * prefix_size, prefix_size));
  EXPECT_EQ("2222", hash_prefixes.substr(2 * prefix_size, prefix_size));
}

TEST_F(V4StoreTest, TestMergeUpdatesFailsForRepeatedHashPrefix) {
  HashPrefixMap prefix_map_old;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(4, "2222", &prefix_map_old));
  HashPrefixMap prefix_map_additions;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(4, "2222", &prefix_map_additions));

  V4Store store(task_runner_, store_path_);
  EXPECT_EQ(ADDITIONS_HAS_EXISTING_PREFIX_FAILURE,
            store.MergeUpdate(prefix_map_old, prefix_map_additions, nullptr));
}

TEST_F(V4StoreTest, TestMergeUpdatesFailsWhenRemovalsIndexTooLarge) {
  HashPrefixMap prefix_map_old;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(4, "2222", &prefix_map_old));
  HashPrefixMap prefix_map_additions;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(4, "11113333", &prefix_map_additions));

  // Even though the merged map could have size 3 without removals, the
  // removals index should only count the entries in the old map.
  V4Store store(task_runner_, store_path_);
  RepeatedField<int32> raw_removals;
  // old_store: ["2222"]
  raw_removals.Add(1);
  EXPECT_EQ(
      REMOVALS_INDEX_TOO_LARGE,
      store.MergeUpdate(prefix_map_old, prefix_map_additions, &raw_removals));
}

TEST_F(V4StoreTest, TestMergeUpdatesRemovesOnlyElement) {
  HashPrefixMap prefix_map_old;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(4, "2222", &prefix_map_old));
  HashPrefixMap prefix_map_additions;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(5, "1111133333", &prefix_map_additions));

  V4Store store(task_runner_, store_path_);
  RepeatedField<int32> raw_removals;
  // old_store: ["2222"]
  raw_removals.Add(0);  // Removes "2222"
  EXPECT_EQ(
      APPLY_UPDATE_SUCCESS,
      store.MergeUpdate(prefix_map_old, prefix_map_additions, &raw_removals));
  const HashPrefixMap& prefix_map = store.hash_prefix_map_;
  // The size is 2 since we reserve space anyway.
  EXPECT_EQ(2u, prefix_map.size());
  EXPECT_TRUE(prefix_map.at(4).empty());
  EXPECT_EQ("1111133333", prefix_map.at(5));
}

TEST_F(V4StoreTest, TestMergeUpdatesRemovesFirstElement) {
  HashPrefixMap prefix_map_old;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(4, "22224444", &prefix_map_old));
  HashPrefixMap prefix_map_additions;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(5, "1111133333", &prefix_map_additions));

  V4Store store(task_runner_, store_path_);
  RepeatedField<int32> raw_removals;
  // old_store: ["2222", "4444"]
  raw_removals.Add(0);  // Removes "2222"
  EXPECT_EQ(
      APPLY_UPDATE_SUCCESS,
      store.MergeUpdate(prefix_map_old, prefix_map_additions, &raw_removals));
  const HashPrefixMap& prefix_map = store.hash_prefix_map_;
  // The size is 2 since we reserve space anyway.
  EXPECT_EQ(2u, prefix_map.size());
  EXPECT_EQ("4444", prefix_map.at(4));
  EXPECT_EQ("1111133333", prefix_map.at(5));
}

TEST_F(V4StoreTest, TestMergeUpdatesRemovesMiddleElement) {
  HashPrefixMap prefix_map_old;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(4, "222233334444", &prefix_map_old));
  HashPrefixMap prefix_map_additions;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(5, "1111133333", &prefix_map_additions));

  V4Store store(task_runner_, store_path_);
  RepeatedField<int32> raw_removals;
  // old_store: ["2222", "3333", 4444"]
  raw_removals.Add(1);  // Removes "3333"
  EXPECT_EQ(
      APPLY_UPDATE_SUCCESS,
      store.MergeUpdate(prefix_map_old, prefix_map_additions, &raw_removals));
  const HashPrefixMap& prefix_map = store.hash_prefix_map_;
  // The size is 2 since we reserve space anyway.
  EXPECT_EQ(2u, prefix_map.size());
  EXPECT_EQ("22224444", prefix_map.at(4));
  EXPECT_EQ("1111133333", prefix_map.at(5));
}

TEST_F(V4StoreTest, TestMergeUpdatesRemovesLastElement) {
  HashPrefixMap prefix_map_old;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(4, "222233334444", &prefix_map_old));
  HashPrefixMap prefix_map_additions;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(5, "1111133333", &prefix_map_additions));

  V4Store store(task_runner_, store_path_);
  RepeatedField<int32> raw_removals;
  // old_store: ["2222", "3333", 4444"]
  raw_removals.Add(2);  // Removes "4444"
  EXPECT_EQ(
      APPLY_UPDATE_SUCCESS,
      store.MergeUpdate(prefix_map_old, prefix_map_additions, &raw_removals));
  const HashPrefixMap& prefix_map = store.hash_prefix_map_;
  // The size is 2 since we reserve space anyway.
  EXPECT_EQ(2u, prefix_map.size());
  EXPECT_EQ("22223333", prefix_map.at(4));
  EXPECT_EQ("1111133333", prefix_map.at(5));
}

TEST_F(V4StoreTest, TestMergeUpdatesRemovesWhenOldHasDifferentSizes) {
  HashPrefixMap prefix_map_old;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(4, "222233334444", &prefix_map_old));
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(5, "aaaaabbbbb", &prefix_map_old));
  HashPrefixMap prefix_map_additions;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(5, "1111133333", &prefix_map_additions));

  V4Store store(task_runner_, store_path_);
  RepeatedField<int32> raw_removals;
  // old_store: ["2222", "3333", 4444", "aaaaa", "bbbbb"]
  raw_removals.Add(3);  // Removes "aaaaa"
  EXPECT_EQ(
      APPLY_UPDATE_SUCCESS,
      store.MergeUpdate(prefix_map_old, prefix_map_additions, &raw_removals));
  const HashPrefixMap& prefix_map = store.hash_prefix_map_;
  // The size is 2 since we reserve space anyway.
  EXPECT_EQ(2u, prefix_map.size());
  EXPECT_EQ("222233334444", prefix_map.at(4));
  EXPECT_EQ("1111133333bbbbb", prefix_map.at(5));
}

TEST_F(V4StoreTest, TestMergeUpdatesRemovesMultipleAcrossDifferentSizes) {
  HashPrefixMap prefix_map_old;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(4, "22223333aaaa", &prefix_map_old));
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(5, "3333344444bbbbb", &prefix_map_old));
  HashPrefixMap prefix_map_additions;
  EXPECT_EQ(APPLY_UPDATE_SUCCESS,
            V4Store::AddUnlumpedHashes(5, "11111", &prefix_map_additions));

  V4Store store(task_runner_, store_path_);
  RepeatedField<int32> raw_removals;
  // old_store: ["2222", "3333", "33333", "44444", "aaaa", "bbbbb"]
  raw_removals.Add(1);  // Removes "3333"
  raw_removals.Add(3);  // Removes "44444"
  EXPECT_EQ(
      APPLY_UPDATE_SUCCESS,
      store.MergeUpdate(prefix_map_old, prefix_map_additions, &raw_removals));
  const HashPrefixMap& prefix_map = store.hash_prefix_map_;
  // The size is 2 since we reserve space anyway.
  EXPECT_EQ(2u, prefix_map.size());
  EXPECT_EQ("2222aaaa", prefix_map.at(4));
  EXPECT_EQ("1111133333bbbbb", prefix_map.at(5));
}

TEST_F(V4StoreTest, TestReadFullResponseWithValidHashPrefixMap) {
  std::unique_ptr<ListUpdateResponse> lur(new ListUpdateResponse);
  lur->set_response_type(ListUpdateResponse::FULL_UPDATE);
  lur->set_new_client_state("test_client_state");
  lur->set_platform_type(WINDOWS_PLATFORM);
  lur->set_threat_entry_type(URL);
  lur->set_threat_type(MALWARE_THREAT);
  ThreatEntrySet* additions = lur->add_additions();
  additions->set_compression_type(RAW);
  additions->mutable_raw_hashes()->set_prefix_size(5);
  additions->mutable_raw_hashes()->set_raw_hashes("00000abcde");
  additions = lur->add_additions();
  additions->set_compression_type(RAW);
  additions->mutable_raw_hashes()->set_prefix_size(4);
  additions->mutable_raw_hashes()->set_raw_hashes("00000abc");
  EXPECT_EQ(WRITE_SUCCESS,
            V4Store(task_runner_, store_path_).WriteToDisk(std::move(lur)));

  V4Store read_store (task_runner_, store_path_);
  EXPECT_EQ(READ_SUCCESS, read_store.ReadFromDisk());
  EXPECT_EQ("test_client_state", read_store.state_);
  ASSERT_EQ(2u, read_store.hash_prefix_map_.size());
  EXPECT_EQ("00000abc", read_store.hash_prefix_map_[4]);
  EXPECT_EQ("00000abcde", read_store.hash_prefix_map_[5]);
}

// This tests fails to read the prefix map from the disk because the file on
// disk is invalid. The hash prefixes string is 6 bytes long, but the prefix
// size is 5 so the parser isn't able to split the hash prefixes list
// completely.
TEST_F(V4StoreTest, TestReadFullResponseWithInvalidHashPrefixMap) {
  std::unique_ptr<ListUpdateResponse> lur(new ListUpdateResponse);
  lur->set_response_type(ListUpdateResponse::FULL_UPDATE);
  lur->set_new_client_state("test_client_state");
  lur->set_platform_type(WINDOWS_PLATFORM);
  lur->set_threat_entry_type(URL);
  lur->set_threat_type(MALWARE_THREAT);
  ThreatEntrySet* additions = lur->add_additions();
  additions->set_compression_type(RAW);
  additions->mutable_raw_hashes()->set_prefix_size(5);
  additions->mutable_raw_hashes()->set_raw_hashes("abcdef");
  EXPECT_EQ(WRITE_SUCCESS,
            V4Store(task_runner_, store_path_).WriteToDisk(std::move(lur)));

  V4Store read_store(task_runner_, store_path_);
  EXPECT_EQ(HASH_PREFIX_MAP_GENERATION_FAILURE, read_store.ReadFromDisk());
  EXPECT_TRUE(read_store.state_.empty());
  EXPECT_TRUE(read_store.hash_prefix_map_.empty());
}

TEST_F(V4StoreTest, TestHashPrefixExistsAtTheBeginning) {
  HashPrefixes hash_prefixes = "abcdebbbbbccccc";
  HashPrefix hash_prefix = "abcde";
  EXPECT_TRUE(V4Store::HashPrefixMatches(hash_prefix, std::begin(hash_prefixes),
                                         std::end(hash_prefixes)));
}

TEST_F(V4StoreTest, TestHashPrefixExistsInTheMiddle) {
  HashPrefixes hash_prefixes = "abcdebbbbbccccc";
  HashPrefix hash_prefix = "bbbbb";
  EXPECT_TRUE(V4Store::HashPrefixMatches(hash_prefix, std::begin(hash_prefixes),
                                         std::end(hash_prefixes)));
}

TEST_F(V4StoreTest, TestHashPrefixExistsAtTheEnd) {
  HashPrefixes hash_prefixes = "abcdebbbbbccccc";
  HashPrefix hash_prefix = "ccccc";
  EXPECT_TRUE(V4Store::HashPrefixMatches(hash_prefix, std::begin(hash_prefixes),
                                         std::end(hash_prefixes)));
}

TEST_F(V4StoreTest, TestHashPrefixExistsAtTheBeginningOfEven) {
  HashPrefixes hash_prefixes = "abcdebbbbb";
  HashPrefix hash_prefix = "abcde";
  EXPECT_TRUE(V4Store::HashPrefixMatches(hash_prefix, std::begin(hash_prefixes),
                                         std::end(hash_prefixes)));
}

TEST_F(V4StoreTest, TestHashPrefixExistsAtTheEndOfEven) {
  HashPrefixes hash_prefixes = "abcdebbbbb";
  HashPrefix hash_prefix = "bbbbb";
  EXPECT_TRUE(V4Store::HashPrefixMatches(hash_prefix, std::begin(hash_prefixes),
                                         std::end(hash_prefixes)));
}

TEST_F(V4StoreTest, TestHashPrefixDoesNotExistInConcatenatedList) {
  HashPrefixes hash_prefixes = "abcdebbbbb";
  HashPrefix hash_prefix = "bbbbc";
  EXPECT_FALSE(V4Store::HashPrefixMatches(
      hash_prefix, std::begin(hash_prefixes), std::end(hash_prefixes)));
}

TEST_F(V4StoreTest, TestFullHashExistsInMapWithSingleSize) {
  V4Store store(task_runner_, store_path_);
  store.hash_prefix_map_[32] =
      "0111222233334444555566667777888811112222333344445555666677778888";
  FullHash full_hash = "11112222333344445555666677778888";
  EXPECT_EQ("11112222333344445555666677778888",
            store.GetMatchingHashPrefix(full_hash));
}

TEST_F(V4StoreTest, TestFullHashExistsInMapWithDifferentSizes) {
  V4Store store(task_runner_, store_path_);
  store.hash_prefix_map_[4] = "22223333aaaa";
  store.hash_prefix_map_[32] = "11112222333344445555666677778888";
  FullHash full_hash = "11112222333344445555666677778888";
  EXPECT_EQ("11112222333344445555666677778888",
            store.GetMatchingHashPrefix(full_hash));
}

TEST_F(V4StoreTest, TestHashPrefixExistsInMapWithSingleSize) {
  V4Store store(task_runner_, store_path_);
  store.hash_prefix_map_[4] = "22223333aaaa";
  FullHash full_hash = "22222222222222222222222222222222";
  EXPECT_EQ("2222", store.GetMatchingHashPrefix(full_hash));
}

TEST_F(V4StoreTest, TestHashPrefixExistsInMapWithDifferentSizes) {
  V4Store store(task_runner_, store_path_);
  store.hash_prefix_map_[4] = "22223333aaaa";
  store.hash_prefix_map_[5] = "11111hhhhh";
  FullHash full_hash = "22222222222222222222222222222222";
  EXPECT_EQ("2222", store.GetMatchingHashPrefix(full_hash));
}

TEST_F(V4StoreTest, TestHashPrefixDoesNotExistInMapWithDifferentSizes) {
  V4Store store(task_runner_, store_path_);
  store.hash_prefix_map_[4] = "3333aaaa";
  store.hash_prefix_map_[5] = "11111hhhhh";
  FullHash full_hash = "22222222222222222222222222222222";
  EXPECT_TRUE(store.GetMatchingHashPrefix(full_hash).empty());
}

}  // namespace safe_browsing
