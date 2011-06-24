// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_internals.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/media/media_internals_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class MockMediaInternalsObserver : public MediaInternalsObserver {
 public:
  MOCK_METHOD1(OnUpdate, void(const string16& javascript));
};

class MediaInternalsTest : public testing::Test {
 public:
  DictionaryValue* data() {
    return &internals_->data_;
  }

  void UpdateItem(const std::string& item, const std::string& property,
                  Value* value) {
    internals_->UpdateItemOnIOThread("", item, property, value);
  }

  void SendUpdate(const std::string& function, Value* value) {
    internals_->SendUpdateOnIOThread(function, value);
  }

 protected:
  virtual void SetUp() {
    internals_.reset(new MediaInternals());
  }
  scoped_ptr<MediaInternals> internals_;
};

TEST_F(MediaInternalsTest, UpdateAddsNewItem) {
  UpdateItem("some.item", "testing", Value::CreateBooleanValue(true));
  bool testing = false;
  std::string id;

  EXPECT_TRUE(data()->GetBoolean("some.item.testing", &testing));
  EXPECT_TRUE(testing);

  EXPECT_TRUE(data()->GetString("some.item.id", &id));
  EXPECT_EQ(id, "some.item");
}

TEST_F(MediaInternalsTest, UpdateModifiesExistingItem) {
  UpdateItem("some.item", "testing", Value::CreateBooleanValue(true));
  UpdateItem("some.item", "value", Value::CreateIntegerValue(5));
  UpdateItem("some.item", "testing", Value::CreateBooleanValue(false));
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
  MessageLoop message_loop(MessageLoop::TYPE_IO);
  scoped_ptr<MockMediaInternalsObserver> observer(
      new MockMediaInternalsObserver());

  internals_->AddUI(observer.get());
  SendUpdate("fn", data());

  EXPECT_CALL(*observer.get(), OnUpdate(testing::_)).Times(1);

  message_loop.RunAllPending();
}

TEST_F(MediaInternalsTest, RemovedObserversReceiveNoNotifications) {
  MessageLoop message_loop(MessageLoop::TYPE_IO);
  scoped_ptr<MockMediaInternalsObserver> observer(
      new MockMediaInternalsObserver());

  internals_->AddUI(observer.get());
  internals_->RemoveUI(observer.get());
  SendUpdate("fn", data());

  EXPECT_CALL(*observer.get(), OnUpdate(testing::_)).Times(0);

  message_loop.RunAllPending();
}
