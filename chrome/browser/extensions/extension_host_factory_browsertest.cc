// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_host_factory.h"

#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/test/base/ui_test_utils.h"
#include "extensions/common/view_type.h"

namespace extensions {

typedef ExtensionBrowserTest ExtensionHostFactoryTest;

// Tests that ExtensionHosts are created with the correct type and profiles.
IN_PROC_BROWSER_TEST_F(ExtensionHostFactoryTest, CreateExtensionHosts) {
  // Load a very simple extension with just a background page.
  scoped_refptr<const Extension> extension =
      LoadExtension(test_data_dir_.AppendASCII("api_test")
                        .AppendASCII("browser_action")
                        .AppendASCII("none"));
  ASSERT_TRUE(extension.get());

  {
    // Popup hosts are created with the correct type and profile.
    scoped_ptr<ExtensionHost> host(
        ExtensionHostFactory::CreatePopupHost(extension->url(), browser()));
    EXPECT_EQ(extension.get(), host->extension());
    EXPECT_EQ(browser()->profile(), host->profile());
    EXPECT_EQ(VIEW_TYPE_EXTENSION_POPUP, host->extension_host_type());
  }

  {
    // Infobar hosts are created with the correct type and profile.
    scoped_ptr<ExtensionHost> host(
        ExtensionHostFactory::CreateInfobarHost(extension->url(), browser()));
    EXPECT_EQ(extension.get(), host->extension());
    EXPECT_EQ(browser()->profile(), host->profile());
    EXPECT_EQ(VIEW_TYPE_EXTENSION_INFOBAR, host->extension_host_type());
  }

  {
    // Dialog hosts are created with the correct type and profile.
    scoped_ptr<ExtensionHost> host(ExtensionHostFactory::CreateDialogHost(
        extension->url(), browser()->profile()));
    EXPECT_EQ(extension.get(), host->extension());
    EXPECT_EQ(browser()->profile(), host->profile());
    EXPECT_EQ(VIEW_TYPE_EXTENSION_DIALOG, host->extension_host_type());
  }
}

// Tests that extensions loaded in incognito mode have the correct profiles
// for split-mode and non-split-mode.
IN_PROC_BROWSER_TEST_F(ExtensionHostFactoryTest, IncognitoExtensionHosts) {
  // Open an incognito browser.
  Browser* incognito_browser = ui_test_utils::OpenURLOffTheRecord(
      browser()->profile(), GURL("about:blank"));

  // Load a non-split-mode extension, enabled in incognito.
  scoped_refptr<const Extension> regular_extension =
      LoadExtensionIncognito(test_data_dir_.AppendASCII("api_test")
                                 .AppendASCII("browser_action")
                                 .AppendASCII("none"));
  ASSERT_TRUE(regular_extension.get());

  // The ExtensionHost for a regular extension in an incognito window is
  // associated with the original window's profile.
  scoped_ptr<ExtensionHost> regular_host(
      ExtensionHostFactory::CreatePopupHost(
            regular_extension->url(), incognito_browser));
  EXPECT_EQ(browser()->profile(), regular_host->profile());

  // Load a split-mode incognito extension.
  scoped_refptr<const Extension> split_mode_extension =
      LoadExtensionIncognito(test_data_dir_.AppendASCII("api_test")
                                 .AppendASCII("browser_action")
                                 .AppendASCII("split_mode"));
  ASSERT_TRUE(split_mode_extension.get());

  // The ExtensionHost for a split-mode extension is associated with the
  // incognito profile.
  scoped_ptr<ExtensionHost> split_mode_host(
      ExtensionHostFactory::CreatePopupHost(
          split_mode_extension->url(), incognito_browser));
  EXPECT_EQ(incognito_browser->profile(), split_mode_host->profile());
}

}  // namespace extensions
