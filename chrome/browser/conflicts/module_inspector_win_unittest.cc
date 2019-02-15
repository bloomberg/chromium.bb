// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/conflicts/module_inspector_win.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

base::FilePath GetKernel32DllFilePath() {
  std::unique_ptr<base::Environment> env = base::Environment::Create();
  std::string sysroot;
  EXPECT_TRUE(env->GetVar("SYSTEMROOT", &sysroot));

  base::FilePath path =
      base::FilePath::FromUTF8Unsafe(sysroot).Append(L"system32\\kernel32.dll");

  return path;
}

class ModuleInspectorTest : public testing::Test {
 public:
  ModuleInspectorTest() = default;

  // Callback for ModuleInspector.
  void OnModuleInspected(const ModuleInfoKey& module_key,
                         ModuleInspectionResult inspection_result) {
    inspected_modules_.push_back(std::move(inspection_result));
  }

  void RunUntilIdle() { test_browser_thread_bundle_.RunUntilIdle(); }

  const std::vector<ModuleInspectionResult>& inspected_modules() {
    return inspected_modules_;
  }

  // A TestBrowserThreadBundle is required instead of a ScopedTaskEnvironment
  // because of AfterStartupTaskUtils (DCHECK for BrowserThread::UI).
  //
  // Must be before the ModuleInspector.
  content::TestBrowserThreadBundle test_browser_thread_bundle_;

 private:
  std::vector<ModuleInspectionResult> inspected_modules_;

  DISALLOW_COPY_AND_ASSIGN(ModuleInspectorTest);
};

}  // namespace

TEST_F(ModuleInspectorTest, OneModule) {
  ModuleInspector module_inspector(base::Bind(
      &ModuleInspectorTest::OnModuleInspected, base::Unretained(this)));

  module_inspector.AddModule({GetKernel32DllFilePath(), 0, 0});

  RunUntilIdle();

  ASSERT_EQ(1u, inspected_modules().size());
}

TEST_F(ModuleInspectorTest, MultipleModules) {
  ModuleInfoKey kTestCases[] = {
      {base::FilePath(), 0, 0}, {base::FilePath(), 0, 0},
      {base::FilePath(), 0, 0}, {base::FilePath(), 0, 0},
      {base::FilePath(), 0, 0},
  };

  ModuleInspector module_inspector(base::Bind(
      &ModuleInspectorTest::OnModuleInspected, base::Unretained(this)));

  for (const auto& module : kTestCases)
    module_inspector.AddModule(module);

  RunUntilIdle();

  EXPECT_EQ(5u, inspected_modules().size());
}

TEST_F(ModuleInspectorTest, DisableBackgroundInspection) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      ModuleInspector::kEnableBackgroundModuleInspection);

  ModuleInfoKey kTestCases[] = {
      {base::FilePath(), 0, 0},
      {base::FilePath(), 0, 0},
  };

  ModuleInspector module_inspector(base::Bind(
      &ModuleInspectorTest::OnModuleInspected, base::Unretained(this)));

  for (const auto& module : kTestCases)
    module_inspector.AddModule(module);

  RunUntilIdle();

  // No inspected modules yet.
  EXPECT_EQ(0u, inspected_modules().size());

  // Increasing inspection priority will start the background inspection
  // process.
  module_inspector.IncreaseInspectionPriority();
  RunUntilIdle();

  EXPECT_EQ(2u, inspected_modules().size());
}
