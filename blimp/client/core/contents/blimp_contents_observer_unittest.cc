// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/public/contents/blimp_contents_observer.h"

#include "base/memory/ptr_util.h"
#include "blimp/client/core/contents/blimp_contents_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const int kDummyTabId = 0;
}

namespace blimp {
namespace client {

namespace {

class BlimpContentsObserverTest : public BlimpContentsObserver {
 public:
  explicit BlimpContentsObserverTest(BlimpContents* blimp_contents)
      : BlimpContentsObserver(blimp_contents) {}

  MOCK_METHOD0(OnContentsDestroyed, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(BlimpContentsObserverTest);
};

TEST(BlimpContentsObserverUnittests, ObserverDies) {
  BlimpContentsImpl contents(kDummyTabId, nullptr);

  std::unique_ptr<BlimpContentsObserver> observer =
      base::MakeUnique<BlimpContentsObserverTest>(&contents);
  BlimpContentsObserver* observer_ptr = observer.get();
  EXPECT_TRUE(contents.HasObserver(observer_ptr));
  observer.reset();

  EXPECT_FALSE(contents.HasObserver(observer_ptr));
}

TEST(BlimpContentsObserverUnittests, ContentsDies) {
  std::unique_ptr<BlimpContentsObserverTest> observer;

  std::unique_ptr<BlimpContentsImpl> contents =
      base::MakeUnique<BlimpContentsImpl>(kDummyTabId, nullptr);
  observer.reset(new BlimpContentsObserverTest(contents.get()));
  EXPECT_CALL(*observer, OnContentsDestroyed()).Times(1);
  EXPECT_EQ(observer->blimp_contents(), contents.get());
  contents.reset();

  EXPECT_EQ(observer->blimp_contents(), nullptr);
}

}  // namespace

}  // namespace client
}  // namespace blimp
