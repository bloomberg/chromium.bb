// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/requirements_checker.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/gpu_data_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/file_util.h"
#include "gpu/config/gpu_info.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

class RequirementsCheckerBrowserTest : public ExtensionBrowserTest {
 public:
  scoped_refptr<const Extension> LoadExtensionFromDirName(
      const std::string& extension_dir_name) {
    base::FilePath extension_path;
    std::string load_error;
    PathService::Get(chrome::DIR_TEST_DATA, &extension_path);
    extension_path = extension_path.AppendASCII("requirements_checker")
                                   .AppendASCII(extension_dir_name);
    scoped_refptr<const Extension> extension = file_util::LoadExtension(
        extension_path, Manifest::UNPACKED, 0, &load_error);
    CHECK_EQ(0U, load_error.length());
    return extension;
  }

  void ValidateRequirementErrors(std::vector<std::string> expected_errors,
                                 std::vector<std::string> actual_errors) {
    ASSERT_EQ(expected_errors, actual_errors);
    requirement_errors_.swap(actual_errors);
  }

  // This should only be called once per test instance. Calling more than once
  // will result in stale information in the GPUDataManager which will throw off
  // the RequirementsChecker.
  void BlackListGPUFeatures(const std::vector<std::string>& features) {
#if !defined(NDEBUG)
    static bool called = false;
    DCHECK(!called);
    called = true;
#endif

    static const std::string json_blacklist =
      "{\n"
      "  \"name\": \"gpu blacklist\",\n"
      "  \"version\": \"1.0\",\n"
      "  \"entries\": [\n"
      "    {\n"
      "      \"id\": 1,\n"
      "      \"features\": [\"" + JoinString(features, "\", \"") + "\"]\n"
      "    }\n"
      "  ]\n"
      "}";
    gpu::GPUInfo gpu_info;
    content::GpuDataManager::GetInstance()->InitializeForTesting(
        json_blacklist, gpu_info);
  }

 protected:
  std::vector<std::string> requirement_errors_;
  RequirementsChecker checker_;
};

IN_PROC_BROWSER_TEST_F(RequirementsCheckerBrowserTest, CheckEmptyExtension) {
  scoped_refptr<const Extension> extension(
      LoadExtensionFromDirName("no_requirements"));
  ASSERT_TRUE(extension.get());
  checker_.Check(extension, base::Bind(
      &RequirementsCheckerBrowserTest::ValidateRequirementErrors,
      base::Unretained(this), std::vector<std::string>()));
  content::BrowserThread::GetBlockingPool()->FlushForTesting();
}

IN_PROC_BROWSER_TEST_F(RequirementsCheckerBrowserTest, CheckNpapiExtension) {
  scoped_refptr<const Extension> extension(
      LoadExtensionFromDirName("require_npapi"));
  ASSERT_TRUE(extension.get());

  std::vector<std::string> expected_errors;
#if defined(OS_POSIX) && !defined(OS_MACOSX)
  expected_errors.push_back(l10n_util::GetStringUTF8(
      IDS_EXTENSION_NPAPI_NOT_SUPPORTED));
#endif

  checker_.Check(extension, base::Bind(
      &RequirementsCheckerBrowserTest::ValidateRequirementErrors,
      base::Unretained(this), expected_errors));
  content::BrowserThread::GetBlockingPool()->FlushForTesting();
}

IN_PROC_BROWSER_TEST_F(RequirementsCheckerBrowserTest,
                       CheckWindowShapeExtension) {
  scoped_refptr<const Extension> extension(
      LoadExtensionFromDirName("require_window_shape"));
  ASSERT_TRUE(extension.get());

  std::vector<std::string> expected_errors;
#if !defined(USE_AURA)
  expected_errors.push_back(l10n_util::GetStringUTF8(
      IDS_EXTENSION_WINDOW_SHAPE_NOT_SUPPORTED));
#endif  // !defined(USE_AURA)

  checker_.Check(extension, base::Bind(
      &RequirementsCheckerBrowserTest::ValidateRequirementErrors,
      base::Unretained(this), expected_errors));
  content::BrowserThread::GetBlockingPool()->FlushForTesting();
}

IN_PROC_BROWSER_TEST_F(RequirementsCheckerBrowserTest, DisallowWebGL) {
  scoped_refptr<const Extension> extension(
      LoadExtensionFromDirName("require_3d"));
  ASSERT_TRUE(extension.get());

  // Backlist webgl
  std::vector<std::string> blacklisted_features;
  blacklisted_features.push_back("webgl");
  BlackListGPUFeatures(blacklisted_features);
  content::BrowserThread::GetBlockingPool()->FlushForTesting();

  std::vector<std::string> expected_errors;
  expected_errors.push_back(l10n_util::GetStringUTF8(
      IDS_EXTENSION_WEBGL_NOT_SUPPORTED));

  checker_.Check(extension, base::Bind(
      &RequirementsCheckerBrowserTest::ValidateRequirementErrors,
      base::Unretained(this), expected_errors));
  content::BrowserThread::GetBlockingPool()->FlushForTesting();
}

IN_PROC_BROWSER_TEST_F(RequirementsCheckerBrowserTest, Check3DExtension) {
  scoped_refptr<const Extension> extension(
      LoadExtensionFromDirName("require_3d"));
  ASSERT_TRUE(extension.get());

  std::vector<std::string> expected_errors;

  if (!content::GpuDataManager::GetInstance()->GpuAccessAllowed(NULL)) {
    expected_errors.push_back(l10n_util::GetStringUTF8(
        IDS_EXTENSION_WEBGL_NOT_SUPPORTED));
  }

  checker_.Check(extension, base::Bind(
      &RequirementsCheckerBrowserTest::ValidateRequirementErrors,
      base::Unretained(this), expected_errors));
  content::BrowserThread::GetBlockingPool()->FlushForTesting();
}

}  // namespace extensions
