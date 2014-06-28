// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for the SafeBrowsing storage system.

#include "chrome/browser/safe_browsing/safe_browsing_database.h"

#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/time/time.h"
#include "chrome/browser/safe_browsing/chunk.pb.h"
#include "chrome/browser/safe_browsing/safe_browsing_store_file.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "crypto/sha2.h"
#include "net/base/net_util.h"
#include "sql/connection.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

using base::Time;
using base::TimeDelta;

namespace {

const TimeDelta kCacheLifetime = TimeDelta::FromMinutes(45);

SBPrefix SBPrefixForString(const std::string& str) {
  return SBFullHashForString(str).prefix;
}

// Construct a full hash which has the given prefix, with the given
// suffix data coming after the prefix.
SBFullHash SBFullHashForPrefixAndSuffix(SBPrefix prefix,
                                        const base::StringPiece& suffix) {
  SBFullHash full_hash;
  memset(&full_hash, 0, sizeof(SBFullHash));
  full_hash.prefix = prefix;
  CHECK_LE(suffix.size() + sizeof(SBPrefix), sizeof(SBFullHash));
  memcpy(full_hash.full_hash + sizeof(SBPrefix), suffix.data(), suffix.size());
  return full_hash;
}

std::string HashedIpPrefix(const std::string& ip_prefix, size_t prefix_size) {
  net::IPAddressNumber ip_number;
  EXPECT_TRUE(net::ParseIPLiteralToNumber(ip_prefix, &ip_number));
  EXPECT_EQ(net::kIPv6AddressSize, ip_number.size());
  const std::string hashed_ip_prefix = base::SHA1HashString(
      net::IPAddressToPackedString(ip_number));
  std::string hash(crypto::kSHA256Length, '\0');
  hash.replace(0, hashed_ip_prefix.size(), hashed_ip_prefix);
  hash[base::kSHA1Length] = static_cast<char>(prefix_size);
  return hash;
}

// Helper to build a chunk.  Caller takes ownership.
SBChunkData* BuildChunk(int chunk_number,
                        safe_browsing::ChunkData::ChunkType chunk_type,
                        safe_browsing::ChunkData::PrefixType prefix_type,
                        const void* data, size_t data_size,
                        const std::vector<int>& add_chunk_numbers) {
  scoped_ptr<safe_browsing::ChunkData> raw_data(new safe_browsing::ChunkData);
  raw_data->set_chunk_number(chunk_number);
  raw_data->set_chunk_type(chunk_type);
  raw_data->set_prefix_type(prefix_type);
  raw_data->set_hashes(data, data_size);
  raw_data->clear_add_numbers();
  for (size_t i = 0; i < add_chunk_numbers.size(); ++i) {
    raw_data->add_add_numbers(add_chunk_numbers[i]);
  }

  return new SBChunkData(raw_data.release());
}

// Create add chunk with a single prefix.
SBChunkData* AddChunkPrefix(int chunk_number,  SBPrefix prefix) {
  return BuildChunk(chunk_number, safe_browsing::ChunkData::ADD,
                    safe_browsing::ChunkData::PREFIX_4B,
                    &prefix, sizeof(prefix),
                    std::vector<int>());
}

// Create add chunk with a single prefix generated from |value|.
SBChunkData* AddChunkPrefixValue(int chunk_number,
                                 const std::string& value) {
  return AddChunkPrefix(chunk_number, SBPrefixForString(value));
}

// Generate an add chunk with two prefixes.
SBChunkData* AddChunkPrefix2Value(int chunk_number,
                                  const std::string& value1,
                                  const std::string& value2) {
  const SBPrefix prefixes[2] = {
    SBPrefixForString(value1),
    SBPrefixForString(value2),
  };
  return BuildChunk(chunk_number, safe_browsing::ChunkData::ADD,
                    safe_browsing::ChunkData::PREFIX_4B,
                    &prefixes[0], sizeof(prefixes),
                    std::vector<int>());
}

// Generate an add chunk with four prefixes.
SBChunkData* AddChunkPrefix4Value(int chunk_number,
                                  const std::string& value1,
                                  const std::string& value2,
                                  const std::string& value3,
                                  const std::string& value4) {
  const SBPrefix prefixes[4] = {
    SBPrefixForString(value1),
    SBPrefixForString(value2),
    SBPrefixForString(value3),
    SBPrefixForString(value4),
  };
  return BuildChunk(chunk_number, safe_browsing::ChunkData::ADD,
                    safe_browsing::ChunkData::PREFIX_4B,
                    &prefixes[0], sizeof(prefixes),
                    std::vector<int>());
}

// Generate an add chunk with a full hash.
SBChunkData* AddChunkFullHash(int chunk_number, SBFullHash full_hash) {
  return BuildChunk(chunk_number, safe_browsing::ChunkData::ADD,
                    safe_browsing::ChunkData::FULL_32B,
                    &full_hash, sizeof(full_hash),
                    std::vector<int>());
}

// Generate an add chunk with a full hash generated from |value|.
SBChunkData* AddChunkFullHashValue(int chunk_number,
                                   const std::string& value) {
  return AddChunkFullHash(chunk_number, SBFullHashForString(value));
}

// Generate an add chunk with two full hashes.
SBChunkData* AddChunkFullHash2Value(int chunk_number,
                                   const std::string& value1,
                                   const std::string& value2) {
  const SBFullHash full_hashes[2] = {
    SBFullHashForString(value1),
    SBFullHashForString(value2),
  };
  return BuildChunk(chunk_number, safe_browsing::ChunkData::ADD,
                    safe_browsing::ChunkData::FULL_32B,
                    &full_hashes[0], sizeof(full_hashes),
                    std::vector<int>());
}

// Generate a sub chunk with a prefix generated from |value|.
SBChunkData* SubChunkPrefixValue(int chunk_number,
                                 const std::string& value,
                                 int add_chunk_number) {
  const SBPrefix prefix = SBPrefixForString(value);
  return BuildChunk(chunk_number, safe_browsing::ChunkData::SUB,
                    safe_browsing::ChunkData::PREFIX_4B,
                    &prefix, sizeof(prefix),
                    std::vector<int>(1, add_chunk_number));
}

// Generate a sub chunk with two prefixes.
SBChunkData* SubChunkPrefix2Value(int chunk_number,
                                  const std::string& value1,
                                  int add_chunk_number1,
                                  const std::string& value2,
                                  int add_chunk_number2) {
  const SBPrefix prefixes[2] = {
    SBPrefixForString(value1),
    SBPrefixForString(value2),
  };
  std::vector<int> add_chunk_numbers;
  add_chunk_numbers.push_back(add_chunk_number1);
  add_chunk_numbers.push_back(add_chunk_number2);
  return BuildChunk(chunk_number, safe_browsing::ChunkData::SUB,
                    safe_browsing::ChunkData::PREFIX_4B,
                    &prefixes[0], sizeof(prefixes),
                    add_chunk_numbers);
}

// Generate a sub chunk with a full hash.
SBChunkData* SubChunkFullHash(int chunk_number,
                              SBFullHash full_hash,
                              int add_chunk_number) {
  return BuildChunk(chunk_number, safe_browsing::ChunkData::SUB,
                    safe_browsing::ChunkData::FULL_32B,
                    &full_hash, sizeof(full_hash),
                    std::vector<int>(1, add_chunk_number));
}

// Generate a sub chunk with a full hash generated from |value|.
SBChunkData* SubChunkFullHashValue(int chunk_number,
                                   const std::string& value,
                                   int add_chunk_number) {
  return SubChunkFullHash(chunk_number,
                          SBFullHashForString(value),
                          add_chunk_number);
}

// Generate an add chunk with a single full hash for the ip blacklist.
SBChunkData* AddChunkHashedIpValue(int chunk_number,
                                   const std::string& ip_str,
                                   size_t prefix_size) {
  const std::string full_hash_str = HashedIpPrefix(ip_str, prefix_size);
  EXPECT_EQ(sizeof(SBFullHash), full_hash_str.size());
  SBFullHash full_hash;
  std::memcpy(&(full_hash.full_hash), full_hash_str.data(), sizeof(SBFullHash));
  return BuildChunk(chunk_number, safe_browsing::ChunkData::ADD,
                    safe_browsing::ChunkData::FULL_32B,
                    &full_hash, sizeof(full_hash),
                    std::vector<int>());
}

// Prevent DCHECK from killing tests.
// TODO(shess): Pawel disputes the use of this, so the test which uses
// it is DISABLED.  http://crbug.com/56448
class ScopedLogMessageIgnorer {
 public:
  ScopedLogMessageIgnorer() {
    logging::SetLogMessageHandler(&LogMessageIgnorer);
  }
  ~ScopedLogMessageIgnorer() {
    // TODO(shess): Would be better to verify whether anyone else
    // changed it, and then restore it to the previous value.
    logging::SetLogMessageHandler(NULL);
  }

 private:
  static bool LogMessageIgnorer(int severity, const char* file, int line,
      size_t message_start, const std::string& str) {
    // Intercept FATAL, strip the stack backtrace, and log it without
    // the crash part.
    if (severity == logging::LOG_FATAL) {
      size_t newline = str.find('\n');
      if (newline != std::string::npos) {
        const std::string msg = str.substr(0, newline + 1);
        fprintf(stderr, "%s", msg.c_str());
        fflush(stderr);
      }
      return true;
    }

    return false;
  }
};

}  // namespace

class SafeBrowsingDatabaseTest : public PlatformTest {
 public:
  virtual void SetUp() {
    PlatformTest::SetUp();

    // Setup a database in a temporary directory.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    database_.reset(new SafeBrowsingDatabaseNew);
    database_filename_ =
        temp_dir_.path().AppendASCII("SafeBrowsingTestDatabase");
    database_->Init(database_filename_);
  }

  virtual void TearDown() {
    database_.reset();

    PlatformTest::TearDown();
  }

  void GetListsInfo(std::vector<SBListChunkRanges>* lists) {
    lists->clear();
    ASSERT_TRUE(database_->UpdateStarted(lists));
    database_->UpdateFinished(true);
  }

  // Helper function to do an AddDel or SubDel command.
  void DelChunk(const std::string& list,
                int chunk_id,
                bool is_sub_del) {
    std::vector<SBChunkDelete> deletes;
    SBChunkDelete chunk_delete;
    chunk_delete.list_name = list;
    chunk_delete.is_sub_del = is_sub_del;
    chunk_delete.chunk_del.push_back(ChunkRange(chunk_id));
    deletes.push_back(chunk_delete);
    database_->DeleteChunks(deletes);
  }

  void AddDelChunk(const std::string& list, int chunk_id) {
    DelChunk(list, chunk_id, false);
  }

  void SubDelChunk(const std::string& list, int chunk_id) {
    DelChunk(list, chunk_id, true);
  }

  // Utility function for setting up the database for the caching test.
  void PopulateDatabaseForCacheTest();

  scoped_ptr<SafeBrowsingDatabaseNew> database_;
  base::FilePath database_filename_;
  base::ScopedTempDir temp_dir_;
};

// Tests retrieving list name information.
TEST_F(SafeBrowsingDatabaseTest, ListNameForBrowse) {
  std::vector<SBListChunkRanges> lists;
  ScopedVector<SBChunkData> chunks;

  chunks.push_back(AddChunkPrefixValue(1, "www.evil.com/malware.html"));
  chunks.push_back(AddChunkPrefixValue(2, "www.foo.com/malware.html"));
  chunks.push_back(AddChunkPrefixValue(3, "www.whatever.com/malware.html"));

  ASSERT_TRUE(database_->UpdateStarted(&lists));
  database_->InsertChunks(safe_browsing_util::kMalwareList, chunks.get());
  database_->UpdateFinished(true);

  GetListsInfo(&lists);
  ASSERT_LE(1U, lists.size());
  EXPECT_EQ(safe_browsing_util::kMalwareList, lists[0].name);
  EXPECT_EQ("1-3", lists[0].adds);
  EXPECT_TRUE(lists[0].subs.empty());

  // Insert a malware sub chunk.
  chunks.clear();
  chunks.push_back(SubChunkPrefixValue(7, "www.subbed.com/noteveil1.html", 19));

  ASSERT_TRUE(database_->UpdateStarted(&lists));
  database_->InsertChunks(safe_browsing_util::kMalwareList, chunks.get());
  database_->UpdateFinished(true);

  GetListsInfo(&lists);
  ASSERT_LE(1U, lists.size());
  EXPECT_EQ(safe_browsing_util::kMalwareList, lists[0].name);
  EXPECT_EQ("1-3", lists[0].adds);
  EXPECT_EQ("7", lists[0].subs);
  if (lists.size() == 2) {
    // Old style database won't have the second entry since it creates the lists
    // when it receives an update containing that list. The filter-based
    // database has these values hard coded.
    EXPECT_EQ(safe_browsing_util::kPhishingList, lists[1].name);
    EXPECT_TRUE(lists[1].adds.empty());
    EXPECT_TRUE(lists[1].subs.empty());
  }

  // Add phishing chunks.
  chunks.clear();
  chunks.push_back(AddChunkPrefixValue(47, "www.evil.com/phishing.html"));
  chunks.push_back(
      SubChunkPrefixValue(200, "www.phishy.com/notevil1.html", 1999));
  chunks.push_back(
      SubChunkPrefixValue(201, "www.phishy2.com/notevil1.html", 1999));

  ASSERT_TRUE(database_->UpdateStarted(&lists));
  database_->InsertChunks(safe_browsing_util::kPhishingList, chunks.get());
  database_->UpdateFinished(true);

  GetListsInfo(&lists);
  ASSERT_EQ(2U, lists.size());
  EXPECT_EQ(safe_browsing_util::kMalwareList, lists[0].name);
  EXPECT_EQ("1-3", lists[0].adds);
  EXPECT_EQ("7", lists[0].subs);
  EXPECT_EQ(safe_browsing_util::kPhishingList, lists[1].name);
  EXPECT_EQ("47", lists[1].adds);
  EXPECT_EQ("200-201", lists[1].subs);
}

TEST_F(SafeBrowsingDatabaseTest, ListNameForBrowseAndDownload) {
  database_.reset();
  base::MessageLoop loop;
  SafeBrowsingStoreFile* browse_store = new SafeBrowsingStoreFile();
  SafeBrowsingStoreFile* download_store = new SafeBrowsingStoreFile();
  SafeBrowsingStoreFile* csd_whitelist_store = new SafeBrowsingStoreFile();
  SafeBrowsingStoreFile* download_whitelist_store = new SafeBrowsingStoreFile();
  SafeBrowsingStoreFile* extension_blacklist_store =
      new SafeBrowsingStoreFile();
  SafeBrowsingStoreFile* ip_blacklist_store = new SafeBrowsingStoreFile();
  database_.reset(new SafeBrowsingDatabaseNew(browse_store,
                                              download_store,
                                              csd_whitelist_store,
                                              download_whitelist_store,
                                              extension_blacklist_store,
                                              NULL,
                                              ip_blacklist_store));
  database_->Init(database_filename_);

  ScopedVector<SBChunkData> chunks;

  std::vector<SBListChunkRanges> lists;
  ASSERT_TRUE(database_->UpdateStarted(&lists));

  // Insert malware, phish, binurl and bindownload add chunks.
  chunks.push_back(AddChunkPrefixValue(1, "www.evil.com/malware.html"));
  database_->InsertChunks(safe_browsing_util::kMalwareList, chunks.get());

  chunks.clear();
  chunks.push_back(AddChunkPrefixValue(2, "www.foo.com/malware.html"));
  database_->InsertChunks(safe_browsing_util::kPhishingList, chunks.get());

  chunks.clear();
  chunks.push_back(AddChunkPrefixValue(3, "www.whatever.com/download.html"));
  database_->InsertChunks(safe_browsing_util::kBinUrlList, chunks.get());

  chunks.clear();
  chunks.push_back(AddChunkFullHashValue(5, "www.forwhitelist.com/a.html"));
  database_->InsertChunks(safe_browsing_util::kCsdWhiteList, chunks.get());

  chunks.clear();
  chunks.push_back(AddChunkFullHashValue(6, "www.download.com/"));
  database_->InsertChunks(safe_browsing_util::kDownloadWhiteList, chunks.get());

  chunks.clear();
  chunks.push_back(AddChunkFullHashValue(8,
                                         "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                                         "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  database_->InsertChunks(safe_browsing_util::kExtensionBlacklist,
                          chunks.get());

  chunks.clear();
  chunks.push_back(AddChunkHashedIpValue(9, "::ffff:192.168.1.0", 120));
  database_->InsertChunks(safe_browsing_util::kIPBlacklist, chunks.get());

  database_->UpdateFinished(true);

  GetListsInfo(&lists);
  ASSERT_EQ(7U, lists.size());
  EXPECT_EQ(safe_browsing_util::kMalwareList, lists[0].name);
  EXPECT_EQ("1", lists[0].adds);
  EXPECT_TRUE(lists[0].subs.empty());
  EXPECT_EQ(safe_browsing_util::kPhishingList, lists[1].name);
  EXPECT_EQ("2", lists[1].adds);
  EXPECT_TRUE(lists[1].subs.empty());
  EXPECT_EQ(safe_browsing_util::kBinUrlList, lists[2].name);
  EXPECT_EQ("3", lists[2].adds);
  EXPECT_TRUE(lists[2].subs.empty());
  EXPECT_EQ(safe_browsing_util::kCsdWhiteList, lists[3].name);
  EXPECT_EQ("5", lists[3].adds);
  EXPECT_TRUE(lists[3].subs.empty());
  EXPECT_EQ(safe_browsing_util::kDownloadWhiteList, lists[4].name);
  EXPECT_EQ("6", lists[4].adds);
  EXPECT_TRUE(lists[4].subs.empty());
  EXPECT_EQ(safe_browsing_util::kExtensionBlacklist, lists[5].name);
  EXPECT_EQ("8", lists[5].adds);
  EXPECT_TRUE(lists[5].subs.empty());
  EXPECT_EQ(safe_browsing_util::kIPBlacklist, lists[6].name);
  EXPECT_EQ("9", lists[6].adds);
  EXPECT_TRUE(lists[6].subs.empty());

  database_.reset();
}

// Checks database reading and writing for browse.
TEST_F(SafeBrowsingDatabaseTest, BrowseDatabase) {
  std::vector<SBListChunkRanges> lists;
  ScopedVector<SBChunkData> chunks;

  chunks.push_back(AddChunkPrefix2Value(1,
                                        "www.evil.com/phishing.html",
                                        "www.evil.com/malware.html"));
  chunks.push_back(AddChunkPrefix4Value(2,
                                        "www.evil.com/notevil1.html",
                                        "www.evil.com/notevil2.html",
                                        "www.good.com/good1.html",
                                        "www.good.com/good2.html"));
  chunks.push_back(AddChunkPrefixValue(3, "192.168.0.1/malware.html"));
  chunks.push_back(AddChunkFullHashValue(7, "www.evil.com/evil.html"));

  ASSERT_TRUE(database_->UpdateStarted(&lists));
  database_->InsertChunks(safe_browsing_util::kMalwareList, chunks.get());
  database_->UpdateFinished(true);

  // Make sure they were added correctly.
  GetListsInfo(&lists);
  ASSERT_LE(1U, lists.size());
  EXPECT_EQ(safe_browsing_util::kMalwareList, lists[0].name);
  EXPECT_EQ("1-3,7", lists[0].adds);
  EXPECT_TRUE(lists[0].subs.empty());

  std::vector<SBPrefix> prefix_hits;
  std::vector<SBFullHashResult> cache_hits;
  EXPECT_TRUE(database_->ContainsBrowseUrl(
      GURL("http://www.evil.com/phishing.html"), &prefix_hits, &cache_hits));
  ASSERT_EQ(1U, prefix_hits.size());
  EXPECT_EQ(SBPrefixForString("www.evil.com/phishing.html"), prefix_hits[0]);
  EXPECT_TRUE(cache_hits.empty());

  EXPECT_TRUE(database_->ContainsBrowseUrl(
      GURL("http://www.evil.com/malware.html"), &prefix_hits, &cache_hits));

  EXPECT_TRUE(database_->ContainsBrowseUrl(
      GURL("http://www.evil.com/notevil1.html"), &prefix_hits, &cache_hits));

  EXPECT_TRUE(database_->ContainsBrowseUrl(
      GURL("http://www.evil.com/notevil2.html"), &prefix_hits, &cache_hits));

  EXPECT_TRUE(database_->ContainsBrowseUrl(
      GURL("http://www.good.com/good1.html"), &prefix_hits, &cache_hits));

  EXPECT_TRUE(database_->ContainsBrowseUrl(
      GURL("http://www.good.com/good2.html"), &prefix_hits, &cache_hits));

  EXPECT_TRUE(database_->ContainsBrowseUrl(
      GURL("http://192.168.0.1/malware.html"), &prefix_hits, &cache_hits));

  EXPECT_FALSE(database_->ContainsBrowseUrl(
      GURL("http://www.evil.com/"), &prefix_hits, &cache_hits));
  EXPECT_TRUE(prefix_hits.empty());
  EXPECT_TRUE(cache_hits.empty());

  EXPECT_FALSE(database_->ContainsBrowseUrl(
      GURL("http://www.evil.com/robots.txt"), &prefix_hits, &cache_hits));

  EXPECT_TRUE(database_->ContainsBrowseUrl(
      GURL("http://www.evil.com/evil.html"), &prefix_hits, &cache_hits));
  ASSERT_EQ(1U, prefix_hits.size());
  EXPECT_EQ(SBPrefixForString("www.evil.com/evil.html"), prefix_hits[0]);

  // Attempt to re-add the first chunk (should be a no-op).
  // see bug: http://code.google.com/p/chromium/issues/detail?id=4522
  chunks.clear();
  chunks.push_back(AddChunkPrefix2Value(1,
                                        "www.evil.com/phishing.html",
                                        "www.evil.com/malware.html"));
  ASSERT_TRUE(database_->UpdateStarted(&lists));
  database_->InsertChunks(safe_browsing_util::kMalwareList, chunks.get());
  database_->UpdateFinished(true);

  GetListsInfo(&lists);
  ASSERT_LE(1U, lists.size());
  EXPECT_EQ(safe_browsing_util::kMalwareList, lists[0].name);
  EXPECT_EQ("1-3,7", lists[0].adds);
  EXPECT_TRUE(lists[0].subs.empty());

  // Test removing a single prefix from the add chunk.
  chunks.clear();
  chunks.push_back(SubChunkPrefixValue(4, "www.evil.com/notevil1.html", 2));
  ASSERT_TRUE(database_->UpdateStarted(&lists));
  database_->InsertChunks(safe_browsing_util::kMalwareList, chunks.get());
  database_->UpdateFinished(true);

  EXPECT_TRUE(database_->ContainsBrowseUrl(
      GURL("http://www.evil.com/phishing.html"), &prefix_hits, &cache_hits));
  ASSERT_EQ(1U, prefix_hits.size());
  EXPECT_EQ(SBPrefixForString("www.evil.com/phishing.html"), prefix_hits[0]);
  EXPECT_TRUE(cache_hits.empty());

  EXPECT_FALSE(database_->ContainsBrowseUrl(
      GURL("http://www.evil.com/notevil1.html"), &prefix_hits, &cache_hits));
  EXPECT_TRUE(prefix_hits.empty());
  EXPECT_TRUE(cache_hits.empty());

  EXPECT_TRUE(database_->ContainsBrowseUrl(
      GURL("http://www.evil.com/notevil2.html"), &prefix_hits, &cache_hits));

  EXPECT_TRUE(database_->ContainsBrowseUrl(
      GURL("http://www.good.com/good1.html"), &prefix_hits, &cache_hits));

  EXPECT_TRUE(database_->ContainsBrowseUrl(
      GURL("http://www.good.com/good2.html"), &prefix_hits, &cache_hits));

  GetListsInfo(&lists);
  ASSERT_LE(1U, lists.size());
  EXPECT_EQ(safe_browsing_util::kMalwareList, lists[0].name);
  EXPECT_EQ("1-3,7", lists[0].adds);
  EXPECT_EQ("4", lists[0].subs);

  // Test the same sub chunk again.  This should be a no-op.
  // see bug: http://code.google.com/p/chromium/issues/detail?id=4522
  chunks.clear();
  chunks.push_back(SubChunkPrefixValue(4, "www.evil.com/notevil1.html", 2));

  ASSERT_TRUE(database_->UpdateStarted(&lists));
  database_->InsertChunks(safe_browsing_util::kMalwareList, chunks.get());
  database_->UpdateFinished(true);

  GetListsInfo(&lists);
  ASSERT_LE(1U, lists.size());
  EXPECT_EQ(safe_browsing_util::kMalwareList, lists[0].name);
  EXPECT_EQ("1-3,7", lists[0].adds);
  EXPECT_EQ("4", lists[0].subs);

  // Test removing all the prefixes from an add chunk.
  ASSERT_TRUE(database_->UpdateStarted(&lists));
  AddDelChunk(safe_browsing_util::kMalwareList, 2);
  database_->UpdateFinished(true);

  EXPECT_FALSE(database_->ContainsBrowseUrl(
      GURL("http://www.evil.com/notevil2.html"), &prefix_hits, &cache_hits));

  EXPECT_FALSE(database_->ContainsBrowseUrl(
      GURL("http://www.good.com/good1.html"), &prefix_hits, &cache_hits));

  EXPECT_FALSE(database_->ContainsBrowseUrl(
      GURL("http://www.good.com/good2.html"), &prefix_hits, &cache_hits));

  GetListsInfo(&lists);
  ASSERT_LE(1U, lists.size());
  EXPECT_EQ(safe_browsing_util::kMalwareList, lists[0].name);
  EXPECT_EQ("1,3,7", lists[0].adds);
  EXPECT_EQ("4", lists[0].subs);

  // The adddel command exposed a bug in the transaction code where any
  // transaction after it would fail.  Add a dummy entry and remove it to
  // make sure the transcation works fine.
  chunks.clear();
  chunks.push_back(AddChunkPrefixValue(44, "www.redherring.com/index.html"));
  ASSERT_TRUE(database_->UpdateStarted(&lists));
  database_->InsertChunks(safe_browsing_util::kMalwareList, chunks.get());

  // Now remove the dummy entry.  If there are any problems with the
  // transactions, asserts will fire.
  AddDelChunk(safe_browsing_util::kMalwareList, 44);

  // Test the subdel command.
  SubDelChunk(safe_browsing_util::kMalwareList, 4);
  database_->UpdateFinished(true);

  GetListsInfo(&lists);
  ASSERT_LE(1U, lists.size());
  EXPECT_EQ(safe_browsing_util::kMalwareList, lists[0].name);
  EXPECT_EQ("1,3,7", lists[0].adds);
  EXPECT_TRUE(lists[0].subs.empty());

  // Test a sub command coming in before the add.
  chunks.clear();
  chunks.push_back(SubChunkPrefix2Value(5,
                                        "www.notevilanymore.com/index.html",
                                        10,
                                        "www.notevilanymore.com/good.html",
                                        10));
  ASSERT_TRUE(database_->UpdateStarted(&lists));
  database_->InsertChunks(safe_browsing_util::kMalwareList, chunks.get());
  database_->UpdateFinished(true);

  EXPECT_FALSE(database_->ContainsBrowseUrl(
      GURL("http://www.notevilanymore.com/index.html"),
      &prefix_hits,
      &cache_hits));

  // Now insert the tardy add chunk and we don't expect them to appear
  // in database because of the previous sub chunk.
  chunks.clear();
  chunks.push_back(AddChunkPrefix2Value(10,
                                        "www.notevilanymore.com/index.html",
                                        "www.notevilanymore.com/good.html"));
  ASSERT_TRUE(database_->UpdateStarted(&lists));
  database_->InsertChunks(safe_browsing_util::kMalwareList, chunks.get());
  database_->UpdateFinished(true);

  EXPECT_FALSE(database_->ContainsBrowseUrl(
      GURL("http://www.notevilanymore.com/index.html"),
      &prefix_hits,
      &cache_hits));

  EXPECT_FALSE(database_->ContainsBrowseUrl(
      GURL("http://www.notevilanymore.com/good.html"),
      &prefix_hits,
      &cache_hits));

  // Reset and reload the database.  The database will rely on the prefix set.
  database_.reset(new SafeBrowsingDatabaseNew);
  database_->Init(database_filename_);

  // Check that a prefix still hits.
  EXPECT_TRUE(database_->ContainsBrowseUrl(
      GURL("http://www.evil.com/phishing.html"), &prefix_hits, &cache_hits));
  ASSERT_EQ(1U, prefix_hits.size());
  EXPECT_EQ(SBPrefixForString("www.evil.com/phishing.html"), prefix_hits[0]);

  // Also check that it's not just always returning true in this case.
  EXPECT_FALSE(database_->ContainsBrowseUrl(
      GURL("http://www.evil.com/"), &prefix_hits, &cache_hits));

  // Check that the full hash is still present.
  EXPECT_TRUE(database_->ContainsBrowseUrl(
      GURL("http://www.evil.com/evil.html"), &prefix_hits, &cache_hits));
  ASSERT_EQ(1U, prefix_hits.size());
  EXPECT_EQ(SBPrefixForString("www.evil.com/evil.html"), prefix_hits[0]);
}

// Test adding zero length chunks to the database.
TEST_F(SafeBrowsingDatabaseTest, ZeroSizeChunk) {
  std::vector<SBListChunkRanges> lists;
  ScopedVector<SBChunkData> chunks;

  // Populate with a couple of normal chunks.
  chunks.push_back(AddChunkPrefix2Value(1,
                                        "www.test.com/test1.html",
                                        "www.test.com/test2.html"));
  chunks.push_back(AddChunkPrefix2Value(10,
                                        "www.random.com/random1.html",
                                        "www.random.com/random2.html"));

  ASSERT_TRUE(database_->UpdateStarted(&lists));
  database_->InsertChunks(safe_browsing_util::kMalwareList, chunks.get());
  database_->UpdateFinished(true);

  // Add an empty ADD and SUB chunk.
  GetListsInfo(&lists);
  ASSERT_LE(1U, lists.size());
  EXPECT_EQ(safe_browsing_util::kMalwareList, lists[0].name);
  EXPECT_EQ("1,10", lists[0].adds);
  EXPECT_TRUE(lists[0].subs.empty());

  chunks.clear();
  chunks.push_back(BuildChunk(19, safe_browsing::ChunkData::ADD,
                              safe_browsing::ChunkData::PREFIX_4B,
                              NULL, 0, std::vector<int>()));
  chunks.push_back(BuildChunk(7, safe_browsing::ChunkData::SUB,
                              safe_browsing::ChunkData::PREFIX_4B,
                              NULL, 0, std::vector<int>()));

  ASSERT_TRUE(database_->UpdateStarted(&lists));
  database_->InsertChunks(safe_browsing_util::kMalwareList, chunks.get());
  database_->UpdateFinished(true);

  GetListsInfo(&lists);
  ASSERT_LE(1U, lists.size());
  EXPECT_EQ(safe_browsing_util::kMalwareList, lists[0].name);
  EXPECT_EQ("1,10,19", lists[0].adds);
  EXPECT_EQ("7", lists[0].subs);

  // Add an empty chunk along with a couple that contain data. This should
  // result in the chunk range being reduced in size.
  chunks.clear();
  chunks.push_back(AddChunkPrefixValue(20, "www.notempty.com/full1.html"));
  chunks.push_back(BuildChunk(21, safe_browsing::ChunkData::ADD,
                              safe_browsing::ChunkData::PREFIX_4B,
                              NULL, 0, std::vector<int>()));
  chunks.push_back(AddChunkPrefixValue(22, "www.notempty.com/full2.html"));

  ASSERT_TRUE(database_->UpdateStarted(&lists));
  database_->InsertChunks(safe_browsing_util::kMalwareList, chunks.get());
  database_->UpdateFinished(true);

  std::vector<SBPrefix> prefix_hits;
  std::vector<SBFullHashResult> cache_hits;
  EXPECT_TRUE(database_->ContainsBrowseUrl(
      GURL("http://www.notempty.com/full1.html"), &prefix_hits, &cache_hits));
  EXPECT_TRUE(database_->ContainsBrowseUrl(
      GURL("http://www.notempty.com/full2.html"), &prefix_hits, &cache_hits));

  GetListsInfo(&lists);
  ASSERT_LE(1U, lists.size());
  EXPECT_EQ(safe_browsing_util::kMalwareList, lists[0].name);
  EXPECT_EQ("1,10,19-22", lists[0].adds);
  EXPECT_EQ("7", lists[0].subs);

  // Handle AddDel and SubDel commands for empty chunks.
  ASSERT_TRUE(database_->UpdateStarted(&lists));
  AddDelChunk(safe_browsing_util::kMalwareList, 21);
  database_->UpdateFinished(true);

  GetListsInfo(&lists);
  ASSERT_LE(1U, lists.size());
  EXPECT_EQ(safe_browsing_util::kMalwareList, lists[0].name);
  EXPECT_EQ("1,10,19-20,22", lists[0].adds);
  EXPECT_EQ("7", lists[0].subs);

  ASSERT_TRUE(database_->UpdateStarted(&lists));
  SubDelChunk(safe_browsing_util::kMalwareList, 7);
  database_->UpdateFinished(true);

  GetListsInfo(&lists);
  ASSERT_LE(1U, lists.size());
  EXPECT_EQ(safe_browsing_util::kMalwareList, lists[0].name);
  EXPECT_EQ("1,10,19-20,22", lists[0].adds);
  EXPECT_TRUE(lists[0].subs.empty());
}

// Utility function for setting up the database for the caching test.
void SafeBrowsingDatabaseTest::PopulateDatabaseForCacheTest() {
  // Add a couple prefixes.
  ScopedVector<SBChunkData> chunks;
  chunks.push_back(AddChunkPrefix2Value(1,
                                        "www.evil.com/phishing.html",
                                        "www.evil.com/malware.html"));

  std::vector<SBListChunkRanges> lists;
  ASSERT_TRUE(database_->UpdateStarted(&lists));
  database_->InsertChunks(safe_browsing_util::kMalwareList, chunks.get());
  database_->UpdateFinished(true);

  // Cache should be cleared after updating.
  EXPECT_TRUE(database_->browse_gethash_cache_.empty());

  SBFullHashResult full_hash;
  full_hash.list_id = safe_browsing_util::MALWARE;

  std::vector<SBFullHashResult> results;
  std::vector<SBPrefix> prefixes;

  // Add a fullhash result for each prefix.
  full_hash.hash = SBFullHashForString("www.evil.com/phishing.html");
  results.push_back(full_hash);
  prefixes.push_back(full_hash.hash.prefix);

  full_hash.hash = SBFullHashForString("www.evil.com/malware.html");
  results.push_back(full_hash);
  prefixes.push_back(full_hash.hash.prefix);

  database_->CacheHashResults(prefixes, results, kCacheLifetime);
}

TEST_F(SafeBrowsingDatabaseTest, HashCaching) {
  PopulateDatabaseForCacheTest();

  // We should have both full hashes in the cache.
  EXPECT_EQ(2U, database_->browse_gethash_cache_.size());

  // Test the cache lookup for the first prefix.
  std::vector<SBPrefix> prefix_hits;
  std::vector<SBFullHashResult> cache_hits;
  EXPECT_TRUE(database_->ContainsBrowseUrl(
      GURL("http://www.evil.com/phishing.html"), &prefix_hits, &cache_hits));
  EXPECT_TRUE(prefix_hits.empty());
  ASSERT_EQ(1U, cache_hits.size());
  EXPECT_TRUE(SBFullHashEqual(
      cache_hits[0].hash, SBFullHashForString("www.evil.com/phishing.html")));

  prefix_hits.clear();
  cache_hits.clear();

  // Test the cache lookup for the second prefix.
  EXPECT_TRUE(database_->ContainsBrowseUrl(
      GURL("http://www.evil.com/malware.html"), &prefix_hits, &cache_hits));
  EXPECT_TRUE(prefix_hits.empty());
  ASSERT_EQ(1U, cache_hits.size());
  EXPECT_TRUE(SBFullHashEqual(
      cache_hits[0].hash, SBFullHashForString("www.evil.com/malware.html")));

  prefix_hits.clear();
  cache_hits.clear();

  // Test removing a prefix via a sub chunk.
  ScopedVector<SBChunkData> chunks;
  chunks.push_back(SubChunkPrefixValue(2, "www.evil.com/phishing.html", 1));

  std::vector<SBListChunkRanges> lists;
  ASSERT_TRUE(database_->UpdateStarted(&lists));
  database_->InsertChunks(safe_browsing_util::kMalwareList, chunks.get());
  database_->UpdateFinished(true);

  // This prefix should still be there, but cached fullhash should be gone.
  EXPECT_TRUE(database_->ContainsBrowseUrl(
      GURL("http://www.evil.com/malware.html"), &prefix_hits, &cache_hits));
  ASSERT_EQ(1U, prefix_hits.size());
  EXPECT_EQ(SBPrefixForString("www.evil.com/malware.html"), prefix_hits[0]);
  EXPECT_TRUE(cache_hits.empty());
  prefix_hits.clear();
  cache_hits.clear();

  // This prefix should be gone.
  EXPECT_FALSE(database_->ContainsBrowseUrl(
      GURL("http://www.evil.com/phishing.html"), &prefix_hits, &cache_hits));
  prefix_hits.clear();
  cache_hits.clear();

  // Test that an AddDel for the original chunk removes the last cached entry.
  ASSERT_TRUE(database_->UpdateStarted(&lists));
  AddDelChunk(safe_browsing_util::kMalwareList, 1);
  database_->UpdateFinished(true);
  EXPECT_FALSE(database_->ContainsBrowseUrl(
      GURL("http://www.evil.com/malware.html"), &prefix_hits, &cache_hits));
  EXPECT_TRUE(database_->browse_gethash_cache_.empty());
  prefix_hits.clear();
  cache_hits.clear();

  // Test that the cache won't return expired values. First we have to adjust
  // the cached entries' received time to make them older, since the database
  // cache insert uses Time::Now(). First, store some entries.
  PopulateDatabaseForCacheTest();

  std::map<SBPrefix, SBCachedFullHashResult>* hash_cache =
      &database_->browse_gethash_cache_;
  EXPECT_EQ(2U, hash_cache->size());

  // Now adjust one of the entries times to be in the past.
  const SBPrefix key = SBPrefixForString("www.evil.com/malware.html");
  std::map<SBPrefix, SBCachedFullHashResult>::iterator iter =
      hash_cache->find(key);
  ASSERT_TRUE(iter != hash_cache->end());
  iter->second.expire_after = Time::Now() - TimeDelta::FromMinutes(1);

  EXPECT_TRUE(database_->ContainsBrowseUrl(
      GURL("http://www.evil.com/malware.html"), &prefix_hits, &cache_hits));
  EXPECT_EQ(1U, prefix_hits.size());
  EXPECT_TRUE(cache_hits.empty());
  // Expired entry should have been removed from cache.
  EXPECT_EQ(1U, hash_cache->size());

  // This entry should still exist.
  EXPECT_TRUE(database_->ContainsBrowseUrl(
      GURL("http://www.evil.com/phishing.html"), &prefix_hits, &cache_hits));
  EXPECT_TRUE(prefix_hits.empty());
  EXPECT_EQ(1U, cache_hits.size());

  // Testing prefix miss caching. First, we clear out the existing database,
  // Since PopulateDatabaseForCacheTest() doesn't handle adding duplicate
  // chunks.
  ASSERT_TRUE(database_->UpdateStarted(&lists));
  AddDelChunk(safe_browsing_util::kMalwareList, 1);
  database_->UpdateFinished(true);

  // Cache should be cleared after updating.
  EXPECT_TRUE(hash_cache->empty());

  std::vector<SBPrefix> prefix_misses;
  std::vector<SBFullHashResult> empty_full_hash;
  prefix_misses.push_back(SBPrefixForString("http://www.bad.com/malware.html"));
  prefix_misses.push_back(
      SBPrefixForString("http://www.bad.com/phishing.html"));
  database_->CacheHashResults(prefix_misses, empty_full_hash, kCacheLifetime);

  // Prefixes with no full results are misses.
  EXPECT_EQ(hash_cache->size(), prefix_misses.size());
  ASSERT_TRUE(
      hash_cache->count(SBPrefixForString("http://www.bad.com/malware.html")));
  EXPECT_TRUE(
      hash_cache->find(SBPrefixForString("http://www.bad.com/malware.html"))
          ->second.full_hashes.empty());
  ASSERT_TRUE(
      hash_cache->count(SBPrefixForString("http://www.bad.com/phishing.html")));
  EXPECT_TRUE(
      hash_cache->find(SBPrefixForString("http://www.bad.com/phishing.html"))
          ->second.full_hashes.empty());

  // Update the database.
  PopulateDatabaseForCacheTest();

  // Cache a GetHash miss for a particular prefix, and even though the prefix is
  // in the database, it is flagged as a miss so looking up the associated URL
  // will not succeed.
  prefix_hits.clear();
  cache_hits.clear();
  prefix_misses.clear();
  empty_full_hash.clear();
  prefix_misses.push_back(SBPrefixForString("www.evil.com/phishing.html"));
  database_->CacheHashResults(prefix_misses, empty_full_hash, kCacheLifetime);
  EXPECT_FALSE(database_->ContainsBrowseUrl(
      GURL("http://www.evil.com/phishing.html"), &prefix_hits, &cache_hits));
  prefix_hits.clear();
  cache_hits.clear();

  // Test receiving a full add chunk.
  chunks.clear();
  chunks.push_back(AddChunkFullHash2Value(20,
                                          "www.fullevil.com/bad1.html",
                                          "www.fullevil.com/bad2.html"));
  ASSERT_TRUE(database_->UpdateStarted(&lists));
  database_->InsertChunks(safe_browsing_util::kMalwareList, chunks.get());
  database_->UpdateFinished(true);

  EXPECT_TRUE(database_->ContainsBrowseUrl(
      GURL("http://www.fullevil.com/bad1.html"), &prefix_hits, &cache_hits));
  ASSERT_EQ(1U, prefix_hits.size());
  EXPECT_EQ(SBPrefixForString("www.fullevil.com/bad1.html"), prefix_hits[0]);
  EXPECT_TRUE(cache_hits.empty());
  prefix_hits.clear();
  cache_hits.clear();

  EXPECT_TRUE(database_->ContainsBrowseUrl(
      GURL("http://www.fullevil.com/bad2.html"), &prefix_hits, &cache_hits));
  ASSERT_EQ(1U, prefix_hits.size());
  EXPECT_EQ(SBPrefixForString("www.fullevil.com/bad2.html"), prefix_hits[0]);
  EXPECT_TRUE(cache_hits.empty());
  prefix_hits.clear();
  cache_hits.clear();

  // Test receiving a full sub chunk, which will remove one of the full adds.
  chunks.clear();
  chunks.push_back(SubChunkFullHashValue(200,
                                         "www.fullevil.com/bad1.html",
                                         20));
  ASSERT_TRUE(database_->UpdateStarted(&lists));
  database_->InsertChunks(safe_browsing_util::kMalwareList, chunks.get());
  database_->UpdateFinished(true);

  EXPECT_FALSE(database_->ContainsBrowseUrl(
      GURL("http://www.fullevil.com/bad1.html"), &prefix_hits, &cache_hits));

  // There should be one remaining full add.
  EXPECT_TRUE(database_->ContainsBrowseUrl(
      GURL("http://www.fullevil.com/bad2.html"), &prefix_hits, &cache_hits));
  ASSERT_EQ(1U, prefix_hits.size());
  EXPECT_EQ(SBPrefixForString("www.fullevil.com/bad2.html"), prefix_hits[0]);
  EXPECT_TRUE(cache_hits.empty());
  prefix_hits.clear();
  cache_hits.clear();

  // Now test an AddDel for the remaining full add.
  ASSERT_TRUE(database_->UpdateStarted(&lists));
  AddDelChunk(safe_browsing_util::kMalwareList, 20);
  database_->UpdateFinished(true);

  EXPECT_FALSE(database_->ContainsBrowseUrl(
      GURL("http://www.fullevil.com/bad1.html"), &prefix_hits, &cache_hits));
  EXPECT_FALSE(database_->ContainsBrowseUrl(
      GURL("http://www.fullevil.com/bad2.html"), &prefix_hits, &cache_hits));

  // Add a fullhash which has a prefix collision for a known url.
  static const char kExampleFine[] = "www.example.com/fine.html";
  static const char kExampleCollision[] =
      "www.example.com/3123364814/malware.htm";
  ASSERT_EQ(SBPrefixForString(kExampleFine),
            SBPrefixForString(kExampleCollision));
  ASSERT_TRUE(database_->UpdateStarted(&lists));
  {
    ScopedVector<SBChunkData> chunks;
    chunks.push_back(AddChunkPrefixValue(21, kExampleCollision));
    database_->InsertChunks(safe_browsing_util::kMalwareList, chunks.get());
  }
  database_->UpdateFinished(true);

  // Expect a prefix hit due to the collision between |kExampleFine| and
  // |kExampleCollision|.
  EXPECT_TRUE(database_->ContainsBrowseUrl(
      GURL(std::string("http://") + kExampleFine), &prefix_hits, &cache_hits));
  ASSERT_EQ(1U, prefix_hits.size());
  EXPECT_EQ(SBPrefixForString(kExampleFine), prefix_hits[0]);
  EXPECT_TRUE(cache_hits.empty());

  // Cache gethash response for |kExampleCollision|.
  {
    SBFullHashResult result;
    result.hash = SBFullHashForString(kExampleCollision);
    result.list_id = safe_browsing_util::MALWARE;
    database_->CacheHashResults(std::vector<SBPrefix>(1, result.hash.prefix),
                                std::vector<SBFullHashResult>(1, result),
                                kCacheLifetime);
  }

  // The cached response means the collision no longer causes a hit.
  EXPECT_FALSE(database_->ContainsBrowseUrl(
      GURL(std::string("http://") + kExampleFine), &prefix_hits, &cache_hits));
}

// Test that corrupt databases are appropriately handled, even if the
// corruption is detected in the midst of the update.
// TODO(shess): Disabled until ScopedLogMessageIgnorer resolved.
// http://crbug.com/56448
TEST_F(SafeBrowsingDatabaseTest, DISABLED_FileCorruptionHandling) {
  // Re-create the database in a captive message loop so that we can
  // influence task-posting.  Database specifically needs to the
  // file-backed.
  database_.reset();
  base::MessageLoop loop;
  SafeBrowsingStoreFile* store = new SafeBrowsingStoreFile();
  database_.reset(new SafeBrowsingDatabaseNew(store, NULL, NULL, NULL, NULL,
                                              NULL, NULL));
  database_->Init(database_filename_);

  // This will cause an empty database to be created.
  std::vector<SBListChunkRanges> lists;
  ASSERT_TRUE(database_->UpdateStarted(&lists));
  database_->UpdateFinished(true);

  // Create a sub chunk to insert.
  ScopedVector<SBChunkData> chunks;
  chunks.push_back(SubChunkPrefixValue(7,
                                       "www.subbed.com/notevil1.html",
                                       19));

  // Corrupt the file by corrupting the checksum, which is not checked
  // until the entire table is read in |UpdateFinished()|.
  FILE* fp = base::OpenFile(database_filename_, "r+");
  ASSERT_TRUE(fp);
  ASSERT_NE(-1, fseek(fp, -8, SEEK_END));
  for (size_t i = 0; i < 8; ++i) {
    fputc('!', fp);
  }
  fclose(fp);

  {
    // The following code will cause DCHECKs, so suppress the crashes.
    ScopedLogMessageIgnorer ignorer;

    // Start an update.  The insert will fail due to corruption.
    ASSERT_TRUE(database_->UpdateStarted(&lists));
    database_->InsertChunks(safe_browsing_util::kMalwareList, chunks.get());
    database_->UpdateFinished(true);

    // Database file still exists until the corruption handler has run.
    EXPECT_TRUE(base::PathExists(database_filename_));

    // Flush through the corruption-handler task.
    VLOG(1) << "Expect failed check on: SafeBrowsing database reset";
    base::MessageLoop::current()->RunUntilIdle();
  }

  // Database file should not exist.
  EXPECT_FALSE(base::PathExists(database_filename_));

  // Run the update again successfully.
  ASSERT_TRUE(database_->UpdateStarted(&lists));
  database_->InsertChunks(safe_browsing_util::kMalwareList, chunks.get());
  database_->UpdateFinished(true);
  EXPECT_TRUE(base::PathExists(database_filename_));

  database_.reset();
}

// Checks database reading and writing.
TEST_F(SafeBrowsingDatabaseTest, ContainsDownloadUrl) {
  database_.reset();
  base::MessageLoop loop;
  SafeBrowsingStoreFile* browse_store = new SafeBrowsingStoreFile();
  SafeBrowsingStoreFile* download_store = new SafeBrowsingStoreFile();
  SafeBrowsingStoreFile* csd_whitelist_store = new SafeBrowsingStoreFile();
  database_.reset(new SafeBrowsingDatabaseNew(browse_store,
                                              download_store,
                                              csd_whitelist_store,
                                              NULL,
                                              NULL,
                                              NULL,
                                              NULL));
  database_->Init(database_filename_);

  const char kEvil1Url1[] = "www.evil1.com/download1/";
  const char kEvil1Url2[] = "www.evil1.com/download2.html";

  // Add a simple chunk with one hostkey for download url list.
  ScopedVector<SBChunkData> chunks;
  chunks.push_back(AddChunkPrefix2Value(1, kEvil1Url1, kEvil1Url2));

  std::vector<SBListChunkRanges> lists;
  ASSERT_TRUE(database_->UpdateStarted(&lists));
  database_->InsertChunks(safe_browsing_util::kBinUrlList, chunks.get());
  database_->UpdateFinished(true);

  std::vector<SBPrefix> prefix_hits;
  std::vector<GURL> urls(1);

  urls[0] = GURL(std::string("http://") + kEvil1Url1);
  EXPECT_TRUE(database_->ContainsDownloadUrl(urls, &prefix_hits));
  ASSERT_EQ(1U, prefix_hits.size());
  EXPECT_EQ(SBPrefixForString(kEvil1Url1), prefix_hits[0]);

  urls[0] = GURL(std::string("http://") + kEvil1Url2);
  EXPECT_TRUE(database_->ContainsDownloadUrl(urls, &prefix_hits));
  ASSERT_EQ(1U, prefix_hits.size());
  EXPECT_EQ(SBPrefixForString(kEvil1Url2), prefix_hits[0]);

  urls[0] = GURL(std::string("https://") + kEvil1Url2);
  EXPECT_TRUE(database_->ContainsDownloadUrl(urls, &prefix_hits));
  ASSERT_EQ(1U, prefix_hits.size());
  EXPECT_EQ(SBPrefixForString(kEvil1Url2), prefix_hits[0]);

  urls[0] = GURL(std::string("ftp://") + kEvil1Url2);
  EXPECT_TRUE(database_->ContainsDownloadUrl(urls, &prefix_hits));
  ASSERT_EQ(1U, prefix_hits.size());
  EXPECT_EQ(SBPrefixForString(kEvil1Url2), prefix_hits[0]);

  urls[0] = GURL("http://www.randomevil.com");
  EXPECT_FALSE(database_->ContainsDownloadUrl(urls, &prefix_hits));

  // Should match with query args stripped.
  urls[0] = GURL(std::string("http://") + kEvil1Url2 + "?blah");
  EXPECT_TRUE(database_->ContainsDownloadUrl(urls, &prefix_hits));
  ASSERT_EQ(1U, prefix_hits.size());
  EXPECT_EQ(SBPrefixForString(kEvil1Url2), prefix_hits[0]);

  // Should match with extra path stuff and query args stripped.
  urls[0] = GURL(std::string("http://") + kEvil1Url1 + "foo/bar?blah");
  EXPECT_TRUE(database_->ContainsDownloadUrl(urls, &prefix_hits));
  ASSERT_EQ(1U, prefix_hits.size());
  EXPECT_EQ(SBPrefixForString(kEvil1Url1), prefix_hits[0]);

  // First hit in redirect chain is malware.
  urls.clear();
  urls.push_back(GURL(std::string("http://") + kEvil1Url1));
  urls.push_back(GURL("http://www.randomevil.com"));
  EXPECT_TRUE(database_->ContainsDownloadUrl(urls, &prefix_hits));
  ASSERT_EQ(1U, prefix_hits.size());
  EXPECT_EQ(SBPrefixForString(kEvil1Url1), prefix_hits[0]);

  // Middle hit in redirect chain is malware.
  urls.clear();
  urls.push_back(GURL("http://www.randomevil.com"));
  urls.push_back(GURL(std::string("http://") + kEvil1Url1));
  urls.push_back(GURL("http://www.randomevil2.com"));
  EXPECT_TRUE(database_->ContainsDownloadUrl(urls, &prefix_hits));
  ASSERT_EQ(1U, prefix_hits.size());
  EXPECT_EQ(SBPrefixForString(kEvil1Url1), prefix_hits[0]);

  // Final hit in redirect chain is malware.
  urls.clear();
  urls.push_back(GURL("http://www.randomevil.com"));
  urls.push_back(GURL(std::string("http://") + kEvil1Url1));
  EXPECT_TRUE(database_->ContainsDownloadUrl(urls, &prefix_hits));
  ASSERT_EQ(1U, prefix_hits.size());
  EXPECT_EQ(SBPrefixForString(kEvil1Url1), prefix_hits[0]);

  // Multiple hits in redirect chain are in malware list.
  urls.clear();
  urls.push_back(GURL(std::string("http://") + kEvil1Url1));
  urls.push_back(GURL(std::string("https://") + kEvil1Url2));
  EXPECT_TRUE(database_->ContainsDownloadUrl(urls, &prefix_hits));
  ASSERT_EQ(2U, prefix_hits.size());
  EXPECT_EQ(SBPrefixForString(kEvil1Url1), prefix_hits[0]);
  EXPECT_EQ(SBPrefixForString(kEvil1Url2), prefix_hits[1]);
  database_.reset();
}

// Checks that the whitelists are handled properly.
TEST_F(SafeBrowsingDatabaseTest, Whitelists) {
  database_.reset();

  // We expect all calls to ContainsCsdWhitelistedUrl in particular to be made
  // from the IO thread.  In general the whitelist lookups are thread-safe.
  content::TestBrowserThreadBundle thread_bundle_;

  // If the whitelist is disabled everything should match the whitelist.
  database_.reset(new SafeBrowsingDatabaseNew(new SafeBrowsingStoreFile(),
                                              NULL, NULL, NULL, NULL, NULL,
                                              NULL));
  database_->Init(database_filename_);
  EXPECT_TRUE(database_->ContainsDownloadWhitelistedUrl(
      GURL(std::string("http://www.phishing.com/"))));
  EXPECT_TRUE(database_->ContainsDownloadWhitelistedUrl(
      GURL(std::string("http://www.phishing.com/"))));
  EXPECT_TRUE(database_->ContainsDownloadWhitelistedString("asdf"));

  SafeBrowsingStoreFile* browse_store = new SafeBrowsingStoreFile();
  SafeBrowsingStoreFile* csd_whitelist_store = new SafeBrowsingStoreFile();
  SafeBrowsingStoreFile* download_whitelist_store = new SafeBrowsingStoreFile();
  SafeBrowsingStoreFile* extension_blacklist_store =
      new SafeBrowsingStoreFile();
  database_.reset(new SafeBrowsingDatabaseNew(browse_store, NULL,
                                              csd_whitelist_store,
                                              download_whitelist_store,
                                              extension_blacklist_store,
                                              NULL, NULL));
  database_->Init(database_filename_);

  const char kGood1Host[] = "www.good1.com/";
  const char kGood1Url1[] = "www.good1.com/a/b.html";
  const char kGood1Url2[] = "www.good1.com/b/";

  const char kGood2Url1[] = "www.good2.com/c";  // Should match '/c/bla'.

  // good3.com/a/b/c/d/e/f/g/ should match because it's a whitelist.
  const char kGood3Url1[] = "good3.com/";

  const char kGoodString[] = "good_string";

  ScopedVector<SBChunkData> csd_chunks;
  ScopedVector<SBChunkData> download_chunks;

  // Add two simple chunks to the csd whitelist.
  csd_chunks.push_back(AddChunkFullHash2Value(1, kGood1Url1, kGood1Url2));
  csd_chunks.push_back(AddChunkFullHashValue(2, kGood2Url1));
  download_chunks.push_back(AddChunkFullHashValue(2, kGood2Url1));
  download_chunks.push_back(AddChunkFullHashValue(3, kGoodString));
  download_chunks.push_back(AddChunkFullHashValue(4, kGood3Url1));

  std::vector<SBListChunkRanges> lists;
  ASSERT_TRUE(database_->UpdateStarted(&lists));
  database_->InsertChunks(safe_browsing_util::kCsdWhiteList, csd_chunks.get());
  database_->InsertChunks(safe_browsing_util::kDownloadWhiteList,
                          download_chunks.get());
  database_->UpdateFinished(true);

  EXPECT_FALSE(database_->ContainsCsdWhitelistedUrl(
      GURL(std::string("http://") + kGood1Host)));

  EXPECT_TRUE(database_->ContainsCsdWhitelistedUrl(
      GURL(std::string("http://") + kGood1Url1)));
  EXPECT_TRUE(database_->ContainsCsdWhitelistedUrl(
      GURL(std::string("http://") + kGood1Url1 + "?a=b")));

  EXPECT_TRUE(database_->ContainsCsdWhitelistedUrl(
      GURL(std::string("http://") + kGood1Url2)));
  EXPECT_TRUE(database_->ContainsCsdWhitelistedUrl(
      GURL(std::string("http://") + kGood1Url2 + "/c.html")));

  EXPECT_TRUE(database_->ContainsCsdWhitelistedUrl(
      GURL(std::string("https://") + kGood1Url2 + "/c.html")));

  EXPECT_TRUE(database_->ContainsCsdWhitelistedUrl(
      GURL(std::string("http://") + kGood2Url1 + "/c")));
  EXPECT_TRUE(database_->ContainsCsdWhitelistedUrl(
      GURL(std::string("http://") + kGood2Url1 + "/c?bla")));
  EXPECT_TRUE(database_->ContainsCsdWhitelistedUrl(
      GURL(std::string("http://") + kGood2Url1 + "/c/bla")));

  EXPECT_FALSE(database_->ContainsCsdWhitelistedUrl(
      GURL(std::string("http://www.google.com/"))));

  EXPECT_TRUE(database_->ContainsDownloadWhitelistedUrl(
      GURL(std::string("http://") + kGood2Url1 + "/c")));
  EXPECT_TRUE(database_->ContainsDownloadWhitelistedUrl(
      GURL(std::string("http://") + kGood2Url1 + "/c?bla")));
  EXPECT_TRUE(database_->ContainsDownloadWhitelistedUrl(
      GURL(std::string("http://") + kGood2Url1 + "/c/bla")));

  EXPECT_TRUE(database_->ContainsDownloadWhitelistedUrl(
      GURL(std::string("http://good3.com/a/b/c/d/e/f/g/"))));
  EXPECT_TRUE(database_->ContainsDownloadWhitelistedUrl(
      GURL(std::string("http://a.b.good3.com/"))));

  EXPECT_FALSE(database_->ContainsDownloadWhitelistedString("asdf"));
  EXPECT_TRUE(database_->ContainsDownloadWhitelistedString(kGoodString));

  EXPECT_FALSE(database_->ContainsDownloadWhitelistedUrl(
      GURL(std::string("http://www.google.com/"))));

  // The CSD whitelist killswitch is not present.
  EXPECT_FALSE(database_->IsCsdWhitelistKillSwitchOn());

  // Test only add the malware IP killswitch
  csd_chunks.clear();
  csd_chunks.push_back(AddChunkFullHashValue(
      15, "sb-ssl.google.com/safebrowsing/csd/killswitch_malware"));

  ASSERT_TRUE(database_->UpdateStarted(&lists));
  database_->InsertChunks(safe_browsing_util::kCsdWhiteList, csd_chunks.get());
  database_->UpdateFinished(true);

  EXPECT_TRUE(database_->IsMalwareIPMatchKillSwitchOn());
  // The CSD whitelist killswitch is not present.
  EXPECT_FALSE(database_->IsCsdWhitelistKillSwitchOn());

  // Test that the kill-switch works as intended.
  csd_chunks.clear();
  download_chunks.clear();
  lists.clear();
  csd_chunks.push_back(AddChunkFullHashValue(
      5, "sb-ssl.google.com/safebrowsing/csd/killswitch"));
  download_chunks.push_back(AddChunkFullHashValue(
      5, "sb-ssl.google.com/safebrowsing/csd/killswitch"));

  ASSERT_TRUE(database_->UpdateStarted(&lists));
  database_->InsertChunks(safe_browsing_util::kCsdWhiteList, csd_chunks.get());
  database_->InsertChunks(safe_browsing_util::kDownloadWhiteList,
                          download_chunks.get());
  database_->UpdateFinished(true);

  // The CSD whitelist killswitch is present.
  EXPECT_TRUE(database_->IsCsdWhitelistKillSwitchOn());
  EXPECT_TRUE(database_->IsMalwareIPMatchKillSwitchOn());
  EXPECT_TRUE(database_->ContainsCsdWhitelistedUrl(
      GURL(std::string("https://") + kGood1Url2 + "/c.html")));
  EXPECT_TRUE(database_->ContainsCsdWhitelistedUrl(
      GURL(std::string("http://www.google.com/"))));
  EXPECT_TRUE(database_->ContainsCsdWhitelistedUrl(
      GURL(std::string("http://www.phishing_url.com/"))));

  EXPECT_TRUE(database_->ContainsDownloadWhitelistedUrl(
      GURL(std::string("https://") + kGood1Url2 + "/c.html")));
  EXPECT_TRUE(database_->ContainsDownloadWhitelistedUrl(
      GURL(std::string("http://www.google.com/"))));
  EXPECT_TRUE(database_->ContainsDownloadWhitelistedUrl(
      GURL(std::string("http://www.phishing_url.com/"))));

  EXPECT_TRUE(database_->ContainsDownloadWhitelistedString("asdf"));
  EXPECT_TRUE(database_->ContainsDownloadWhitelistedString(kGoodString));

  // Remove the kill-switch and verify that we can recover.
  csd_chunks.clear();
  download_chunks.clear();
  lists.clear();
  csd_chunks.push_back(SubChunkFullHashValue(
      1, "sb-ssl.google.com/safebrowsing/csd/killswitch", 5));
  csd_chunks.push_back(SubChunkFullHashValue(
      10, "sb-ssl.google.com/safebrowsing/csd/killswitch_malware", 15));
  download_chunks.push_back(SubChunkFullHashValue(
      1, "sb-ssl.google.com/safebrowsing/csd/killswitch", 5));

  ASSERT_TRUE(database_->UpdateStarted(&lists));
  database_->InsertChunks(safe_browsing_util::kCsdWhiteList, csd_chunks.get());
  database_->InsertChunks(safe_browsing_util::kDownloadWhiteList,
                          download_chunks.get());
  database_->UpdateFinished(true);

  EXPECT_FALSE(database_->IsMalwareIPMatchKillSwitchOn());
  EXPECT_FALSE(database_->IsCsdWhitelistKillSwitchOn());
  EXPECT_TRUE(database_->ContainsCsdWhitelistedUrl(
      GURL(std::string("https://") + kGood1Url2 + "/c.html")));
  EXPECT_TRUE(database_->ContainsCsdWhitelistedUrl(
      GURL(std::string("https://") + kGood2Url1 + "/c/bla")));
  EXPECT_FALSE(database_->ContainsCsdWhitelistedUrl(
      GURL(std::string("http://www.google.com/"))));
  EXPECT_FALSE(database_->ContainsCsdWhitelistedUrl(
      GURL(std::string("http://www.phishing_url.com/"))));

  EXPECT_TRUE(database_->ContainsDownloadWhitelistedUrl(
      GURL(std::string("https://") + kGood2Url1 + "/c/bla")));
  EXPECT_TRUE(database_->ContainsDownloadWhitelistedUrl(
      GURL(std::string("https://good3.com/"))));
  EXPECT_TRUE(database_->ContainsDownloadWhitelistedString(kGoodString));
  EXPECT_FALSE(database_->ContainsDownloadWhitelistedUrl(
      GURL(std::string("http://www.google.com/"))));
  EXPECT_FALSE(database_->ContainsDownloadWhitelistedUrl(
      GURL(std::string("http://www.phishing_url.com/"))));
  EXPECT_FALSE(database_->ContainsDownloadWhitelistedString("asdf"));

  database_.reset();
}

// Test to make sure we could insert chunk list that
// contains entries for the same host.
TEST_F(SafeBrowsingDatabaseTest, SameHostEntriesOkay) {
  ScopedVector<SBChunkData> chunks;

  // Add a malware add chunk with two entries of the same host.
  chunks.push_back(AddChunkPrefix2Value(1,
                                        "www.evil.com/malware1.html",
                                        "www.evil.com/malware2.html"));

  // Insert the testing chunks into database.
  std::vector<SBListChunkRanges> lists;
  ASSERT_TRUE(database_->UpdateStarted(&lists));
  database_->InsertChunks(safe_browsing_util::kMalwareList, chunks.get());
  database_->UpdateFinished(true);

  GetListsInfo(&lists);
  ASSERT_LE(1U, lists.size());
  EXPECT_EQ(safe_browsing_util::kMalwareList, lists[0].name);
  EXPECT_EQ("1", lists[0].adds);
  EXPECT_TRUE(lists[0].subs.empty());

  // Add a phishing add chunk with two entries of the same host.
  chunks.clear();
  chunks.push_back(AddChunkPrefix2Value(47,
                                        "www.evil.com/phishing1.html",
                                        "www.evil.com/phishing2.html"));

  ASSERT_TRUE(database_->UpdateStarted(&lists));
  database_->InsertChunks(safe_browsing_util::kPhishingList, chunks.get());
  database_->UpdateFinished(true);

  GetListsInfo(&lists);
  ASSERT_EQ(2U, lists.size());
  EXPECT_EQ(safe_browsing_util::kMalwareList, lists[0].name);
  EXPECT_EQ("1", lists[0].adds);
  EXPECT_TRUE(lists[0].subs.empty());
  EXPECT_EQ(safe_browsing_util::kPhishingList, lists[1].name);
  EXPECT_EQ("47", lists[1].adds);
  EXPECT_TRUE(lists[1].subs.empty());

  std::vector<SBPrefix> prefix_hits;
  std::vector<SBFullHashResult> cache_hits;

  EXPECT_TRUE(database_->ContainsBrowseUrl(
      GURL("http://www.evil.com/malware1.html"), &prefix_hits, &cache_hits));
  EXPECT_TRUE(database_->ContainsBrowseUrl(
      GURL("http://www.evil.com/malware2.html"), &prefix_hits, &cache_hits));
  EXPECT_TRUE(database_->ContainsBrowseUrl(
      GURL("http://www.evil.com/phishing1.html"), &prefix_hits, &cache_hits));
  EXPECT_TRUE(database_->ContainsBrowseUrl(
      GURL("http://www.evil.com/phishing2.html"), &prefix_hits, &cache_hits));

  // Test removing a single prefix from the add chunk.
  // Remove the prefix that added first.
  chunks.clear();
  chunks.push_back(SubChunkPrefixValue(4, "www.evil.com/malware1.html", 1));
  ASSERT_TRUE(database_->UpdateStarted(&lists));
  database_->InsertChunks(safe_browsing_util::kMalwareList, chunks.get());
  database_->UpdateFinished(true);

  // Remove the prefix that added last.
  chunks.clear();
  chunks.push_back(SubChunkPrefixValue(5, "www.evil.com/phishing2.html", 47));
  ASSERT_TRUE(database_->UpdateStarted(&lists));
  database_->InsertChunks(safe_browsing_util::kPhishingList, chunks.get());
  database_->UpdateFinished(true);

  // Verify that the database contains urls expected.
  EXPECT_FALSE(database_->ContainsBrowseUrl(
      GURL("http://www.evil.com/malware1.html"), &prefix_hits, &cache_hits));
  EXPECT_TRUE(database_->ContainsBrowseUrl(
      GURL("http://www.evil.com/malware2.html"), &prefix_hits, &cache_hits));
  EXPECT_TRUE(database_->ContainsBrowseUrl(
      GURL("http://www.evil.com/phishing1.html"), &prefix_hits, &cache_hits));
  EXPECT_FALSE(database_->ContainsBrowseUrl(
      GURL("http://www.evil.com/phishing2.html"), &prefix_hits, &cache_hits));
}

// Test that an empty update doesn't actually update the database.
// This isn't a functionality requirement, but it is a useful
// optimization.
TEST_F(SafeBrowsingDatabaseTest, EmptyUpdate) {
  ScopedVector<SBChunkData> chunks;

  base::FilePath filename = database_->BrowseDBFilename(database_filename_);

  // Prime the database.
  std::vector<SBListChunkRanges> lists;
  ASSERT_TRUE(database_->UpdateStarted(&lists));
  chunks.push_back(AddChunkPrefixValue(1, "www.evil.com/malware.html"));
  database_->InsertChunks(safe_browsing_util::kMalwareList, chunks.get());
  database_->UpdateFinished(true);

  // Get an older time to reset the lastmod time for detecting whether
  // the file has been updated.
  base::File::Info before_info, after_info;
  ASSERT_TRUE(base::GetFileInfo(filename, &before_info));
  const Time old_last_modified =
      before_info.last_modified - TimeDelta::FromSeconds(10);

  // Inserting another chunk updates the database file.  The sleep is
  // needed because otherwise the entire test can finish w/in the
  // resolution of the lastmod time.
  ASSERT_TRUE(base::TouchFile(filename, old_last_modified, old_last_modified));
  ASSERT_TRUE(base::GetFileInfo(filename, &before_info));
  ASSERT_TRUE(database_->UpdateStarted(&lists));
  chunks.push_back(AddChunkPrefixValue(2, "www.foo.com/malware.html"));
  database_->InsertChunks(safe_browsing_util::kMalwareList, chunks.get());
  database_->UpdateFinished(true);
  ASSERT_TRUE(base::GetFileInfo(filename, &after_info));
  EXPECT_LT(before_info.last_modified, after_info.last_modified);

  // Deleting a chunk updates the database file.
  ASSERT_TRUE(base::TouchFile(filename, old_last_modified, old_last_modified));
  ASSERT_TRUE(base::GetFileInfo(filename, &before_info));
  ASSERT_TRUE(database_->UpdateStarted(&lists));
  AddDelChunk(safe_browsing_util::kMalwareList, 2);
  database_->UpdateFinished(true);
  ASSERT_TRUE(base::GetFileInfo(filename, &after_info));
  EXPECT_LT(before_info.last_modified, after_info.last_modified);

  // Simply calling |UpdateStarted()| then |UpdateFinished()| does not
  // update the database file.
  ASSERT_TRUE(base::TouchFile(filename, old_last_modified, old_last_modified));
  ASSERT_TRUE(base::GetFileInfo(filename, &before_info));
  ASSERT_TRUE(database_->UpdateStarted(&lists));
  database_->UpdateFinished(true);
  ASSERT_TRUE(base::GetFileInfo(filename, &after_info));
  EXPECT_EQ(before_info.last_modified, after_info.last_modified);
}

// Test that a filter file is written out during update and read back
// in during setup.
TEST_F(SafeBrowsingDatabaseTest, FilterFile) {
  // Create a database with trivial example data and write it out.
  {
    // Prime the database.
    std::vector<SBListChunkRanges> lists;
    ASSERT_TRUE(database_->UpdateStarted(&lists));

    ScopedVector<SBChunkData> chunks;
    chunks.push_back(AddChunkPrefixValue(1, "www.evil.com/malware.html"));
    database_->InsertChunks(safe_browsing_util::kMalwareList, chunks.get());
    database_->UpdateFinished(true);
  }

  // Find the malware url in the database, don't find a good url.
  std::vector<SBPrefix> prefix_hits;
  std::vector<SBFullHashResult> cache_hits;
  EXPECT_TRUE(database_->ContainsBrowseUrl(
      GURL("http://www.evil.com/malware.html"), &prefix_hits, &cache_hits));
  EXPECT_FALSE(database_->ContainsBrowseUrl(
      GURL("http://www.good.com/goodware.html"), &prefix_hits, &cache_hits));

  base::FilePath filter_file = database_->PrefixSetForFilename(
      database_->BrowseDBFilename(database_filename_));

  // After re-creating the database, it should have a filter read from
  // a file, so it should find the same results.
  ASSERT_TRUE(base::PathExists(filter_file));
  database_.reset(new SafeBrowsingDatabaseNew);
  database_->Init(database_filename_);
  EXPECT_TRUE(database_->ContainsBrowseUrl(
      GURL("http://www.evil.com/malware.html"), &prefix_hits, &cache_hits));
  EXPECT_FALSE(database_->ContainsBrowseUrl(
      GURL("http://www.good.com/goodware.html"), &prefix_hits, &cache_hits));

  // If there is no filter file, the database cannot find malware urls.
  base::DeleteFile(filter_file, false);
  ASSERT_FALSE(base::PathExists(filter_file));
  database_.reset(new SafeBrowsingDatabaseNew);
  database_->Init(database_filename_);
  EXPECT_FALSE(database_->ContainsBrowseUrl(
      GURL("http://www.evil.com/malware.html"), &prefix_hits, &cache_hits));
  EXPECT_FALSE(database_->ContainsBrowseUrl(
      GURL("http://www.good.com/goodware.html"), &prefix_hits, &cache_hits));
}

TEST_F(SafeBrowsingDatabaseTest, CachedFullMiss) {
  const SBPrefix kPrefix1 = 1001U;
  const SBFullHash kFullHash1_1 =
      SBFullHashForPrefixAndSuffix(kPrefix1, "\x01");

  const SBPrefix kPrefix2 = 1002U;
  const SBFullHash kFullHash2_1 =
      SBFullHashForPrefixAndSuffix(kPrefix2, "\x01");

  // Insert prefix kPrefix1 and kPrefix2 into database.
  ScopedVector<SBChunkData> chunks;
  chunks.push_back(AddChunkPrefix(1, kPrefix1));
  chunks.push_back(AddChunkPrefix(2, kPrefix2));

  std::vector<SBListChunkRanges> lists;
  ASSERT_TRUE(database_->UpdateStarted(&lists));
  database_->InsertChunks(safe_browsing_util::kMalwareList, chunks.get());
  database_->UpdateFinished(true);

  {
    // Cache a full miss result for kPrefix1.
    std::vector<SBPrefix> prefixes(1, kPrefix1);
    std::vector<SBFullHashResult> cache_results;
    database_->CacheHashResults(prefixes, cache_results, kCacheLifetime);
  }

  {
    // kFullHash1_1 gets no prefix hit because of the cached item, and also does
    // not have a cache hit.
    std::vector<SBFullHash> full_hashes(1, kFullHash1_1);
    std::vector<SBPrefix> prefix_hits;
    std::vector<SBFullHashResult> cache_hits;
    EXPECT_FALSE(database_->ContainsBrowseUrlHashes(
        full_hashes, &prefix_hits, &cache_hits));

    // kFullHash2_1 gets a hit from the prefix in the database.
    full_hashes.push_back(kFullHash2_1);
    prefix_hits.clear();
    cache_hits.clear();
    EXPECT_TRUE(database_->ContainsBrowseUrlHashes(
        full_hashes, &prefix_hits, &cache_hits));
    ASSERT_EQ(1U, prefix_hits.size());
    EXPECT_EQ(kPrefix2, prefix_hits[0]);
    EXPECT_TRUE(cache_hits.empty());
  }
}

TEST_F(SafeBrowsingDatabaseTest, CachedPrefixHitFullMiss) {
  const SBPrefix kPrefix1 = 1001U;
  const SBFullHash kFullHash1_1 =
      SBFullHashForPrefixAndSuffix(kPrefix1, "\x01");
  const SBFullHash kFullHash1_2 =
      SBFullHashForPrefixAndSuffix(kPrefix1, "\x02");
  const SBFullHash kFullHash1_3 =
      SBFullHashForPrefixAndSuffix(kPrefix1, "\x03");

  const SBPrefix kPrefix2 = 1002U;
  const SBFullHash kFullHash2_1 =
      SBFullHashForPrefixAndSuffix(kPrefix2, "\x01");

  const SBPrefix kPrefix3 = 1003U;
  const SBFullHash kFullHash3_1 =
      SBFullHashForPrefixAndSuffix(kPrefix3, "\x01");

  // Insert prefix kPrefix1 and kPrefix2 into database.
  ScopedVector<SBChunkData> chunks;
  chunks.push_back(AddChunkPrefix(1, kPrefix1));
  chunks.push_back(AddChunkPrefix(2, kPrefix2));

  std::vector<SBListChunkRanges> lists;
  ASSERT_TRUE(database_->UpdateStarted(&lists));
  database_->InsertChunks(safe_browsing_util::kMalwareList, chunks.get());
  database_->UpdateFinished(true);

  {
    // kFullHash1_1 has a prefix hit of kPrefix1.
    std::vector<SBFullHash> full_hashes;
    full_hashes.push_back(kFullHash1_1);
    std::vector<SBPrefix> prefix_hits;
    std::vector<SBFullHashResult> cache_hits;
    EXPECT_TRUE(database_->ContainsBrowseUrlHashes(
        full_hashes, &prefix_hits, &cache_hits));
    ASSERT_EQ(1U, prefix_hits.size());
    EXPECT_EQ(kPrefix1, prefix_hits[0]);
    EXPECT_TRUE(cache_hits.empty());

    // kFullHash2_1 has a prefix hit of kPrefix2.
    full_hashes.push_back(kFullHash2_1);
    prefix_hits.clear();
    cache_hits.clear();
    EXPECT_TRUE(database_->ContainsBrowseUrlHashes(
        full_hashes, &prefix_hits, &cache_hits));
    ASSERT_EQ(2U, prefix_hits.size());
    EXPECT_EQ(kPrefix1, prefix_hits[0]);
    EXPECT_EQ(kPrefix2, prefix_hits[1]);
    EXPECT_TRUE(cache_hits.empty());

    // kFullHash3_1 has no hits.
    full_hashes.push_back(kFullHash3_1);
    prefix_hits.clear();
    cache_hits.clear();
    EXPECT_TRUE(database_->ContainsBrowseUrlHashes(
        full_hashes, &prefix_hits, &cache_hits));
    ASSERT_EQ(2U, prefix_hits.size());
    EXPECT_EQ(kPrefix1, prefix_hits[0]);
    EXPECT_EQ(kPrefix2, prefix_hits[1]);
    EXPECT_TRUE(cache_hits.empty());
  }

  {
    // Cache a fullhash result for two kPrefix1 full hashes.
    std::vector<SBPrefix> prefixes(1, kPrefix1);
    std::vector<SBFullHashResult> cache_results;

    SBFullHashResult full_hash_result;
    full_hash_result.list_id = safe_browsing_util::MALWARE;

    full_hash_result.hash = kFullHash1_1;
    cache_results.push_back(full_hash_result);

    full_hash_result.hash = kFullHash1_3;
    cache_results.push_back(full_hash_result);

    database_->CacheHashResults(prefixes, cache_results, kCacheLifetime);
  }

  {
    // kFullHash1_1 should now see a cache hit.
    std::vector<SBFullHash> full_hashes(1, kFullHash1_1);
    std::vector<SBPrefix> prefix_hits;
    std::vector<SBFullHashResult> cache_hits;
    EXPECT_TRUE(database_->ContainsBrowseUrlHashes(
        full_hashes, &prefix_hits, &cache_hits));
    EXPECT_TRUE(prefix_hits.empty());
    ASSERT_EQ(1U, cache_hits.size());
    EXPECT_TRUE(SBFullHashEqual(kFullHash1_1, cache_hits[0].hash));

    // Adding kFullHash2_1 will see the existing cache hit plus the prefix hit
    // for kPrefix2.
    full_hashes.push_back(kFullHash2_1);
    prefix_hits.clear();
    cache_hits.clear();
    EXPECT_TRUE(database_->ContainsBrowseUrlHashes(
        full_hashes, &prefix_hits, &cache_hits));
    ASSERT_EQ(1U, prefix_hits.size());
    EXPECT_EQ(kPrefix2, prefix_hits[0]);
    ASSERT_EQ(1U, cache_hits.size());
    EXPECT_TRUE(SBFullHashEqual(kFullHash1_1, cache_hits[0].hash));

    // kFullHash1_3 also gets a cache hit.
    full_hashes.push_back(kFullHash1_3);
    prefix_hits.clear();
    cache_hits.clear();
    EXPECT_TRUE(database_->ContainsBrowseUrlHashes(
        full_hashes, &prefix_hits, &cache_hits));
    ASSERT_EQ(1U, prefix_hits.size());
    EXPECT_EQ(kPrefix2, prefix_hits[0]);
    ASSERT_EQ(2U, cache_hits.size());
    EXPECT_TRUE(SBFullHashEqual(kFullHash1_1, cache_hits[0].hash));
    EXPECT_TRUE(SBFullHashEqual(kFullHash1_3, cache_hits[1].hash));
  }

  {
    // Check if DB contains only kFullHash1_3. Should return a cache hit.
    std::vector<SBFullHash> full_hashes(1, kFullHash1_3);
    std::vector<SBPrefix> prefix_hits;
    std::vector<SBFullHashResult> cache_hits;
    EXPECT_TRUE(database_->ContainsBrowseUrlHashes(
        full_hashes, &prefix_hits, &cache_hits));
    EXPECT_TRUE(prefix_hits.empty());
    ASSERT_EQ(1U, cache_hits.size());
    EXPECT_TRUE(SBFullHashEqual(kFullHash1_3, cache_hits[0].hash));
  }

  {
    // kFullHash1_2 has no cache hit, and no prefix hit because of the cache for
    // kPrefix1.
    std::vector<SBFullHash> full_hashes(1, kFullHash1_2);
    std::vector<SBPrefix> prefix_hits;
    std::vector<SBFullHashResult> cache_hits;
    EXPECT_FALSE(database_->ContainsBrowseUrlHashes(
        full_hashes, &prefix_hits, &cache_hits));

    // Other prefix hits possible when kFullHash1_2 hits nothing.
    full_hashes.push_back(kFullHash2_1);
    prefix_hits.clear();
    cache_hits.clear();
    EXPECT_TRUE(database_->ContainsBrowseUrlHashes(
        full_hashes, &prefix_hits, &cache_hits));
    ASSERT_EQ(1U, prefix_hits.size());
    EXPECT_EQ(kPrefix2, prefix_hits[0]);
    EXPECT_TRUE(cache_hits.empty());
  }
}

TEST_F(SafeBrowsingDatabaseTest, BrowseFullHashMatching) {
  const SBPrefix kPrefix1 = 1001U;
  const SBFullHash kFullHash1_1 =
      SBFullHashForPrefixAndSuffix(kPrefix1, "\x01");
  const SBFullHash kFullHash1_2 =
      SBFullHashForPrefixAndSuffix(kPrefix1, "\x02");
  const SBFullHash kFullHash1_3 =
      SBFullHashForPrefixAndSuffix(kPrefix1, "\x03");

  // Insert two full hashes with a shared prefix.
  ScopedVector<SBChunkData> chunks;
  chunks.push_back(AddChunkFullHash(1, kFullHash1_1));
  chunks.push_back(AddChunkFullHash(2, kFullHash1_2));

  std::vector<SBListChunkRanges> lists;
  ASSERT_TRUE(database_->UpdateStarted(&lists));
  database_->InsertChunks(safe_browsing_util::kMalwareList, chunks.get());
  database_->UpdateFinished(true);

  {
    // Check a full hash which isn't present.
    std::vector<SBFullHash> full_hashes(1, kFullHash1_3);
    std::vector<SBPrefix> prefix_hits;
    std::vector<SBFullHashResult> cache_hits;
    EXPECT_FALSE(database_->ContainsBrowseUrlHashes(
        full_hashes, &prefix_hits, &cache_hits));

    // Also one which is present, should have a prefix hit.
    full_hashes.push_back(kFullHash1_1);
    prefix_hits.clear();
    cache_hits.clear();
    EXPECT_TRUE(database_->ContainsBrowseUrlHashes(
        full_hashes, &prefix_hits, &cache_hits));
    ASSERT_EQ(1U, prefix_hits.size());
    EXPECT_EQ(kPrefix1, prefix_hits[0]);
    EXPECT_TRUE(cache_hits.empty());

    // Two full hash matches with the same prefix should return one prefix hit.
    full_hashes.push_back(kFullHash1_2);
    prefix_hits.clear();
    cache_hits.clear();
    EXPECT_TRUE(database_->ContainsBrowseUrlHashes(
        full_hashes, &prefix_hits, &cache_hits));
    ASSERT_EQ(1U, prefix_hits.size());
    EXPECT_EQ(kPrefix1, prefix_hits[0]);
    EXPECT_TRUE(cache_hits.empty());
  }

  {
    // Cache a gethash result for kFullHash1_2.
    SBFullHashResult full_hash_result;
    full_hash_result.list_id = safe_browsing_util::MALWARE;
    full_hash_result.hash = kFullHash1_2;

    std::vector<SBPrefix> prefixes(1, kPrefix1);
    std::vector<SBFullHashResult> cache_results(1, full_hash_result);

    database_->CacheHashResults(prefixes, cache_results, kCacheLifetime);
  }

  {
    // kFullHash1_3 should still return false, because the cached
    // result for kPrefix1 doesn't contain kFullHash1_3.
    std::vector<SBFullHash> full_hashes(1, kFullHash1_3);
    std::vector<SBPrefix> prefix_hits;
    std::vector<SBFullHashResult> cache_hits;
    EXPECT_FALSE(database_->ContainsBrowseUrlHashes(
        full_hashes, &prefix_hits, &cache_hits));

    // kFullHash1_1 is also not in the cached result, which takes
    // priority over the database.
    prefix_hits.clear();
    full_hashes.push_back(kFullHash1_1);
    cache_hits.clear();
    EXPECT_FALSE(database_->ContainsBrowseUrlHashes(
        full_hashes, &prefix_hits, &cache_hits));

    // kFullHash1_2 is in the cached result.
    full_hashes.push_back(kFullHash1_2);
    prefix_hits.clear();
    cache_hits.clear();
    EXPECT_TRUE(database_->ContainsBrowseUrlHashes(
        full_hashes, &prefix_hits, &cache_hits));
    EXPECT_TRUE(prefix_hits.empty());
    ASSERT_EQ(1U, cache_hits.size());
    EXPECT_TRUE(SBFullHashEqual(kFullHash1_2, cache_hits[0].hash));
  }

  // Remove kFullHash1_1 from the database.
  chunks.clear();
  chunks.push_back(SubChunkFullHash(11, kFullHash1_1, 1));

  ASSERT_TRUE(database_->UpdateStarted(&lists));
  database_->InsertChunks(safe_browsing_util::kMalwareList, chunks.get());
  database_->UpdateFinished(true);

  // Cache should be cleared after updating.
  EXPECT_TRUE(database_->browse_gethash_cache_.empty());

  {
    // Now the database doesn't contain kFullHash1_1.
    std::vector<SBFullHash> full_hashes(1, kFullHash1_1);
    std::vector<SBPrefix> prefix_hits;
    std::vector<SBFullHashResult> cache_hits;
    EXPECT_FALSE(database_->ContainsBrowseUrlHashes(
        full_hashes, &prefix_hits, &cache_hits));

    // Nor kFullHash1_3.
    full_hashes.push_back(kFullHash1_3);
    prefix_hits.clear();
    cache_hits.clear();
    EXPECT_FALSE(database_->ContainsBrowseUrlHashes(
        full_hashes, &prefix_hits, &cache_hits));

    // Still has kFullHash1_2.
    full_hashes.push_back(kFullHash1_2);
    prefix_hits.clear();
    cache_hits.clear();
    EXPECT_TRUE(database_->ContainsBrowseUrlHashes(
        full_hashes, &prefix_hits, &cache_hits));
    ASSERT_EQ(1U, prefix_hits.size());
    EXPECT_EQ(kPrefix1, prefix_hits[0]);
    EXPECT_TRUE(cache_hits.empty());
  }

  // Remove kFullHash1_2 from the database.
  chunks.clear();
  chunks.push_back(SubChunkFullHash(12, kFullHash1_2, 2));

  ASSERT_TRUE(database_->UpdateStarted(&lists));
  database_->InsertChunks(safe_browsing_util::kMalwareList, chunks.get());
  database_->UpdateFinished(true);

  // Cache should be cleared after updating.
  EXPECT_TRUE(database_->browse_gethash_cache_.empty());

  {
    // None are present.
    std::vector<SBFullHash> full_hashes;
    std::vector<SBPrefix> prefix_hits;
    std::vector<SBFullHashResult> cache_hits;
    full_hashes.push_back(kFullHash1_1);
    full_hashes.push_back(kFullHash1_2);
    full_hashes.push_back(kFullHash1_3);
    EXPECT_FALSE(database_->ContainsBrowseUrlHashes(
        full_hashes, &prefix_hits, &cache_hits));
  }
}

TEST_F(SafeBrowsingDatabaseTest, BrowseFullHashAndPrefixMatching) {
  const SBPrefix kPrefix1 = 1001U;
  const SBFullHash kFullHash1_1 =
      SBFullHashForPrefixAndSuffix(kPrefix1, "\x01");
  const SBFullHash kFullHash1_2 =
      SBFullHashForPrefixAndSuffix(kPrefix1, "\x02");

  ScopedVector<SBChunkData> chunks;
  chunks.push_back(AddChunkFullHash(1, kFullHash1_1));

  std::vector<SBListChunkRanges> lists;
  ASSERT_TRUE(database_->UpdateStarted(&lists));
  database_->InsertChunks(safe_browsing_util::kMalwareList, chunks.get());
  database_->UpdateFinished(true);

  {
    // kFullHash1_2 does not match kFullHash1_1.
    std::vector<SBFullHash> full_hashes(1, kFullHash1_2);
    std::vector<SBPrefix> prefix_hits;
    std::vector<SBFullHashResult> cache_hits;
    EXPECT_FALSE(database_->ContainsBrowseUrlHashes(
        full_hashes, &prefix_hits, &cache_hits));
  }

  // Add a prefix match.
  chunks.clear();
  chunks.push_back(AddChunkPrefix(2, kPrefix1));

  ASSERT_TRUE(database_->UpdateStarted(&lists));
  database_->InsertChunks(safe_browsing_util::kMalwareList, chunks.get());
  database_->UpdateFinished(true);

  {
    // kFullHash1_2 does match kPrefix1.
    std::vector<SBFullHash> full_hashes(1, kFullHash1_2);
    std::vector<SBPrefix> prefix_hits;
    std::vector<SBFullHashResult> cache_hits;
    EXPECT_TRUE(database_->ContainsBrowseUrlHashes(
        full_hashes, &prefix_hits, &cache_hits));
    ASSERT_EQ(1U, prefix_hits.size());
    EXPECT_EQ(kPrefix1, prefix_hits[0]);
    EXPECT_TRUE(cache_hits.empty());
  }

  // Remove the full hash.
  chunks.clear();
  chunks.push_back(SubChunkFullHash(11, kFullHash1_1, 1));

  ASSERT_TRUE(database_->UpdateStarted(&lists));
  database_->InsertChunks(safe_browsing_util::kMalwareList, chunks.get());
  database_->UpdateFinished(true);

  {
    // kFullHash1_2 still returns true due to the prefix hit.
    std::vector<SBFullHash> full_hashes(1, kFullHash1_2);
    std::vector<SBPrefix> prefix_hits;
    std::vector<SBFullHashResult> cache_hits;
    EXPECT_TRUE(database_->ContainsBrowseUrlHashes(
        full_hashes, &prefix_hits, &cache_hits));
    ASSERT_EQ(1U, prefix_hits.size());
    EXPECT_EQ(kPrefix1, prefix_hits[0]);
    EXPECT_TRUE(cache_hits.empty());
  }
}

TEST_F(SafeBrowsingDatabaseTest, MalwareIpBlacklist) {
  database_.reset();
  SafeBrowsingStoreFile* browse_store = new SafeBrowsingStoreFile();
  SafeBrowsingStoreFile* ip_blacklist_store = new SafeBrowsingStoreFile();
  database_.reset(new SafeBrowsingDatabaseNew(browse_store,
                                              NULL,
                                              NULL,
                                              NULL,
                                              NULL,
                                              NULL,
                                              ip_blacklist_store));
  database_->Init(database_filename_);
  std::vector<SBListChunkRanges> lists;
  ASSERT_TRUE(database_->UpdateStarted(&lists));

  ScopedVector<SBChunkData> chunks;

  // IPv4 prefix match for ::ffff:192.168.1.0/120.
  chunks.push_back(AddChunkHashedIpValue(1, "::ffff:192.168.1.0", 120));

  // IPv4 exact match for ::ffff:192.1.1.1.
  chunks.push_back(AddChunkHashedIpValue(2, "::ffff:192.1.1.1", 128));

  // IPv6 exact match for: fe80::31a:a0ff:fe10:786e/128.
  chunks.push_back(AddChunkHashedIpValue(3, "fe80::31a:a0ff:fe10:786e", 128));

  // IPv6 prefix match for: 2620:0:1000:3103::/64.
  chunks.push_back(AddChunkHashedIpValue(4, "2620:0:1000:3103::", 64));

  // IPv4 prefix match for ::ffff:192.1.122.0/119.
  chunks.push_back(AddChunkHashedIpValue(5, "::ffff:192.1.122.0", 119));

  // IPv4 prefix match for ::ffff:192.1.128.0/113.
  chunks.push_back(AddChunkHashedIpValue(6, "::ffff:192.1.128.0", 113));

  database_->InsertChunks(safe_browsing_util::kIPBlacklist, chunks.get());
  database_->UpdateFinished(true);

  EXPECT_FALSE(database_->ContainsMalwareIP("192.168.0.255"));
  EXPECT_TRUE(database_->ContainsMalwareIP("192.168.1.0"));
  EXPECT_TRUE(database_->ContainsMalwareIP("192.168.1.255"));
  EXPECT_TRUE(database_->ContainsMalwareIP("192.168.1.10"));
  EXPECT_TRUE(database_->ContainsMalwareIP("::ffff:192.168.1.2"));
  EXPECT_FALSE(database_->ContainsMalwareIP("192.168.2.0"));

  EXPECT_FALSE(database_->ContainsMalwareIP("192.1.1.0"));
  EXPECT_TRUE(database_->ContainsMalwareIP("192.1.1.1"));
  EXPECT_FALSE(database_->ContainsMalwareIP("192.1.1.2"));

  EXPECT_FALSE(database_->ContainsMalwareIP(
      "2620:0:1000:3102:ffff:ffff:ffff:ffff"));
  EXPECT_TRUE(database_->ContainsMalwareIP("2620:0:1000:3103::"));
  EXPECT_TRUE(database_->ContainsMalwareIP(
      "2620:0:1000:3103:ffff:ffff:ffff:ffff"));
  EXPECT_FALSE(database_->ContainsMalwareIP("2620:0:1000:3104::"));

  EXPECT_FALSE(database_->ContainsMalwareIP("fe80::21a:a0ff:fe10:786d"));
  EXPECT_TRUE(database_->ContainsMalwareIP("fe80::31a:a0ff:fe10:786e"));
  EXPECT_FALSE(database_->ContainsMalwareIP("fe80::21a:a0ff:fe10:786f"));

  EXPECT_FALSE(database_->ContainsMalwareIP("192.1.121.255"));
  EXPECT_TRUE(database_->ContainsMalwareIP("192.1.122.0"));
  EXPECT_TRUE(database_->ContainsMalwareIP("::ffff:192.1.122.1"));
  EXPECT_TRUE(database_->ContainsMalwareIP("192.1.122.255"));
  EXPECT_TRUE(database_->ContainsMalwareIP("192.1.123.0"));
  EXPECT_TRUE(database_->ContainsMalwareIP("192.1.123.255"));
  EXPECT_FALSE(database_->ContainsMalwareIP("192.1.124.0"));

  EXPECT_FALSE(database_->ContainsMalwareIP("192.1.127.255"));
  EXPECT_TRUE(database_->ContainsMalwareIP("192.1.128.0"));
  EXPECT_TRUE(database_->ContainsMalwareIP("::ffff:192.1.128.1"));
  EXPECT_TRUE(database_->ContainsMalwareIP("192.1.128.255"));
  EXPECT_TRUE(database_->ContainsMalwareIP("192.1.255.0"));
  EXPECT_TRUE(database_->ContainsMalwareIP("192.1.255.255"));
  EXPECT_FALSE(database_->ContainsMalwareIP("192.2.0.0"));
}

TEST_F(SafeBrowsingDatabaseTest, ContainsBrowseURL) {
  std::vector<SBListChunkRanges> lists;
  ASSERT_TRUE(database_->UpdateStarted(&lists));

  // Add a host-level hit.
  {
    ScopedVector<SBChunkData> chunks;
    chunks.push_back(AddChunkPrefixValue(1, "www.evil.com/"));
    database_->InsertChunks(safe_browsing_util::kMalwareList, chunks.get());
  }

  // Add a specific fullhash.
  static const char kWhateverMalware[] = "www.whatever.com/malware.html";
  {
    ScopedVector<SBChunkData> chunks;
    chunks.push_back(AddChunkFullHashValue(2, kWhateverMalware));
    database_->InsertChunks(safe_browsing_util::kMalwareList, chunks.get());
  }

  // Add a fullhash which has a prefix collision for a known url.
  static const char kExampleFine[] = "www.example.com/fine.html";
  static const char kExampleCollision[] =
      "www.example.com/3123364814/malware.htm";
  ASSERT_EQ(SBPrefixForString(kExampleFine),
            SBPrefixForString(kExampleCollision));
  {
    ScopedVector<SBChunkData> chunks;
    chunks.push_back(AddChunkFullHashValue(3, kExampleCollision));
    database_->InsertChunks(safe_browsing_util::kMalwareList, chunks.get());
  }

  database_->UpdateFinished(true);

  std::vector<SBPrefix> prefix_hits;
  std::vector<SBFullHashResult> cache_hits;

  // Anything will hit the host prefix.
  EXPECT_TRUE(database_->ContainsBrowseUrl(
      GURL("http://www.evil.com/malware.html"), &prefix_hits, &cache_hits));
  ASSERT_EQ(1U, prefix_hits.size());
  EXPECT_EQ(SBPrefixForString("www.evil.com/"), prefix_hits[0]);
  EXPECT_TRUE(cache_hits.empty());

  // Hit the specific URL prefix.
  EXPECT_TRUE(database_->ContainsBrowseUrl(
      GURL(std::string("http://") + kWhateverMalware),
      &prefix_hits, &cache_hits));
  ASSERT_EQ(1U, prefix_hits.size());
  EXPECT_EQ(SBPrefixForString(kWhateverMalware), prefix_hits[0]);
  EXPECT_TRUE(cache_hits.empty());

  // Other URLs at that host are fine.
  EXPECT_FALSE(database_->ContainsBrowseUrl(
      GURL("http://www.whatever.com/fine.html"), &prefix_hits, &cache_hits));
  EXPECT_TRUE(prefix_hits.empty());
  EXPECT_TRUE(cache_hits.empty());

  // Hit the specific URL full hash.
  EXPECT_TRUE(database_->ContainsBrowseUrl(
      GURL(std::string("http://") + kExampleCollision),
      &prefix_hits, &cache_hits));
  ASSERT_EQ(1U, prefix_hits.size());
  EXPECT_EQ(SBPrefixForString(kExampleCollision), prefix_hits[0]);
  EXPECT_TRUE(cache_hits.empty());

  // This prefix collides, but no full hash match.
  EXPECT_FALSE(database_->ContainsBrowseUrl(
      GURL(std::string("http://") + kExampleFine), &prefix_hits, &cache_hits));
}
