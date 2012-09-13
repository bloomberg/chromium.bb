// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/hyperbolic_download_item_notifier.h"

#include "content/public/test/mock_download_item.h"
#include "content/public/test/mock_download_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::NiceMock;
using testing::SetArgPointee;
using testing::_;

namespace {

class MockNotifierObserver : public HyperbolicDownloadItemNotifier::Observer {
 public:
  MockNotifierObserver() {
  }
  virtual ~MockNotifierObserver() {}

  MOCK_METHOD2(OnDownloadCreated, void(
      content::DownloadManager* manager, content::DownloadItem* item));
  MOCK_METHOD2(OnDownloadUpdated, void(
      content::DownloadManager* manager, content::DownloadItem* item));
  MOCK_METHOD2(OnDownloadOpened, void(
      content::DownloadManager* manager, content::DownloadItem* item));
  MOCK_METHOD2(OnDownloadRemoved, void(
      content::DownloadManager* manager, content::DownloadItem* item));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockNotifierObserver);
};

class HyperbolicDownloadItemNotifierTest : public testing::Test {
 public:
  HyperbolicDownloadItemNotifierTest()
    : download_manager_(new content::MockDownloadManager) {
  }

  virtual ~HyperbolicDownloadItemNotifierTest() {}

  content::MockDownloadManager& manager() {
    return *download_manager_.get();
  }

  content::MockDownloadItem& item() { return item_; }

  content::DownloadItem::Observer* NotifierAsItemObserver() const {
    return notifier_.get();
  }

  content::DownloadManager::Observer* NotifierAsManagerObserver() const {
    return notifier_.get();
  }

  MockNotifierObserver& observer() { return observer_; }

  void SetNotifier() {
    EXPECT_CALL(*download_manager_.get(), AddObserver(_));
    notifier_.reset(new HyperbolicDownloadItemNotifier(
        download_manager_.get(), &observer_));
  }

  void ClearNotifier() {
    notifier_.reset();
  }

 private:
  NiceMock<content::MockDownloadItem> item_;
  scoped_refptr<content::MockDownloadManager> download_manager_;
  scoped_ptr<HyperbolicDownloadItemNotifier> notifier_;
  NiceMock<MockNotifierObserver> observer_;

  DISALLOW_COPY_AND_ASSIGN(HyperbolicDownloadItemNotifierTest);
};

}  // namespace

TEST_F(HyperbolicDownloadItemNotifierTest,
    HyperbolicDownloadItemNotifierTest_0) {
  content::DownloadManager::DownloadVector items;
  items.push_back(&item());
  EXPECT_CALL(manager(), GetAllDownloads(_))
    .WillOnce(SetArgPointee<0>(items));
  EXPECT_CALL(item(), AddObserver(_));
  SetNotifier();

  EXPECT_CALL(observer(), OnDownloadUpdated(&manager(), &item()));
  NotifierAsItemObserver()->OnDownloadUpdated(&item());

  EXPECT_CALL(observer(), OnDownloadOpened(&manager(), &item()));
  NotifierAsItemObserver()->OnDownloadOpened(&item());

  EXPECT_CALL(observer(), OnDownloadRemoved(&manager(), &item()));
  NotifierAsItemObserver()->OnDownloadRemoved(&item());

  EXPECT_CALL(item(), RemoveObserver(NotifierAsItemObserver()));
  EXPECT_CALL(manager(), RemoveObserver(NotifierAsManagerObserver()));
  ClearNotifier();
}

TEST_F(HyperbolicDownloadItemNotifierTest,
    HyperbolicDownloadItemNotifierTest_1) {
  EXPECT_CALL(manager(), GetAllDownloads(_));
  SetNotifier();

  EXPECT_CALL(item(), AddObserver(NotifierAsItemObserver()));
  EXPECT_CALL(observer(), OnDownloadCreated(&manager(), &item()));
  NotifierAsManagerObserver()->OnDownloadCreated(
      &manager(), &item());

  EXPECT_CALL(manager(), RemoveObserver(NotifierAsManagerObserver()));
  NotifierAsManagerObserver()->ManagerGoingDown(&manager());

  EXPECT_CALL(item(), RemoveObserver(NotifierAsItemObserver()));
  NotifierAsItemObserver()->OnDownloadDestroyed(&item());

  ClearNotifier();
}
