// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/favicon_cache.h"

#include "base/stringprintf.h"
#include "sync/api/sync_error_factory_mock.h"
#include "sync/protocol/favicon_image_specifics.pb.h"
#include "sync/protocol/favicon_tracking_specifics.pb.h"
#include "sync/protocol/sync.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {

namespace {

// Total number of favicons to use in sync tests.
const int kTotalFavicons = 10;

// TestChangeProcessor --------------------------------------------------------

// Dummy SyncChangeProcessor used to help review what SyncChanges are pushed
// back up to Sync.
class TestChangeProcessor : public syncer::SyncChangeProcessor {
 public:
  TestChangeProcessor();
  virtual ~TestChangeProcessor();

  // Store a copy of all the changes passed in so we can examine them later.
  virtual syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) OVERRIDE;

  bool contains_guid(const std::string& guid) const {
    return change_map_.count(guid) != 0;
  }

  syncer::SyncChange change_for_guid(const std::string& guid) const {
    DCHECK(contains_guid(guid));
    return change_map_.find(guid)->second;
  }

  // Returns the last change list received, and resets the internal list.
  syncer::SyncChangeList GetAndResetChangeList() {
    syncer::SyncChangeList list;
    list.swap(change_list_);
    return list;
  }

  void set_erroneous(bool erroneous) { erroneous_ = erroneous; }

 private:
  // Track the changes received in ProcessSyncChanges.
  std::map<std::string, syncer::SyncChange> change_map_;
  syncer::SyncChangeList change_list_;
  bool erroneous_;

  DISALLOW_COPY_AND_ASSIGN(TestChangeProcessor);
};

TestChangeProcessor::TestChangeProcessor() : erroneous_(false) {
}

TestChangeProcessor::~TestChangeProcessor() {
}

syncer::SyncError TestChangeProcessor::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  if (erroneous_) {
    return syncer::SyncError(
        FROM_HERE, "Some error.", change_list[0].sync_data().GetDataType());
  }

  change_list_ = change_list;
  change_map_.erase(change_map_.begin(), change_map_.end());
  for (syncer::SyncChangeList::const_iterator iter = change_list.begin();
      iter != change_list.end(); ++iter) {
    change_map_[iter->sync_data().GetTitle()] = *iter;
  }
  return syncer::SyncError();
}


// SyncChangeProcessorDelegate ------------------------------------------------

class SyncChangeProcessorDelegate : public syncer::SyncChangeProcessor {
 public:
  explicit SyncChangeProcessorDelegate(syncer::SyncChangeProcessor* recipient);
  virtual ~SyncChangeProcessorDelegate();

  // syncer::SyncChangeProcessor implementation.
  virtual syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) OVERRIDE;

 private:
  // The recipient of all sync changes.
  syncer::SyncChangeProcessor* recipient_;

  DISALLOW_COPY_AND_ASSIGN(SyncChangeProcessorDelegate);
};

SyncChangeProcessorDelegate::SyncChangeProcessorDelegate(
    syncer::SyncChangeProcessor* recipient)
    : recipient_(recipient) {
  DCHECK(recipient_);
}

SyncChangeProcessorDelegate::~SyncChangeProcessorDelegate() {
}

syncer::SyncError SyncChangeProcessorDelegate::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  return recipient_->ProcessSyncChanges(from_here, change_list);
}

// TestFaviconData ------------------------------------------------------------
struct TestFaviconData {
  TestFaviconData() : last_visit_time(0), is_bookmarked(false) {}
  GURL page_url;
  GURL icon_url;
  std::string image_16;
  std::string image_32;
  std::string image_64;
  int last_visit_time;
  bool is_bookmarked;
};

TestFaviconData BuildFaviconData(int index) {
  TestFaviconData data;
  data.page_url = GURL(base::StringPrintf("http://bla.com/%i.html", index));
  data.icon_url = GURL(base::StringPrintf("http://bla.com/%i.ico", index));
  data.image_16 = base::StringPrintf("16 %i", index);
  // TODO(zea): enable this once the cache supports writing them.
  // data.image_32 = base::StringPrintf("32 %i", index);
  // data.image_64 = base::StringPrintf("64 %i", index);
  data.last_visit_time = index;
  return data;
}

void FillImageSpecifics(
    const TestFaviconData& test_data,
    sync_pb::FaviconImageSpecifics* image_specifics) {
  image_specifics->set_favicon_url(test_data.icon_url.spec());
  if (!test_data.image_16.empty()) {
    image_specifics->mutable_favicon_web()->set_height(16);
    image_specifics->mutable_favicon_web()->set_width(16);
    image_specifics->mutable_favicon_web()->set_favicon(test_data.image_16);
  }
  if (!test_data.image_32.empty()) {
    image_specifics->mutable_favicon_web_32()->set_height(32);
    image_specifics->mutable_favicon_web_32()->set_width(32);
    image_specifics->mutable_favicon_web_32()->set_favicon(test_data.image_32);
  }
  if (!test_data.image_64.empty()) {
    image_specifics->mutable_favicon_touch_64()->set_height(64);
    image_specifics->mutable_favicon_touch_64()->set_width(64);
    image_specifics->mutable_favicon_touch_64()->
        set_favicon(test_data.image_64);
  }
}

void FillTrackingSpecifics(
    const TestFaviconData& test_data,
    sync_pb::FaviconTrackingSpecifics* tracking_specifics) {
  tracking_specifics->set_favicon_url(test_data.icon_url.spec());
  tracking_specifics->set_last_visit_time_ms(test_data.last_visit_time);
  tracking_specifics->set_is_bookmarked(test_data.is_bookmarked);
}

testing::AssertionResult CompareFaviconDataToSpecifics(
    const TestFaviconData& test_data,
    const sync_pb::EntitySpecifics& specifics) {
  if (specifics.has_favicon_image()) {
    sync_pb::FaviconImageSpecifics image_specifics = specifics.favicon_image();
    if (image_specifics.favicon_url() != test_data.icon_url.spec())
      return testing::AssertionFailure() << "Image icon url doesn't match.";
    if (!test_data.image_16.empty()) {
      if (image_specifics.favicon_web().favicon() != test_data.image_16 ||
          image_specifics.favicon_web().height() != 16 ||
          image_specifics.favicon_web().width() != 16) {
        return testing::AssertionFailure() << "16p image data doesn't match.";
      }
    } else if (image_specifics.has_favicon_web()) {
      return testing::AssertionFailure() << "Missing 16p favicon.";
    }
    if (!test_data.image_32.empty()) {
      if (image_specifics.favicon_web_32().favicon() != test_data.image_32 ||
          image_specifics.favicon_web().height() != 32 ||
          image_specifics.favicon_web().width() != 32) {
        return testing::AssertionFailure() << "32p image data doesn't match.";
      }
    } else if (image_specifics.has_favicon_web_32()) {
      return testing::AssertionFailure() << "Missing 32p favicon.";
    }
    if (!test_data.image_64.empty()) {
      if (image_specifics.favicon_touch_64().favicon() != test_data.image_64 ||
          image_specifics.favicon_web().height() != 64 ||
          image_specifics.favicon_web().width() != 64) {
        return testing::AssertionFailure() << "64p image data doesn't match.";
      }
    } else if (image_specifics.has_favicon_touch_64()) {
      return testing::AssertionFailure() << "Missing 64p favicon.";
    }
  } else {
    sync_pb::FaviconTrackingSpecifics tracking_specifics =
        specifics.favicon_tracking();
    if (tracking_specifics.favicon_url() != test_data.icon_url.spec())
      return testing::AssertionFailure() << "Tracking icon url doesn't match.";
    if (tracking_specifics.last_visit_time_ms() != test_data.last_visit_time)
      return testing::AssertionFailure() << "Visit time doesn't match.";
    if (tracking_specifics.is_bookmarked() != test_data.is_bookmarked)
      return testing::AssertionFailure() << "Bookmark status doens't match.";
  }
  return testing::AssertionSuccess();
}

testing::AssertionResult VerifyChanges(
    syncer::ModelType expected_model_type,
    const std::vector<syncer::SyncChange::SyncChangeType>&
        expected_change_types,
    const std::vector<int>& expected_icons,
    const syncer::SyncChangeList& change_list) {
  DCHECK_EQ(expected_change_types.size(), expected_icons.size());
  if (change_list.size() != expected_icons.size())
    return testing::AssertionFailure() << "Change list size doesn't match.";
  for (size_t i = 0; i < expected_icons.size(); ++i) {
    TestFaviconData data = BuildFaviconData(expected_icons[i]);
    if (change_list[i].sync_data().GetDataType() != expected_model_type)
      return testing::AssertionFailure() << "Change datatype doesn't match.";
    if (change_list[i].change_type() != expected_change_types[i])
      return testing::AssertionFailure() << "Change type doesn't match.";
    testing::AssertionResult compare_result =
        CompareFaviconDataToSpecifics(
            BuildFaviconData(expected_icons[i]),
            change_list[i].sync_data().GetSpecifics());
    if (compare_result.failure_message())
      return compare_result;
  }
  return testing::AssertionSuccess();
}

}  // namespace

class SyncFaviconCacheTest : public testing::Test {
 public:
  SyncFaviconCacheTest();
  virtual ~SyncFaviconCacheTest() {}

  void SetUpInitialSync(const syncer::SyncDataList& initial_image_data,
                        const syncer::SyncDataList& initial_tracking_data);

  size_t GetFaviconCount() const;
  testing::AssertionResult ExpectFaviconEquals(
        const std::string& page_url,
        const std::string& bytes) const;
  testing::AssertionResult VerifyLocalIcons(
      const std::vector<int>& expected_icons);
  testing::AssertionResult VerifyLocalCustomIcons(
      const std::vector<TestFaviconData>& expected_icons);

  scoped_ptr<syncer::SyncChangeProcessor> CreateAndPassProcessor();
  scoped_ptr<syncer::SyncErrorFactory> CreateAndPassSyncErrorFactory();

  FaviconCache* cache() { return &cache_; }
  TestChangeProcessor* processor() { return sync_processor_.get(); }

 private:
  FaviconCache cache_;

  // Our dummy ChangeProcessor used to inspect changes pushed to Sync.
  scoped_ptr<TestChangeProcessor> sync_processor_;
  scoped_ptr<SyncChangeProcessorDelegate> sync_processor_delegate_;
};

SyncFaviconCacheTest::SyncFaviconCacheTest()
    : cache_(NULL),
      sync_processor_(new TestChangeProcessor),
      sync_processor_delegate_(new SyncChangeProcessorDelegate(
                                   sync_processor_.get())) {
}

void SyncFaviconCacheTest::SetUpInitialSync(
    const syncer::SyncDataList& initial_image_data,
    const syncer::SyncDataList& initial_tracking_data) {
  cache()->MergeDataAndStartSyncing(syncer::FAVICON_IMAGES,
                                    initial_image_data,
                                    CreateAndPassProcessor(),
                                    CreateAndPassSyncErrorFactory());
  ASSERT_EQ(0U, processor()->GetAndResetChangeList().size());
  cache()->MergeDataAndStartSyncing(syncer::FAVICON_TRACKING,
                                    initial_tracking_data,
                                    CreateAndPassProcessor(),
                                    CreateAndPassSyncErrorFactory());
  ASSERT_EQ(0U, processor()->GetAndResetChangeList().size());
}

size_t SyncFaviconCacheTest::GetFaviconCount() const {
  return cache_.NumFaviconsForTest();
}

testing::AssertionResult SyncFaviconCacheTest::ExpectFaviconEquals(
    const std::string& page_url,
    const std::string& bytes) const {
  GURL gurl(page_url);
  scoped_refptr<base::RefCountedMemory> favicon;
  if (!cache_.GetSyncedFaviconForPageURL(gurl, &favicon))
    return testing::AssertionFailure() << "Favicon is missing.";
  if (favicon->size() != bytes.size())
    return testing::AssertionFailure() << "Favicon sizes don't match.";
  for (size_t i = 0; i < favicon->size(); ++i) {
    if (bytes[i] != *(favicon->front() + i))
      return testing::AssertionFailure() << "Favicon data doesn't match.";
  }
  return testing::AssertionSuccess();
}

testing::AssertionResult SyncFaviconCacheTest::VerifyLocalIcons(
    const std::vector<int>& expected_icons) {
  std::vector<TestFaviconData> expected_custom_icons;
  for (size_t i = 0; i < expected_icons.size(); ++i) {
    expected_custom_icons.push_back(BuildFaviconData(expected_icons[i]));
  }
  return VerifyLocalCustomIcons(expected_custom_icons);
}


testing::AssertionResult SyncFaviconCacheTest::VerifyLocalCustomIcons(
    const std::vector<TestFaviconData>& expected_custom_icons) {
  syncer::SyncDataList image_data_list =
      cache()->GetAllSyncData(syncer::FAVICON_IMAGES);
  syncer::SyncDataList tracking_data_list =
      cache()->GetAllSyncData(syncer::FAVICON_TRACKING);
  if (expected_custom_icons.size() > image_data_list.size() ||
      expected_custom_icons.size() > tracking_data_list.size())
    return testing::AssertionFailure() << "Number of icons doesn't match.";
  for (size_t i = 0; i < expected_custom_icons.size(); ++i) {
    const TestFaviconData& test_data = expected_custom_icons[i];
    // Find the test data in the data lists. Assume that both lists have the
    // same ordering, which may not match the |expected_custom_icons| ordering.
    bool found_match = false;
    for (size_t j = 0; j < image_data_list.size(); ++j) {
      if (image_data_list[j].GetTitle() != test_data.icon_url.spec())
        continue;
      found_match = true;
      const sync_pb::FaviconImageSpecifics& image_specifics =
          image_data_list[j].GetSpecifics().favicon_image();
      sync_pb::FaviconImageSpecifics expected_image_specifics;
      FillImageSpecifics(test_data, &expected_image_specifics);
      if (image_specifics.SerializeAsString() !=
          expected_image_specifics.SerializeAsString()) {
        return testing::AssertionFailure() << "Image data doesn't match.";
      }
      const sync_pb::FaviconTrackingSpecifics& tracking_specifics =
          tracking_data_list[j].GetSpecifics().favicon_tracking();
      sync_pb::FaviconTrackingSpecifics expected_tracking_specifics;
      FillTrackingSpecifics(test_data, &expected_tracking_specifics);
      if (tracking_specifics.SerializeAsString() !=
          expected_tracking_specifics.SerializeAsString()) {
        return testing::AssertionFailure() << "Tracking data doesn't match.";
      }
    }
    if (!found_match)
      return testing::AssertionFailure() << "Could not find favicon.";
  }
  return testing::AssertionSuccess();
}

scoped_ptr<syncer::SyncChangeProcessor>
SyncFaviconCacheTest::CreateAndPassProcessor() {
  return scoped_ptr<syncer::SyncChangeProcessor>(
      new SyncChangeProcessorDelegate(sync_processor_.get()));
}

scoped_ptr<syncer::SyncErrorFactory> SyncFaviconCacheTest::
    CreateAndPassSyncErrorFactory() {
  return scoped_ptr<syncer::SyncErrorFactory>(
      new syncer::SyncErrorFactoryMock());
}

// A freshly constructed cache should be empty.
TEST_F(SyncFaviconCacheTest, Empty) {
  EXPECT_EQ(0U, GetFaviconCount());
}

TEST_F(SyncFaviconCacheTest, ReceiveSyncFavicon) {
  std::string page_url = "http://www.google.com";
  std::string fav_url = "http://www.google.com/favicon.ico";
  std::string bytes = "bytes";
  EXPECT_EQ(0U, GetFaviconCount());
  cache()->OnReceivedSyncFavicon(GURL(page_url), GURL(fav_url), bytes, 0);
  EXPECT_EQ(1U, GetFaviconCount());
  EXPECT_TRUE(ExpectFaviconEquals(page_url, bytes));
}

TEST_F(SyncFaviconCacheTest, ReceiveEmptySyncFavicon) {
  std::string page_url = "http://www.google.com";
  std::string fav_url = "http://www.google.com/favicon.ico";
  std::string bytes = "bytes";
  EXPECT_EQ(0U, GetFaviconCount());
  cache()->OnReceivedSyncFavicon(GURL(page_url),
                                 GURL(fav_url),
                                 std::string(),
                                 0);
  EXPECT_EQ(0U, GetFaviconCount());
  EXPECT_FALSE(ExpectFaviconEquals(page_url, std::string()));

  // Then receive the actual favicon.
  cache()->OnReceivedSyncFavicon(GURL(page_url), GURL(fav_url), bytes, 0);
  EXPECT_EQ(1U, GetFaviconCount());
  EXPECT_TRUE(ExpectFaviconEquals(page_url, bytes));
}

TEST_F(SyncFaviconCacheTest, ReceiveUpdatedSyncFavicon) {
  std::string page_url = "http://www.google.com";
  std::string fav_url = "http://www.google.com/favicon.ico";
  std::string bytes = "bytes";
  std::string bytes2 = "bytes2";
  EXPECT_EQ(0U, GetFaviconCount());
  cache()->OnReceivedSyncFavicon(GURL(page_url), GURL(fav_url), bytes, 0);
  EXPECT_EQ(1U, GetFaviconCount());
  EXPECT_TRUE(ExpectFaviconEquals(page_url, bytes));

  // The cache should not update existing favicons from tab sync favicons
  // (which can be reassociated several times).
  cache()->OnReceivedSyncFavicon(GURL(page_url), GURL(fav_url), bytes2, 0);
  EXPECT_EQ(1U, GetFaviconCount());
  EXPECT_TRUE(ExpectFaviconEquals(page_url, bytes));
  EXPECT_FALSE(ExpectFaviconEquals(page_url, bytes2));
}

TEST_F(SyncFaviconCacheTest, MultipleMappings) {
  std::string page_url = "http://www.google.com";
  std::string page2_url = "http://bla.google.com";
  std::string fav_url = "http://www.google.com/favicon.ico";
  std::string bytes = "bytes";
  EXPECT_EQ(0U, GetFaviconCount());
  cache()->OnReceivedSyncFavicon(GURL(page_url), GURL(fav_url), bytes, 0);
  EXPECT_EQ(1U, GetFaviconCount());
  EXPECT_TRUE(ExpectFaviconEquals(page_url, bytes));

  // Map another page to the same favicon. They should share the same data.
  cache()->OnReceivedSyncFavicon(GURL(page2_url), GURL(fav_url), bytes, 0);
  EXPECT_EQ(1U, GetFaviconCount());
  EXPECT_TRUE(ExpectFaviconEquals(page2_url, bytes));
}

TEST_F(SyncFaviconCacheTest, SyncEmpty) {
  syncer::SyncMergeResult merge_result =
      cache()->MergeDataAndStartSyncing(syncer::FAVICON_IMAGES,
                                        syncer::SyncDataList(),
                                        CreateAndPassProcessor(),
                                        CreateAndPassSyncErrorFactory());

  EXPECT_EQ(0U, cache()->GetAllSyncData(syncer::FAVICON_IMAGES).size());
  EXPECT_EQ(0U, processor()->GetAndResetChangeList().size());
  EXPECT_EQ(0, merge_result.num_items_added());
  EXPECT_EQ(0, merge_result.num_items_modified());
  EXPECT_EQ(0, merge_result.num_items_deleted());
  EXPECT_EQ(0, merge_result.num_items_before_association());
  EXPECT_EQ(0, merge_result.num_items_after_association());

  merge_result =
      cache()->MergeDataAndStartSyncing(syncer::FAVICON_TRACKING,
                                        syncer::SyncDataList(),
                                        CreateAndPassProcessor(),
                                        CreateAndPassSyncErrorFactory());

  EXPECT_EQ(0U, cache()->GetAllSyncData(syncer::FAVICON_TRACKING).size());
  EXPECT_EQ(0U, processor()->GetAndResetChangeList().size());
  EXPECT_EQ(0, merge_result.num_items_added());
  EXPECT_EQ(0, merge_result.num_items_modified());
  EXPECT_EQ(0, merge_result.num_items_deleted());
  EXPECT_EQ(0, merge_result.num_items_before_association());
  EXPECT_EQ(0, merge_result.num_items_after_association());
}

// Setting up sync with existing local favicons should push those favicons into
// sync.
TEST_F(SyncFaviconCacheTest, SyncExistingLocal) {
  std::vector<syncer::SyncChange::SyncChangeType> expected_change_types;
  std::vector<int> expected_icons;
  for (int i = 0; i < kTotalFavicons; ++i) {
    TestFaviconData favicon = BuildFaviconData(i);
    cache()->OnReceivedSyncFavicon(favicon.page_url,
                                   favicon.icon_url,
                                   favicon.image_16,
                                   i);
    expected_change_types.push_back(syncer::SyncChange::ACTION_ADD);
    expected_icons.push_back(i);
  }

  syncer::SyncMergeResult merge_result =
      cache()->MergeDataAndStartSyncing(syncer::FAVICON_IMAGES,
                                        syncer::SyncDataList(),
                                        CreateAndPassProcessor(),
                                        CreateAndPassSyncErrorFactory());
  EXPECT_EQ((unsigned long)kTotalFavicons,
            cache()->GetAllSyncData(syncer::FAVICON_IMAGES).size());
  syncer::SyncChangeList change_list = processor()->GetAndResetChangeList();
  EXPECT_TRUE(VerifyChanges(syncer::FAVICON_IMAGES,
                            expected_change_types,
                            expected_icons,
                            change_list));
  EXPECT_EQ(0, merge_result.num_items_added());
  EXPECT_EQ(0, merge_result.num_items_modified());
  EXPECT_EQ(0, merge_result.num_items_deleted());
  EXPECT_EQ(kTotalFavicons, merge_result.num_items_before_association());
  EXPECT_EQ(kTotalFavicons, merge_result.num_items_after_association());

  merge_result =
      cache()->MergeDataAndStartSyncing(syncer::FAVICON_TRACKING,
                                        syncer::SyncDataList(),
                                        CreateAndPassProcessor(),
                                        CreateAndPassSyncErrorFactory());
  EXPECT_EQ((unsigned long)kTotalFavicons,
            cache()->GetAllSyncData(syncer::FAVICON_TRACKING).size());
  change_list = processor()->GetAndResetChangeList();
  EXPECT_TRUE(VerifyChanges(syncer::FAVICON_TRACKING,
                            expected_change_types,
                            expected_icons,
                            change_list));
  EXPECT_EQ(0, merge_result.num_items_added());
  EXPECT_EQ(0, merge_result.num_items_modified());
  EXPECT_EQ(0, merge_result.num_items_deleted());
  EXPECT_EQ(kTotalFavicons, merge_result.num_items_before_association());
  EXPECT_EQ(kTotalFavicons, merge_result.num_items_after_association());
}

// Setting up sync with existing sync data should load that data into the local
// cache.
TEST_F(SyncFaviconCacheTest, SyncExistingRemote) {
  syncer::SyncDataList initial_image_data, initial_tracking_data;
  std::vector<int> expected_icons;
  for (int i = 0; i < kTotalFavicons; ++i) {
    expected_icons.push_back(i);
    sync_pb::EntitySpecifics image_specifics, tracking_specifics;
    FillImageSpecifics(BuildFaviconData(i),
                       image_specifics.mutable_favicon_image());
    initial_image_data.push_back(
        syncer::SyncData::CreateRemoteData(1,
                                           image_specifics));
    FillTrackingSpecifics(BuildFaviconData(i),
                          tracking_specifics.mutable_favicon_tracking());
    initial_tracking_data.push_back(
        syncer::SyncData::CreateRemoteData(1,
                                           tracking_specifics));
  }

  syncer::SyncMergeResult merge_result =
      cache()->MergeDataAndStartSyncing(syncer::FAVICON_IMAGES,
                                        initial_image_data,
                                        CreateAndPassProcessor(),
                                        CreateAndPassSyncErrorFactory());
  EXPECT_EQ((unsigned long)kTotalFavicons,
            cache()->GetAllSyncData(syncer::FAVICON_IMAGES).size());
  EXPECT_EQ(0U, processor()->GetAndResetChangeList().size());
  EXPECT_EQ(kTotalFavicons, merge_result.num_items_added());
  EXPECT_EQ(0, merge_result.num_items_modified());
  EXPECT_EQ(0, merge_result.num_items_deleted());
  EXPECT_EQ(0, merge_result.num_items_before_association());
  EXPECT_EQ(kTotalFavicons, merge_result.num_items_after_association());

  merge_result =
      cache()->MergeDataAndStartSyncing(syncer::FAVICON_TRACKING,
                                        initial_tracking_data,
                                        CreateAndPassProcessor(),
                                        CreateAndPassSyncErrorFactory());
  EXPECT_EQ((unsigned long)kTotalFavicons,
            cache()->GetAllSyncData(syncer::FAVICON_TRACKING).size());
  EXPECT_EQ(0U, processor()->GetAndResetChangeList().size());
  EXPECT_EQ(0, merge_result.num_items_added());
  EXPECT_EQ(kTotalFavicons, merge_result.num_items_modified());
  EXPECT_EQ(0, merge_result.num_items_deleted());
  EXPECT_EQ(kTotalFavicons, merge_result.num_items_before_association());
  EXPECT_EQ(kTotalFavicons, merge_result.num_items_after_association());

  ASSERT_TRUE(VerifyLocalIcons(expected_icons));
}

// Setting up sync with local data and sync data should merge the two image
// sets, with remote data having priority in case both exist.
TEST_F(SyncFaviconCacheTest, SyncMergesImages) {
  // First go through and add local 16p favicons.
  for (int i = 0; i < kTotalFavicons; ++i) {
    TestFaviconData favicon = BuildFaviconData(i);
    cache()->OnReceivedSyncFavicon(favicon.page_url,
                                   favicon.icon_url,
                                   favicon.image_16,
                                   i);
  }

  // Then go through and create the initial sync data, which does not have 16p
  // favicons for the first half, and has custom 16p favicons for the second.
  std::vector<syncer::SyncChange::SyncChangeType> expected_change_types;
  std::vector<int> expected_icons;
  std::vector<TestFaviconData> expected_data;
  syncer::SyncDataList initial_image_data, initial_tracking_data;
  for (int i = 0; i < kTotalFavicons; ++i) {
    sync_pb::EntitySpecifics image_specifics, tracking_specifics;
    TestFaviconData test_data = BuildFaviconData(i);
    if (i < kTotalFavicons/2) {
      test_data.image_16 = std::string();
      expected_icons.push_back(i);
      expected_change_types.push_back(syncer::SyncChange::ACTION_UPDATE);
    } else {
      test_data.image_16 += "custom";
      expected_data.push_back(test_data);
    }
    FillImageSpecifics(test_data,
                       image_specifics.mutable_favicon_image());

    initial_image_data.push_back(
        syncer::SyncData::CreateRemoteData(1,
                                           image_specifics));
    FillTrackingSpecifics(test_data,
                          tracking_specifics.mutable_favicon_tracking());
    initial_tracking_data.push_back(
        syncer::SyncData::CreateRemoteData(1,
                                           tracking_specifics));
  }

  syncer::SyncMergeResult merge_result =
      cache()->MergeDataAndStartSyncing(syncer::FAVICON_IMAGES,
                                        initial_image_data,
                                        CreateAndPassProcessor(),
                                        CreateAndPassSyncErrorFactory());
  EXPECT_EQ((unsigned long)kTotalFavicons,
            cache()->GetAllSyncData(syncer::FAVICON_IMAGES).size());
  syncer::SyncChangeList changes = processor()->GetAndResetChangeList();
  EXPECT_EQ((unsigned long)kTotalFavicons/2, changes.size());
  EXPECT_EQ(0, merge_result.num_items_added());
  EXPECT_EQ(kTotalFavicons, merge_result.num_items_modified());
  EXPECT_EQ(0, merge_result.num_items_deleted());
  EXPECT_EQ(kTotalFavicons, merge_result.num_items_before_association());
  EXPECT_EQ(kTotalFavicons, merge_result.num_items_after_association());

  merge_result =
      cache()->MergeDataAndStartSyncing(syncer::FAVICON_TRACKING,
                                        initial_tracking_data,
                                        CreateAndPassProcessor(),
                                        CreateAndPassSyncErrorFactory());
  EXPECT_EQ((unsigned long)kTotalFavicons,
            cache()->GetAllSyncData(syncer::FAVICON_TRACKING).size());
  EXPECT_EQ(0U, processor()->GetAndResetChangeList().size());
  EXPECT_EQ(0, merge_result.num_items_added());
  EXPECT_EQ(kTotalFavicons, merge_result.num_items_modified());
  EXPECT_EQ(0, merge_result.num_items_deleted());
  EXPECT_EQ(kTotalFavicons, merge_result.num_items_before_association());
  EXPECT_EQ(kTotalFavicons, merge_result.num_items_after_association());

  ASSERT_TRUE(VerifyLocalIcons(expected_icons));
  ASSERT_TRUE(VerifyLocalCustomIcons(expected_data));
  ASSERT_TRUE(VerifyChanges(syncer::FAVICON_IMAGES,
                            expected_change_types,
                            expected_icons,
                            changes));
}

// Setting up sync with local data and sync data should merge the two tracking
// sets, such that the visit time is the most recent.
TEST_F(SyncFaviconCacheTest, SyncMergesTracking) {
  // First go through and add local 16p favicons.
  for (int i = 0; i < kTotalFavicons; ++i) {
    TestFaviconData favicon = BuildFaviconData(i);
    cache()->OnReceivedSyncFavicon(favicon.page_url,
                                   favicon.icon_url,
                                   favicon.image_16,
                                   i);
  }

  // Then go through and create the initial sync data, which for the first half
  // the local has a newer visit, and for the second the remote does.
  std::vector<syncer::SyncChange::SyncChangeType> expected_change_types;
  std::vector<int> expected_icons;
  std::vector<TestFaviconData> expected_data;
  syncer::SyncDataList initial_image_data, initial_tracking_data;
  for (int i = 0; i < kTotalFavicons; ++i) {
    sync_pb::EntitySpecifics image_specifics, tracking_specifics;
    TestFaviconData test_data = BuildFaviconData(i);
    if (i < kTotalFavicons/2) {
      test_data.last_visit_time = i-1;
      expected_icons.push_back(i);
      expected_change_types.push_back(syncer::SyncChange::ACTION_UPDATE);
    } else {
      test_data.last_visit_time = i+1;
      expected_data.push_back(test_data);
    }
    FillImageSpecifics(test_data,
                       image_specifics.mutable_favicon_image());

    initial_image_data.push_back(
        syncer::SyncData::CreateRemoteData(1,
                                           image_specifics));
    FillTrackingSpecifics(test_data,
                          tracking_specifics.mutable_favicon_tracking());
    initial_tracking_data.push_back(
        syncer::SyncData::CreateRemoteData(1,
                                           tracking_specifics));
  }

  syncer::SyncMergeResult merge_result =
      cache()->MergeDataAndStartSyncing(syncer::FAVICON_IMAGES,
                                        initial_image_data,
                                        CreateAndPassProcessor(),
                                        CreateAndPassSyncErrorFactory());
  EXPECT_EQ((unsigned long)kTotalFavicons,
            cache()->GetAllSyncData(syncer::FAVICON_IMAGES).size());
  EXPECT_EQ(0U, processor()->GetAndResetChangeList().size());
  EXPECT_EQ(0, merge_result.num_items_added());
  EXPECT_EQ(kTotalFavicons, merge_result.num_items_modified());
  EXPECT_EQ(0, merge_result.num_items_deleted());
  EXPECT_EQ(kTotalFavicons, merge_result.num_items_before_association());
  EXPECT_EQ(kTotalFavicons, merge_result.num_items_after_association());

  merge_result =
      cache()->MergeDataAndStartSyncing(syncer::FAVICON_TRACKING,
                                        initial_tracking_data,
                                        CreateAndPassProcessor(),
                                        CreateAndPassSyncErrorFactory());
  EXPECT_EQ((unsigned long)kTotalFavicons,
            cache()->GetAllSyncData(syncer::FAVICON_TRACKING).size());
  syncer::SyncChangeList changes = processor()->GetAndResetChangeList();
  EXPECT_EQ((unsigned long)kTotalFavicons/2, changes.size());
  EXPECT_EQ(0, merge_result.num_items_added());
  EXPECT_EQ(kTotalFavicons, merge_result.num_items_modified());
  EXPECT_EQ(0, merge_result.num_items_deleted());
  EXPECT_EQ(kTotalFavicons, merge_result.num_items_before_association());
  EXPECT_EQ(kTotalFavicons, merge_result.num_items_after_association());

  ASSERT_TRUE(VerifyLocalIcons(expected_icons));
  ASSERT_TRUE(VerifyLocalCustomIcons(expected_data));
  ASSERT_TRUE(VerifyChanges(syncer::FAVICON_TRACKING,
                            expected_change_types,
                            expected_icons,
                            changes));
}

// Receiving old icons (missing image data) should result in pushing the new
// merged icons back to the remote syncer.
TEST_F(SyncFaviconCacheTest, ReceiveStaleImages) {
  syncer::SyncDataList initial_image_data, initial_tracking_data;
  syncer::SyncChangeList stale_changes;
  std::vector<int> expected_icons;
  std::vector<syncer::SyncChange::SyncChangeType> expected_change_types;
  for (int i = 0; i < kTotalFavicons; ++i) {
    expected_icons.push_back(i);
    sync_pb::EntitySpecifics image_specifics, tracking_specifics;
    FillImageSpecifics(BuildFaviconData(i),
                       image_specifics.mutable_favicon_image());
    initial_image_data.push_back(
        syncer::SyncData::CreateRemoteData(1,
                                           image_specifics));
    expected_change_types.push_back(syncer::SyncChange::ACTION_UPDATE);
    image_specifics.mutable_favicon_image()->clear_favicon_web();
    stale_changes.push_back(
        syncer::SyncChange(
             FROM_HERE,
             syncer::SyncChange::ACTION_UPDATE,
             syncer::SyncData::CreateRemoteData(1, image_specifics)));
    FillTrackingSpecifics(BuildFaviconData(i),
                          tracking_specifics.mutable_favicon_tracking());
    initial_tracking_data.push_back(
        syncer::SyncData::CreateRemoteData(1,
                                           tracking_specifics));
  }

  SetUpInitialSync(initial_image_data, initial_tracking_data);

  // Now receive the same icons as an update, but with missing image data.
  cache()->ProcessSyncChanges(FROM_HERE, stale_changes);
  syncer::SyncChangeList changes = processor()->GetAndResetChangeList();
  ASSERT_TRUE(VerifyLocalIcons(expected_icons));
  ASSERT_EQ((unsigned long)kTotalFavicons, changes.size());
  ASSERT_TRUE(VerifyChanges(syncer::FAVICON_IMAGES,
                            expected_change_types,
                            expected_icons,
                            changes));
}

// New icons should be added locally without pushing anything back to the
// remote syncer.
TEST_F(SyncFaviconCacheTest, ReceiveNewImages) {
  syncer::SyncDataList initial_image_data, initial_tracking_data;
  syncer::SyncChangeList new_changes;
  std::vector<int> expected_icons;
  for (int i = 0; i < kTotalFavicons; ++i) {
    expected_icons.push_back(i);
    TestFaviconData test_data = BuildFaviconData(i);
    sync_pb::EntitySpecifics image_specifics, tracking_specifics;
    FillImageSpecifics(test_data,
                       image_specifics.mutable_favicon_image());
    new_changes.push_back(
        syncer::SyncChange(
             FROM_HERE,
             syncer::SyncChange::ACTION_UPDATE,
             syncer::SyncData::CreateRemoteData(1, image_specifics)));
    image_specifics.mutable_favicon_image()->mutable_favicon_web()->
        mutable_favicon()->append("old");
    initial_image_data.push_back(
        syncer::SyncData::CreateRemoteData(1,
                                           image_specifics));
    FillTrackingSpecifics(BuildFaviconData(i),
                          tracking_specifics.mutable_favicon_tracking());
    initial_tracking_data.push_back(
        syncer::SyncData::CreateRemoteData(1,
                                           tracking_specifics));
  }

  SetUpInitialSync(initial_image_data, initial_tracking_data);

  // Now receive the new icons as an update.
  cache()->ProcessSyncChanges(FROM_HERE, new_changes);
  EXPECT_EQ(0U, processor()->GetAndResetChangeList().size());
  ASSERT_TRUE(VerifyLocalIcons(expected_icons));
}

// Recieving the same icons as the local data should have no effect.
TEST_F(SyncFaviconCacheTest, ReceiveSameImages) {
  syncer::SyncDataList initial_image_data, initial_tracking_data;
  syncer::SyncChangeList same_changes;
  std::vector<int> expected_icons;
  for (int i = 0; i < kTotalFavicons; ++i) {
    expected_icons.push_back(i);
    TestFaviconData test_data = BuildFaviconData(i);
    sync_pb::EntitySpecifics image_specifics, tracking_specifics;
    FillImageSpecifics(test_data,
                       image_specifics.mutable_favicon_image());
    same_changes.push_back(
        syncer::SyncChange(
             FROM_HERE,
             syncer::SyncChange::ACTION_UPDATE,
             syncer::SyncData::CreateRemoteData(1, image_specifics)));
    initial_image_data.push_back(
        syncer::SyncData::CreateRemoteData(1,
                                           image_specifics));
    FillTrackingSpecifics(BuildFaviconData(i),
                          tracking_specifics.mutable_favicon_tracking());
    initial_tracking_data.push_back(
        syncer::SyncData::CreateRemoteData(1,
                                           tracking_specifics));
  }

  SetUpInitialSync(initial_image_data, initial_tracking_data);

  // Now receive the new icons as an update.
  cache()->ProcessSyncChanges(FROM_HERE, same_changes);
  EXPECT_EQ(0U, processor()->GetAndResetChangeList().size());
  ASSERT_TRUE(VerifyLocalIcons(expected_icons));
}

// Receiving stale tracking (old visit times) should result in pushing back
// the newer visit times to the remote syncer.
TEST_F(SyncFaviconCacheTest, ReceiveStaleTracking) {
  syncer::SyncDataList initial_image_data, initial_tracking_data;
  syncer::SyncChangeList stale_changes;
  std::vector<int> expected_icons;
  std::vector<syncer::SyncChange::SyncChangeType> expected_change_types;
  for (int i = 0; i < kTotalFavicons; ++i) {
    expected_icons.push_back(i);
    sync_pb::EntitySpecifics image_specifics, tracking_specifics;
    FillImageSpecifics(BuildFaviconData(i),
                       image_specifics.mutable_favicon_image());
    initial_image_data.push_back(
        syncer::SyncData::CreateRemoteData(1,
                                           image_specifics));
    expected_change_types.push_back(syncer::SyncChange::ACTION_UPDATE);
    FillTrackingSpecifics(BuildFaviconData(i),
                          tracking_specifics.mutable_favicon_tracking());
    initial_tracking_data.push_back(
        syncer::SyncData::CreateRemoteData(1,
                                           tracking_specifics));
    tracking_specifics.mutable_favicon_tracking()->set_last_visit_time_ms(-1);
    stale_changes.push_back(
        syncer::SyncChange(
             FROM_HERE,
             syncer::SyncChange::ACTION_UPDATE,
             syncer::SyncData::CreateRemoteData(1, tracking_specifics)));
  }

  SetUpInitialSync(initial_image_data, initial_tracking_data);

  // Now receive the same icons as an update, but with missing image data.
  cache()->ProcessSyncChanges(FROM_HERE, stale_changes);
  syncer::SyncChangeList changes = processor()->GetAndResetChangeList();
  ASSERT_TRUE(VerifyLocalIcons(expected_icons));
  ASSERT_EQ((unsigned long)kTotalFavicons, changes.size());
  ASSERT_TRUE(VerifyChanges(syncer::FAVICON_TRACKING,
                            expected_change_types,
                            expected_icons,
                            changes));
}

// New tracking information should be added locally without pushing anything
// back to the remote syncer.
TEST_F(SyncFaviconCacheTest, ReceiveNewTracking) {
  syncer::SyncDataList initial_image_data, initial_tracking_data;
  syncer::SyncChangeList new_changes;
  std::vector<int> expected_icons;
  // We start from one here so that we don't have to deal with a -1 visit time.
  for (int i = 1; i <= kTotalFavicons; ++i) {
    expected_icons.push_back(i);
    sync_pb::EntitySpecifics image_specifics, tracking_specifics;
    FillImageSpecifics(BuildFaviconData(i),
                       image_specifics.mutable_favicon_image());
    initial_image_data.push_back(
        syncer::SyncData::CreateRemoteData(1,
                                           image_specifics));
    FillTrackingSpecifics(BuildFaviconData(i),
                          tracking_specifics.mutable_favicon_tracking());
    new_changes.push_back(
        syncer::SyncChange(
             FROM_HERE,
             syncer::SyncChange::ACTION_UPDATE,
             syncer::SyncData::CreateRemoteData(1, tracking_specifics)));
    tracking_specifics.mutable_favicon_tracking()->set_last_visit_time_ms(i-1);
    initial_tracking_data.push_back(
        syncer::SyncData::CreateRemoteData(1,
                                           tracking_specifics));
  }

  SetUpInitialSync(initial_image_data, initial_tracking_data);

  // Now receive the new icons as an update.
  cache()->ProcessSyncChanges(FROM_HERE, new_changes);
  EXPECT_EQ(0U, processor()->GetAndResetChangeList().size());
  ASSERT_TRUE(VerifyLocalIcons(expected_icons));
}

// Receiving the same tracking information as the local data should have no
// effect.
TEST_F(SyncFaviconCacheTest, ReceiveSameTracking) {
  syncer::SyncDataList initial_image_data, initial_tracking_data;
  syncer::SyncChangeList same_changes;
  std::vector<int> expected_icons;
  for (int i = 0; i < kTotalFavicons; ++i) {
    expected_icons.push_back(i);
    sync_pb::EntitySpecifics image_specifics, tracking_specifics;
    FillImageSpecifics(BuildFaviconData(i),
                       image_specifics.mutable_favicon_image());
    initial_image_data.push_back(
        syncer::SyncData::CreateRemoteData(1,
                                           image_specifics));
    FillTrackingSpecifics(BuildFaviconData(i),
                          tracking_specifics.mutable_favicon_tracking());
    initial_tracking_data.push_back(
        syncer::SyncData::CreateRemoteData(1,
                                           tracking_specifics));
    same_changes.push_back(
        syncer::SyncChange(
             FROM_HERE,
             syncer::SyncChange::ACTION_UPDATE,
             syncer::SyncData::CreateRemoteData(1, tracking_specifics)));
  }

  SetUpInitialSync(initial_image_data, initial_tracking_data);

  // Now receive the new icons as an update.
  cache()->ProcessSyncChanges(FROM_HERE, same_changes);
  EXPECT_EQ(0U, processor()->GetAndResetChangeList().size());
  ASSERT_TRUE(VerifyLocalIcons(expected_icons));
}

// Verify we can delete favicons after setting up sync.
TEST_F(SyncFaviconCacheTest, DeleteFavicons) {
  syncer::SyncDataList initial_image_data, initial_tracking_data;
  syncer::SyncChangeList deletions;
  for (int i = 0; i < kTotalFavicons; ++i) {
    sync_pb::EntitySpecifics image_specifics, tracking_specifics;
    FillImageSpecifics(BuildFaviconData(i),
                       image_specifics.mutable_favicon_image());
    initial_image_data.push_back(
        syncer::SyncData::CreateRemoteData(1,
                                           image_specifics));
    FillTrackingSpecifics(BuildFaviconData(i),
                          tracking_specifics.mutable_favicon_tracking());
    initial_tracking_data.push_back(
        syncer::SyncData::CreateRemoteData(1,
                                           tracking_specifics));
    deletions.push_back(
        syncer::SyncChange(
             FROM_HERE,
             syncer::SyncChange::ACTION_DELETE,
             syncer::SyncData::CreateRemoteData(1, tracking_specifics)));
  }

  SetUpInitialSync(initial_image_data, initial_tracking_data);

  // Now receive the new icons as an update.
  EXPECT_EQ((unsigned long)kTotalFavicons, GetFaviconCount());
  cache()->ProcessSyncChanges(FROM_HERE, deletions);
  EXPECT_EQ(0U, processor()->GetAndResetChangeList().size());
  EXPECT_EQ(0U, GetFaviconCount());
}

}  // namespace browser_sync
