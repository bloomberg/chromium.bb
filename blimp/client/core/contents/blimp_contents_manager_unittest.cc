// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/contents/blimp_contents_manager.h"

#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "blimp/client/core/contents/blimp_contents_impl.h"
#include "blimp/client/core/contents/tab_control_feature.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace {
const int kDummyTabId = 0;
}

namespace blimp {
namespace client {
namespace {

class MockTabControlFeature : public TabControlFeature {
 public:
  MockTabControlFeature() {}
  ~MockTabControlFeature() override = default;

  MOCK_METHOD1(CreateTab, void(int));
  MOCK_METHOD1(CloseTab, void(int));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockTabControlFeature);
};

TEST(BlimpContentsManagerUnittest, GetExistingBlimpContents) {
  base::MessageLoop loop;
  MockTabControlFeature tab_control_feature;

  BlimpContentsManager blimp_contents_manager(nullptr, nullptr,
                                              &tab_control_feature);

  EXPECT_CALL(tab_control_feature, CreateTab(_)).Times(1);
  std::unique_ptr<BlimpContentsImpl> blimp_contents =
      blimp_contents_manager.CreateBlimpContents();
  int id = blimp_contents->id();
  BlimpContentsImpl* existing_contents =
      blimp_contents_manager.GetBlimpContents(id);
  EXPECT_EQ(blimp_contents.get(), existing_contents);
}

TEST(BlimpContentsManagerUnittest, GetNonExistingBlimpContents) {
  MockTabControlFeature tab_control_feature;

  BlimpContentsManager blimp_contents_manager(nullptr, nullptr,
                                              &tab_control_feature);

  BlimpContentsImpl* existing_contents =
      blimp_contents_manager.GetBlimpContents(kDummyTabId);
  EXPECT_EQ(nullptr, existing_contents);
}

TEST(BlimpContentsManagerUnittest, GetDestroyedBlimpContents) {
  base::MessageLoop loop;
  MockTabControlFeature tab_control_feature;
  BlimpContentsManager blimp_contents_manager(nullptr, nullptr,
                                              &tab_control_feature);
  int id;

  EXPECT_CALL(tab_control_feature, CreateTab(_)).Times(1);
  std::unique_ptr<BlimpContentsImpl> blimp_contents =
      blimp_contents_manager.CreateBlimpContents();
  id = blimp_contents.get()->id();
  BlimpContentsImpl* existing_contents =
      blimp_contents_manager.GetBlimpContents(id);
  EXPECT_EQ(blimp_contents.get(), existing_contents);

  EXPECT_CALL(tab_control_feature, CloseTab(id)).Times(1);
  blimp_contents.reset();

  loop.RunUntilIdle();
  EXPECT_EQ(nullptr, blimp_contents_manager.GetBlimpContents(id));
}

// TODO(mlliu): remove this test case (http://crbug.com/642558)
TEST(BlimpContentsManagerUnittest, CreateTwoBlimpContentsDestroyAndCreate) {
  base::MessageLoop loop;
  MockTabControlFeature tab_control_feature;
  BlimpContentsManager blimp_contents_manager(nullptr, nullptr,
                                              &tab_control_feature);

  EXPECT_CALL(tab_control_feature, CreateTab(_)).Times(2);
  std::unique_ptr<BlimpContentsImpl> blimp_contents =
      blimp_contents_manager.CreateBlimpContents();
  EXPECT_NE(blimp_contents, nullptr);

  std::unique_ptr<BlimpContentsImpl> second_blimp_contents =
      blimp_contents_manager.CreateBlimpContents();
  EXPECT_EQ(second_blimp_contents, nullptr);

  blimp_contents.reset();
  std::unique_ptr<BlimpContentsImpl> third_blimp_contents =
      blimp_contents_manager.CreateBlimpContents();
  EXPECT_NE(third_blimp_contents, nullptr);
}

}  // namespace
}  // namespace client
}  // namespace blimp
