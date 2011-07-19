// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_sync_data.h"

#include "base/memory/scoped_ptr.h"
#include "base/version.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ExtensionSyncDataTest : public testing::Test {
};

bool ExtensionSyncDataEqual(const ExtensionSyncData& a,
                            const ExtensionSyncData& b) {
  return
      (a.id == b.id) &&
      (a.enabled == b.enabled) &&
      (a.incognito_enabled == b.incognito_enabled) &&
      (a.version.Equals(b.version)) &&
      (a.update_url == b.update_url) &&
      (a.name == b.name);
}

TEST_F(ExtensionSyncDataTest, MergeOlder) {
  ExtensionSyncData data;
  data.id = "id";
  data.enabled = true;
  data.incognito_enabled = false;
  {
    scoped_ptr<Version> version(Version::GetVersionFromString("1.2.0.0"));
    data.version = *version;
  }
  data.update_url = GURL("http://www.old.com");
  data.name = "data";

  ExtensionSyncData new_data;
  new_data.id = "id";
  new_data.enabled = false;
  new_data.incognito_enabled = true;
  {
    scoped_ptr<Version> version(Version::GetVersionFromString("1.1.0.0"));
    new_data.version = *version;
  }
  new_data.update_url = GURL("http://www.new.com");
  new_data.name = "new_data";

  ExtensionSyncData expected_data = data;
  expected_data.enabled = new_data.enabled;
  expected_data.incognito_enabled = new_data.incognito_enabled;

  data.Merge(new_data);
  EXPECT_TRUE(ExtensionSyncDataEqual(data, expected_data));
}

TEST_F(ExtensionSyncDataTest, MergeNewer) {
  ExtensionSyncData data;
  data.id = "id";
  data.enabled = true;
  data.incognito_enabled = false;
  {
    scoped_ptr<Version> version(Version::GetVersionFromString("1.2.0.0"));
    data.version = *version;
  }
  data.update_url = GURL("http://www.old.com");
  data.name = "data";

  ExtensionSyncData new_data;
  new_data.id = "id";
  new_data.enabled = false;
  new_data.incognito_enabled = true;
  {
    scoped_ptr<Version> version(Version::GetVersionFromString("1.3.0.0"));
    new_data.version = *version;
  }
  new_data.update_url = GURL("http://www.new.com");
  new_data.name = "new_data";

  data.Merge(new_data);
  EXPECT_TRUE(ExtensionSyncDataEqual(data, new_data));
}

}  // namespace
