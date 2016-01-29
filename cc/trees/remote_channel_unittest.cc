// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/layer_tree_test.h"

namespace cc {

class RemoteChannelTest : public LayerTreeTest {
 protected:
  RemoteChannelTest() : calls_received_(0) {}

  ~RemoteChannelTest() override {}

  void BeginTest() override {
    DCHECK(IsRemoteTest());
    BeginChannelTest();
  }
  virtual void BeginChannelTest() {}

  int calls_received_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RemoteChannelTest);
};

class RemoteChannelTestMainThreadStoppedFlinging : public RemoteChannelTest {
  void BeginChannelTest() override { proxy()->MainThreadHasStoppedFlinging(); }

  void MainThreadHasStoppedFlingingOnImpl() override {
    calls_received_++;
    EndTest();
  }

  void AfterTest() override { EXPECT_EQ(1, calls_received_); }
};

REMOTE_DIRECT_RENDERER_TEST_F(RemoteChannelTestMainThreadStoppedFlinging);

}  // namespace cc
