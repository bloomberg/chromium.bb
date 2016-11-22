// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/browsertest_util.h"

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/process_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/extensions/updater/local_extension_cache.h"
#include "chromeos/chromeos_paths.h"
#endif

namespace extensions {
namespace browsertest_util {

std::string ExecuteScriptInBackgroundPage(Profile* profile,
                                          const std::string& extension_id,
                                          const std::string& script) {
  extensions::ProcessManager* manager =
      extensions::ProcessManager::Get(profile);
  extensions::ExtensionHost* host =
      manager->GetBackgroundHostForExtension(extension_id);
  if (host == NULL) {
    ADD_FAILURE() << "Extension " << extension_id << " has no background page.";
    return "";
  }
  std::string result;
  if (!content::ExecuteScriptAndExtractString(
           host->host_contents(), script, &result)) {
    ADD_FAILURE() << "Executing script failed: " << script;
    result.clear();
  }
  return result;
}

bool ExecuteScriptInBackgroundPageNoWait(Profile* profile,
                                         const std::string& extension_id,
                                         const std::string& script) {
  extensions::ProcessManager* manager =
      extensions::ProcessManager::Get(profile);
  extensions::ExtensionHost* host =
      manager->GetBackgroundHostForExtension(extension_id);
  if (host == NULL) {
    ADD_FAILURE() << "Extension " << extension_id << " has no background page.";
    return false;
  }
  return content::ExecuteScript(host->host_contents(), script);
}

void CreateAndInitializeLocalCache() {
#if defined(OS_CHROMEOS)
  base::FilePath extension_cache_dir;
  CHECK(PathService::Get(chromeos::DIR_DEVICE_EXTENSION_LOCAL_CACHE,
                         &extension_cache_dir));
  base::FilePath cache_init_file = extension_cache_dir.Append(
      extensions::LocalExtensionCache::kCacheReadyFlagFileName);
  EXPECT_EQ(base::WriteFile(cache_init_file, "", 0), 0);
#endif
}

}  // namespace browsertest_util
}  // namespace extensions
