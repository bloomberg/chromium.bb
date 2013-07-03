// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/pickle.h"
#include "chrome/browser/ui/views/extensions/browser_action_drag_data.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider_win.h"
#include "url/gurl.h"

namespace {

ui::OSExchangeData::Provider* CloneProvider(const ui::OSExchangeData& data) {
  return new ui::OSExchangeDataProviderWin(
      ui::OSExchangeDataProviderWin::GetIDataObject(data));
}

}  // namespace

typedef testing::Test BrowserActionDragDataTest;

TEST_F(BrowserActionDragDataTest, ArbitraryFormat) {
  TestingProfile profile;
  profile.SetID(L"id");

  ui::OSExchangeData data;
  data.SetURL(GURL("http://www.google.com"), L"Title");

  // We only support our format, so this should not succeed.
  BrowserActionDragData drag_data;
  EXPECT_FALSE(drag_data.Read(ui::OSExchangeData(CloneProvider(data))));
}

TEST_F(BrowserActionDragDataTest, BrowserActionDragDataFormat) {
  TestingProfile profile;
  Profile* profile_ptr = &profile;
  profile.SetID(L"id");

  const std::string extension_id = "42";
  Pickle pickle;
  pickle.WriteBytes(&profile_ptr, sizeof(&profile));
  pickle.WriteString(extension_id);
  pickle.WriteUInt64(42);

  ui::OSExchangeData data;
  data.SetPickledData(BrowserActionDragData::GetBrowserActionCustomFormat(),
                      pickle);

  BrowserActionDragData drag_data;
  EXPECT_TRUE(drag_data.Read(ui::OSExchangeData(CloneProvider(data))));
  ASSERT_TRUE(drag_data.IsFromProfile(profile.GetOriginalProfile()));
  ASSERT_STREQ(extension_id.c_str(), drag_data.id().c_str());
  ASSERT_EQ(42, drag_data.index());
}
