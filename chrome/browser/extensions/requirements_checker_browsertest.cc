// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/requirements_checker.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "chrome/test/base/test_launcher_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/common/gpu_info.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gl/gl_switches.h"

namespace extensions {

class RequirementsCheckerBrowserTest : public ExtensionBrowserTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    // In linux, we need to launch GPU process to decide if WebGL is allowed.
    // Run it on top of osmesa to avoid bot driver issues.
#if defined(OS_LINUX)
    CHECK(test_launcher_utils::OverrideGLImplementation(
        command_line, gfx::kGLImplementationOSMesaName)) <<
        "kUseGL must not be set multiple times!";
#endif
  }

  scoped_refptr<const Extension> LoadExtensionFromDirName(
      const std::string& extension_dir_name) {
    base::FilePath extension_path;
    std::string load_error;
    PathService::Get(chrome::DIR_TEST_DATA, &extension_path);
    extension_path = extension_path.AppendASCII("requirements_checker")
                                   .AppendASCII(extension_dir_name);
    scoped_refptr<const Extension> extension =
        extension_file_util::LoadExtension(extension_path, Manifest::UNPACKED,
                                           0, &load_error);
    CHECK(load_error.length() == 0u);
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
    static const std::string json_blacklist =
      "{\n"
      "  \"name\": \"gpu blacklist\",\n"
      "  \"version\": \"1.0\",\n"
      "  \"entries\": [\n"
      "    {\n"
      "      \"id\": 1,\n"
      "      \"blacklist\": [\"" + JoinString(features, "\", \"") + "\"]\n"
      "    }\n"
      "  ]\n"
      "}";
    content::GPUInfo gpu_info;
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
  // npapi plugins are dissalowd on CROMEOS.
#if defined(OS_CHROMEOS)
  expected_errors.push_back(l10n_util::GetStringUTF8(
      IDS_EXTENSION_NPAPI_NOT_SUPPORTED));
#endif  // defined(OS_CHROMEOS)

  checker_.Check(extension, base::Bind(
      &RequirementsCheckerBrowserTest::ValidateRequirementErrors,
      base::Unretained(this), expected_errors));
  content::BrowserThread::GetBlockingPool()->FlushForTesting();
}

IN_PROC_BROWSER_TEST_F(RequirementsCheckerBrowserTest, DisallowCSS3D) {
  scoped_refptr<const Extension> extension(
      LoadExtensionFromDirName("require_3d"));
  ASSERT_TRUE(extension.get());


  // Blacklist css3d
  std::vector<std::string> blacklisted_features;
  blacklisted_features.push_back("accelerated_compositing");
  BlackListGPUFeatures(blacklisted_features);
  content::BrowserThread::GetBlockingPool()->FlushForTesting();

  std::vector<std::string> expected_errors;
  expected_errors.push_back(l10n_util::GetStringUTF8(
      IDS_EXTENSION_CSS3D_NOT_SUPPORTED));

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

IN_PROC_BROWSER_TEST_F(RequirementsCheckerBrowserTest, DisallowGPUFeatures) {
  scoped_refptr<const Extension> extension(
      LoadExtensionFromDirName("require_3d"));
  ASSERT_TRUE(extension.get());

  // Backlist both webgl and css3d
  std::vector<std::string> blacklisted_features;
  blacklisted_features.push_back("webgl");
  blacklisted_features.push_back("accelerated_compositing");
  BlackListGPUFeatures(blacklisted_features);
  content::BrowserThread::GetBlockingPool()->FlushForTesting();

  std::vector<std::string> expected_errors;
  expected_errors.push_back(l10n_util::GetStringUTF8(
      IDS_EXTENSION_WEBGL_NOT_SUPPORTED));
  expected_errors.push_back(l10n_util::GetStringUTF8(
      IDS_EXTENSION_CSS3D_NOT_SUPPORTED));

  checker_.Check(extension, base::Bind(
      &RequirementsCheckerBrowserTest::ValidateRequirementErrors,
      base::Unretained(this), expected_errors));
  content::BrowserThread::GetBlockingPool()->FlushForTesting();
}

IN_PROC_BROWSER_TEST_F(RequirementsCheckerBrowserTest, Check3DExtension) {
  scoped_refptr<const Extension> extension(
      LoadExtensionFromDirName("require_3d"));
  ASSERT_TRUE(extension.get());

  checker_.Check(extension, base::Bind(
      &RequirementsCheckerBrowserTest::ValidateRequirementErrors,
      base::Unretained(this), std::vector<std::string>()));
  content::BrowserThread::GetBlockingPool()->FlushForTesting();
}

}  // extensions
