// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/host_zoom_map.h"

#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "content/public/browser/browser_thread.h"
#include "content/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

class HostZoomMapTest : public testing::Test {
 public:
  HostZoomMapTest() : ui_thread_(content::BrowserThread::UI, &message_loop_) {
  }

 protected:
  MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
};

TEST_F(HostZoomMapTest, GetSetZoomLevel) {
  scoped_refptr<HostZoomMap> host_zoom_map = new HostZoomMap;

  double zoomed = 2.5;
  host_zoom_map->SetZoomLevel("zoomed.com", zoomed);

  EXPECT_DOUBLE_EQ(host_zoom_map->GetZoomLevel("normal.com"), 0);
  EXPECT_DOUBLE_EQ(host_zoom_map->GetZoomLevel("zoomed.com"), zoomed);
}

