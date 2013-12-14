// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/dom_distiller_service.h"

#include "base/bind.h"
#include "base/containers/hash_tables.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "components/dom_distiller/core/article_entry.h"
#include "components/dom_distiller/core/dom_distiller_model.h"
#include "components/dom_distiller/core/dom_distiller_store.h"
#include "components/dom_distiller/core/dom_distiller_test_util.h"
#include "components/dom_distiller/core/fake_db.h"
#include "components/dom_distiller/core/fake_distiller.h"
#include "components/dom_distiller/core/task_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Invoke;
using testing::Return;
using testing::_;

namespace dom_distiller {
namespace test {

namespace {

class FakeViewRequestDelegate : public ViewRequestDelegate {
 public:
  virtual ~FakeViewRequestDelegate() {}
  MOCK_METHOD1(OnArticleReady, void(DistilledPageProto* proto));
};

class MockDistillerObserver : public DomDistillerObserver {
 public:
  MOCK_METHOD1(ArticleEntriesUpdated, void(const std::vector<ArticleUpdate>&));
  virtual ~MockDistillerObserver() {}
};

}  // namespace

class DomDistillerServiceTest : public testing::Test {
 public:
  virtual void SetUp() {
    main_loop_.reset(new base::MessageLoop());
    FakeDB* fake_db = new FakeDB(&db_model_);
    FakeDB::EntryMap store_model;
    store_ = test::util::CreateStoreWithFakeDB(fake_db, &store_model);
    distiller_factory_ = new MockDistillerFactory();
    service_.reset(new DomDistillerService(
        scoped_ptr<DomDistillerStoreInterface>(store_),
        scoped_ptr<DistillerFactory>(distiller_factory_)));
    fake_db->InitCallback(true);
    fake_db->LoadCallback(true);
  }

  virtual void TearDown() {
    base::RunLoop().RunUntilIdle();
    store_ = NULL;
    distiller_factory_ = NULL;
    service_.reset();
  }

 protected:
  // store is owned by service_.
  DomDistillerStoreInterface* store_;
  MockDistillerFactory* distiller_factory_;
  scoped_ptr<DomDistillerService> service_;
  scoped_ptr<base::MessageLoop> main_loop_;
  FakeDB::EntryMap db_model_;
};

TEST_F(DomDistillerServiceTest, TestViewEntry) {
  FakeDistiller* distiller = new FakeDistiller();
  EXPECT_CALL(*distiller_factory_, CreateDistillerImpl())
      .WillOnce(Return(distiller));

  GURL url("http://www.example.com/p1");
  std::string entry_id("id0");
  ArticleEntry entry;
  entry.set_entry_id(entry_id);
  entry.add_pages()->set_url(url.spec());

  store_->AddEntry(entry);

  FakeViewRequestDelegate viewer_delegate;
  scoped_ptr<ViewerHandle> handle =
      service_->ViewEntry(&viewer_delegate, entry_id);

  ASSERT_FALSE(distiller->GetCallback().is_null());

  scoped_ptr<DistilledPageProto> proto(new DistilledPageProto);
  EXPECT_CALL(viewer_delegate, OnArticleReady(proto.get()));

  distiller->RunDistillerCallback(proto.Pass());
}

TEST_F(DomDistillerServiceTest, TestViewUrl) {
  FakeDistiller* distiller = new FakeDistiller();
  EXPECT_CALL(*distiller_factory_, CreateDistillerImpl())
      .WillOnce(Return(distiller));

  FakeViewRequestDelegate viewer_delegate;
  GURL url("http://www.example.com/p1");
  scoped_ptr<ViewerHandle> handle = service_->ViewUrl(&viewer_delegate, url);

  ASSERT_FALSE(distiller->GetCallback().is_null());
  EXPECT_EQ(url, distiller->GetUrl());

  scoped_ptr<DistilledPageProto> proto(new DistilledPageProto);
  EXPECT_CALL(viewer_delegate, OnArticleReady(proto.get()));

  distiller->RunDistillerCallback(proto.Pass());
}

TEST_F(DomDistillerServiceTest, TestMultipleViewUrl) {
  FakeDistiller* distiller = new FakeDistiller();
  FakeDistiller* distiller2 = new FakeDistiller();
  EXPECT_CALL(*distiller_factory_, CreateDistillerImpl())
      .WillOnce(Return(distiller))
      .WillOnce(Return(distiller2));

  FakeViewRequestDelegate viewer_delegate;
  FakeViewRequestDelegate viewer_delegate2;

  GURL url("http://www.example.com/p1");
  GURL url2("http://www.example.com/a/p1");

  scoped_ptr<ViewerHandle> handle = service_->ViewUrl(&viewer_delegate, url);
  scoped_ptr<ViewerHandle> handle2 = service_->ViewUrl(&viewer_delegate2, url2);

  ASSERT_FALSE(distiller->GetCallback().is_null());
  EXPECT_EQ(url, distiller->GetUrl());

  scoped_ptr<DistilledPageProto> proto(new DistilledPageProto);
  EXPECT_CALL(viewer_delegate, OnArticleReady(proto.get()));

  distiller->RunDistillerCallback(proto.Pass());

  ASSERT_FALSE(distiller2->GetCallback().is_null());
  EXPECT_EQ(url2, distiller2->GetUrl());

  scoped_ptr<DistilledPageProto> proto2(new DistilledPageProto);
  EXPECT_CALL(viewer_delegate2, OnArticleReady(proto2.get()));

  distiller2->RunDistillerCallback(proto2.Pass());
}

TEST_F(DomDistillerServiceTest, TestViewUrlCancelled) {
  FakeDistiller* distiller = new FakeDistiller();
  EXPECT_CALL(*distiller_factory_, CreateDistillerImpl())
      .WillOnce(Return(distiller));

  bool distiller_destroyed = false;
  EXPECT_CALL(*distiller, Die())
      .WillOnce(testing::Assign(&distiller_destroyed, true));

  FakeViewRequestDelegate viewer_delegate;
  GURL url("http://www.example.com/p1");
  scoped_ptr<ViewerHandle> handle = service_->ViewUrl(&viewer_delegate, url);

  ASSERT_FALSE(distiller->GetCallback().is_null());
  EXPECT_EQ(url, distiller->GetUrl());

  EXPECT_CALL(viewer_delegate, OnArticleReady(_)).Times(0);

  EXPECT_FALSE(distiller_destroyed);

  handle.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(distiller_destroyed);
}

TEST_F(DomDistillerServiceTest, TestAddAndRemoveEntry) {
  FakeDistiller* distiller = new FakeDistiller();
  EXPECT_CALL(*distiller_factory_, CreateDistillerImpl())
      .WillOnce(Return(distiller));

  GURL url("http://www.example.com/p1");

  service_->AddToList(url);

  ASSERT_FALSE(distiller->GetCallback().is_null());
  EXPECT_EQ(url, distiller->GetUrl());

  ArticleEntry entry;
  EXPECT_TRUE(store_->GetEntryByUrl(url, &entry));
  EXPECT_EQ(1u, store_->GetEntries().size());
  store_->RemoveEntry(entry);
  EXPECT_EQ(0u, store_->GetEntries().size());
}

TEST_F(DomDistillerServiceTest, TestObserverIntegration) {
  FakeDistiller* distiller = new FakeDistiller();

  MockDistillerObserver observer;
  service_->AddObserver(&observer);

  EXPECT_CALL(*distiller_factory_, CreateDistillerImpl())
      .WillOnce(Return(distiller));
  EXPECT_CALL(observer, ArticleEntriesUpdated(_));

  GURL url("http://www.example.com/p1");

  service_->AddToList(url);

  ArticleEntry entry;
  EXPECT_TRUE(store_->GetEntryByUrl(url, &entry));
  std::vector<DomDistillerObserver::ArticleUpdate> expected_updates;
  DomDistillerObserver::ArticleUpdate update;
  update.entry_id = entry.entry_id();
  update.update_type = DomDistillerObserver::ArticleUpdate::UPDATE;
  expected_updates.push_back(update);

  EXPECT_CALL(
      observer,
      ArticleEntriesUpdated(test::util::HasExpectedUpdates(expected_updates)));

  scoped_ptr<DistilledPageProto> proto(new DistilledPageProto);
  distiller->RunDistillerCallback(proto.Pass());
}

}  // namespace test
}  // namespace dom_distiller
