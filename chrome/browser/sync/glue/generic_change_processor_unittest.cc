// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/generic_change_processor.h"

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/sync/glue/data_type_error_handler_mock.h"
#include "sync/api/fake_syncable_service.h"
#include "sync/api/sync_merge_result.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/read_node.h"
#include "sync/internal_api/public/test/test_user_share.h"
#include "sync/internal_api/public/user_share.h"
#include "sync/internal_api/public/write_node.h"
#include "sync/internal_api/public/write_transaction.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {

namespace {

class GenericChangeProcessorTest : public testing::Test {
 public:
  // It doesn't matter which type we use.  Just pick one.
  static const syncer::ModelType kType = syncer::PREFERENCES;

  GenericChangeProcessorTest() :
      loop_(base::MessageLoop::TYPE_UI),
      sync_merge_result_(kType),
      merge_result_ptr_factory_(&sync_merge_result_),
      syncable_service_ptr_factory_(&fake_syncable_service_) {
  }

  virtual void SetUp() OVERRIDE {
    test_user_share_.SetUp();
    syncer::TestUserShare::CreateRoot(kType, test_user_share_.user_share());
    change_processor_.reset(
        new GenericChangeProcessor(
            &data_type_error_handler_,
            syncable_service_ptr_factory_.GetWeakPtr(),
            merge_result_ptr_factory_.GetWeakPtr(),
            test_user_share_.user_share()));
  }

  virtual void TearDown() OVERRIDE {
    test_user_share_.TearDown();
  }

  void BuildChildNodes(int n) {
    syncer::WriteTransaction trans(FROM_HERE, user_share());
    syncer::ReadNode root(&trans);
    ASSERT_EQ(syncer::BaseNode::INIT_OK,
              root.InitByTagLookup(syncer::ModelTypeToRootTag(kType)));
    for (int i = 0; i < n; ++i) {
      syncer::WriteNode node(&trans);
      node.InitUniqueByCreation(kType, root, base::StringPrintf("node%05d", i));
    }
  }

  GenericChangeProcessor* change_processor() {
    return change_processor_.get();
  }

  syncer::UserShare* user_share() {
    return test_user_share_.user_share();
  }

 private:
  base::MessageLoop loop_;

  syncer::SyncMergeResult sync_merge_result_;
  base::WeakPtrFactory<syncer::SyncMergeResult> merge_result_ptr_factory_;

  syncer::FakeSyncableService fake_syncable_service_;
  base::WeakPtrFactory<syncer::FakeSyncableService>
      syncable_service_ptr_factory_;

  DataTypeErrorHandlerMock data_type_error_handler_;
  syncer::TestUserShare test_user_share_;

  scoped_ptr<GenericChangeProcessor> change_processor_;
};

// This test exercises GenericChangeProcessor's GetSyncDataForType function.
// It's not a great test, but, by modifying some of the parameters, you could
// turn it into a micro-benchmark for model association.
TEST_F(GenericChangeProcessorTest, StressGetSyncDataForType) {
  const int kNumChildNodes = 1000;
  const int kRepeatCount = 1;

  ASSERT_NO_FATAL_FAILURE(BuildChildNodes(kNumChildNodes));

  for (int i = 0; i < kRepeatCount; ++i) {
    syncer::SyncDataList sync_data;
    change_processor()->GetSyncDataForType(kType, &sync_data);

    // Start with a simple test.  We can add more in-depth testing later.
    EXPECT_EQ(static_cast<size_t>(kNumChildNodes), sync_data.size());
  }
}

}  // namespace

}  // namespace browser_sync

