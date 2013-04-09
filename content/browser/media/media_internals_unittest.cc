// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_internals.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

class MockObserverBaseClass {
 public:
  ~MockObserverBaseClass() {}
  virtual void OnUpdate(const string16& javascript) = 0;
};

class MockMediaInternalsObserver : public MockObserverBaseClass {
 public:
  virtual ~MockMediaInternalsObserver() {}
  MOCK_METHOD1(OnUpdate, void(const string16& javascript));
};

}  // namespace

class MediaInternalsTest : public testing::Test {
 public:
  MediaInternalsTest() : io_thread_(BrowserThread::IO, &loop_) {}
  base::DictionaryValue* data() {
    return &internals_->data_;
  }

  void DeleteItem(const std::string& item) {
    internals_->DeleteItem(item);
  }

  void UpdateItem(const std::string& item, const std::string& property,
                  base::Value* value) {
    internals_->UpdateItem("", item, property, value);
  }

  void SendUpdate(const std::string& function, base::Value* value) {
    internals_->SendUpdate(function, value);
  }

 protected:
  virtual void SetUp() {
    internals_.reset(new MediaInternals());
  }

  MessageLoop loop_;
  TestBrowserThread io_thread_;
  scoped_ptr<MediaInternals> internals_;
};

TEST_F(MediaInternalsTest, UpdateAddsNewItem) {
  UpdateItem("some.item", "testing", new base::FundamentalValue(true));
  bool testing = false;
  std::string id;

  EXPECT_TRUE(data()->GetBoolean("some.item.testing", &testing));
  EXPECT_TRUE(testing);

  EXPECT_TRUE(data()->GetString("some.item.id", &id));
  EXPECT_EQ(id, "some.item");
}

TEST_F(MediaInternalsTest, UpdateModifiesExistingItem) {
  UpdateItem("some.item", "testing", new base::FundamentalValue(true));
  UpdateItem("some.item", "value", new base::FundamentalValue(5));
  UpdateItem("some.item", "testing", new base::FundamentalValue(false));
  bool testing = true;
  int value = 0;
  std::string id;

  EXPECT_TRUE(data()->GetBoolean("some.item.testing", &testing));
  EXPECT_FALSE(testing);

  EXPECT_TRUE(data()->GetInteger("some.item.value", &value));
  EXPECT_EQ(value, 5);

  EXPECT_TRUE(data()->GetString("some.item.id", &id));
  EXPECT_EQ(id, "some.item");
}

TEST_F(MediaInternalsTest, ObserversReceiveNotifications) {
  scoped_ptr<MockMediaInternalsObserver> observer(
      new MockMediaInternalsObserver());

  EXPECT_CALL(*observer.get(), OnUpdate(testing::_)).Times(1);

  MediaInternals::UpdateCallback callback = base::Bind(
      &MockMediaInternalsObserver::OnUpdate, base::Unretained(observer.get()));

  internals_->AddUpdateCallback(callback);
  SendUpdate("fn", data());
}

TEST_F(MediaInternalsTest, RemovedObserversReceiveNoNotifications) {
  scoped_ptr<MockMediaInternalsObserver> observer(
      new MockMediaInternalsObserver());

  EXPECT_CALL(*observer.get(), OnUpdate(testing::_)).Times(0);

  MediaInternals::UpdateCallback callback = base::Bind(
      &MockMediaInternalsObserver::OnUpdate, base::Unretained(observer.get()));

  internals_->AddUpdateCallback(callback);
  internals_->RemoveUpdateCallback(callback);
  SendUpdate("fn", data());
}

TEST_F(MediaInternalsTest, DeleteRemovesItem) {
  base::Value* out;

  UpdateItem("some.item", "testing", base::Value::CreateNullValue());
  EXPECT_TRUE(data()->Get("some.item", &out));
  EXPECT_TRUE(data()->Get("some", &out));

  DeleteItem("some.item");
  EXPECT_FALSE(data()->Get("some.item", &out));
  EXPECT_TRUE(data()->Get("some", &out));

  DeleteItem("some");
  EXPECT_FALSE(data()->Get("some.item", &out));
  EXPECT_FALSE(data()->Get("some", &out));
}

}  // namespace content
