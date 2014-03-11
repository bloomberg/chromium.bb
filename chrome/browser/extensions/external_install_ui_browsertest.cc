// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/external_install_ui.h"

namespace extensions {

// This test will fail by crashing in URLRequestContext::AssertNoURLRequests().
// If this happens, please do not disable the test, as it is an indication that
// we are not cleaning up our URLRequests properly, and it should be treated as
// a failure.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest,
                       ExternalInstallUIDoesntLeakURLRequestsOnQuickShutdown) {
  // Load an extension which updates from the gallery.
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("update_from_webstore"));
  ASSERT_TRUE(extension);

  // Add an ExternalInstallError for the extension. This will cause the
  // ExternalInstallUI to launch a WebstoreDataFetcher to get the extension's
  // metadata.
  AddExternalInstallError(ExtensionSystem::Get(profile())->extension_service(),
                          extension,
                          false);  // Not new profile.

  // Once the test ends, the profile will be shut down, which also destroys the
  // URLRequestContext for the ExternalInstallUI (since it is owned by the
  // profile).
  // If the ExternalInstallUI doesn't clean up after itself, this will result in
  // leaked URL requests, then causing a CHECK(false) in
  // URLRequestContext::AssertNoURLRequests(), which is called on destruction.
  // If the ExternalInstallUI cleans up properly, this won't be a problem.
  //
  // Unfortunately, that does mean that this is a "Pass or Crash" test. But
  // I can't see a good way around that right now. So if this test starts
  // crashing, it should most likely be treated as a failure.
}

}  // namespace extensions
