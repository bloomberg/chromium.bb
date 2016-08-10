// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/contents/blimp_contents_manager.h"

#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "blimp/client/core/contents/blimp_contents_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const int kDummyTabId = 0;
}

namespace blimp {
namespace client {
namespace {

TEST(BlimpContentsManagerUnittest, GetExistingBlimpContents) {
  base::MessageLoop loop;
  BlimpContentsManager blimp_contents_manager;

  std::unique_ptr<BlimpContentsImpl> blimp_contents =
      blimp_contents_manager.CreateBlimpContents();
  int id = blimp_contents->id();
  BlimpContentsImpl* existing_contents =
      blimp_contents_manager.GetBlimpContents(id);
  EXPECT_EQ(blimp_contents.get(), existing_contents);
}

TEST(BlimpContentsManagerUnittest, GetNonExistingBlimpContents) {
  BlimpContentsManager blimp_contents_manager;

  BlimpContentsImpl* existing_contents =
      blimp_contents_manager.GetBlimpContents(kDummyTabId);
  EXPECT_EQ(nullptr, existing_contents);
}

TEST(BlimpContentsManagerUnittest, GetDestroyedBlimpContents) {
  base::MessageLoop loop;
  BlimpContentsManager blimp_contents_manager;
  int id;

  std::unique_ptr<BlimpContentsImpl> blimp_contents =
      blimp_contents_manager.CreateBlimpContents();
  id = blimp_contents.get()->id();
  BlimpContentsImpl* existing_contents =
      blimp_contents_manager.GetBlimpContents(id);
  EXPECT_EQ(blimp_contents.get(), existing_contents);
  blimp_contents.reset();

  loop.RunUntilIdle();
  EXPECT_EQ(nullptr, blimp_contents_manager.GetBlimpContents(id));
}

}  // namespace
}  // namespace client
}  // namespace blimp
