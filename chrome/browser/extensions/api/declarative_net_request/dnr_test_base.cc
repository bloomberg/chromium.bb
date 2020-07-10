// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_net_request/dnr_test_base.h"

#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "components/version_info/version_info.h"

namespace extensions {
namespace declarative_net_request {

// Use channel UNKNOWN to ensure that the declarativeNetRequest API is
// available, irrespective of its actual availability.
DNRTestBase::DNRTestBase() : channel_(::version_info::Channel::UNKNOWN) {}

void DNRTestBase::SetUp() {
  ExtensionServiceTestBase::SetUp();
  InitializeEmptyExtensionService();
}

std::unique_ptr<ChromeTestExtensionLoader>
DNRTestBase::CreateExtensionLoader() {
  auto loader = std::make_unique<ChromeTestExtensionLoader>(browser_context());
  switch (GetParam()) {
    case ExtensionLoadType::PACKED:
      loader->set_pack_extension(true);

      // CrxInstaller reloads the extension after moving it, which causes an
      // install warning for packed extensions due to the presence of
      // kMetadata folder. However, this isn't actually surfaced to the user.
      loader->set_ignore_manifest_warnings(true);
      break;
    case ExtensionLoadType::UNPACKED:
      loader->set_pack_extension(false);
      break;
  }
  return loader;
}

}  // namespace declarative_net_request
}  // namespace  extensions
