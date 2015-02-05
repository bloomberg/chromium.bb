// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/signin/gaia_auth_extension_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"

namespace extensions {

namespace {

const char kTestData1[] = "A test string";
const char kTestData2[] = "Another test string";

}  // namespace

typedef InProcessBrowserTest GaiaAuthExtensionLoaderTest;

IN_PROC_BROWSER_TEST_F(GaiaAuthExtensionLoaderTest, AddAndGet) {

  GaiaAuthExtensionLoader* loader = GaiaAuthExtensionLoader::Get(
      browser()->profile());
  loader->LoadIfNeeded();

  int id1 = loader->AddData(kTestData1);
  int id2 = loader->AddData(kTestData2);
  EXPECT_NE(id1, id2);

  std::string fetched;
  EXPECT_TRUE(loader->GetData(id1, &fetched));
  EXPECT_EQ(kTestData1, fetched);

  EXPECT_TRUE(loader->GetData(id2, &fetched));
  EXPECT_EQ(kTestData2, fetched);

  const int kUnknownId = 1234;
  EXPECT_FALSE(loader->GetData(kUnknownId, &fetched));

  loader->UnloadIfNeeded();
}

IN_PROC_BROWSER_TEST_F(GaiaAuthExtensionLoaderTest, ClearDataOnUnload) {
  GaiaAuthExtensionLoader* loader = GaiaAuthExtensionLoader::Get(
      browser()->profile());
  loader->LoadIfNeeded();

  int id = loader->AddData(kTestData1);
  std::string fetched;
  EXPECT_TRUE(loader->GetData(id, &fetched));

  loader->UnloadIfNeeded();
  EXPECT_FALSE(loader->GetData(id, &fetched));
}

}  // namespace extensions
