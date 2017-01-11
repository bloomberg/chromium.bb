// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/test/components_test_suite.h"

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/statistics_recorder.h"
#include "base/path_service.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "url/url_util.h"

#if !defined(OS_IOS)
#include "content/public/common/content_client.h"
#include "content/public/test/content_test_suite_base.h"
#include "content/public/test/test_content_client_initializer.h"
#include "content/public/test/unittest_test_suite.h"
#include "ui/gl/test/gl_surface_test_support.h"
#endif

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "components/gcm_driver/instance_id/android/component_jni_registrar.h"
#include "components/invalidation/impl/android/component_jni_registrar.h"
#include "components/policy/core/browser/android/component_jni_registrar.h"
#include "components/safe_json/android/component_jni_registrar.h"
#include "components/signin/core/browser/android/component_jni_registrar.h"
#include "content/public/test/test_utils.h"
#include "net/android/net_jni_registrar.h"
#include "ui/base/android/ui_base_jni_registrar.h"
#include "ui/gfx/android/gfx_jni_registrar.h"
#endif

namespace {

class ComponentsTestSuite : public base::TestSuite {
 public:
  ComponentsTestSuite(int argc, char** argv) : base::TestSuite(argc, argv) {}

 private:
  void Initialize() override {
    base::TestSuite::Initialize();

    // Initialize the histograms subsystem, so that any histograms hit in tests
    // are correctly registered with the statistics recorder and can be queried
    // by tests.
    base::StatisticsRecorder::Initialize();

#if !defined(OS_IOS)
    gl::GLSurfaceTestSupport::InitializeOneOff();

    // Setup content scheme statics.
    {
      content::ContentClient content_client;
      content::ContentTestSuiteBase::RegisterContentSchemes(&content_client);
    }
#endif
#if defined(OS_ANDROID)
    // Register JNI bindings for android.
    JNIEnv* env = base::android::AttachCurrentThread();
    ASSERT_TRUE(content::RegisterJniForTesting(env));
    ASSERT_TRUE(gfx::android::RegisterJni(env));
    ASSERT_TRUE(instance_id::android::RegisterInstanceIDJni(env));
    ASSERT_TRUE(invalidation::android::RegisterInvalidationJni(env));
    ASSERT_TRUE(policy::android::RegisterPolicy(env));
    ASSERT_TRUE(safe_json::android::RegisterSafeJsonJni(env));
    ASSERT_TRUE(signin::android::RegisterSigninJni(env));
    ASSERT_TRUE(net::android::RegisterJni(env));
    ASSERT_TRUE(ui::android::RegisterJni(env));
#endif

    ui::RegisterPathProvider();

    base::FilePath pak_path;
#if defined(OS_ANDROID)
    PathService::Get(ui::DIR_RESOURCE_PAKS_ANDROID, &pak_path);
#else
    PathService::Get(base::DIR_MODULE, &pak_path);
#endif

    base::FilePath ui_test_pak_path;
    ASSERT_TRUE(PathService::Get(ui::UI_TEST_PAK, &ui_test_pak_path));
    ui::ResourceBundle::InitSharedInstanceWithPakPath(ui_test_pak_path);

    ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
        pak_path.AppendASCII("components_tests_resources.pak"),
        ui::SCALE_FACTOR_NONE);

    // These schemes need to be added globally to pass tests of
    // autocomplete_input_unittest.cc and content_settings_pattern*
    url::AddStandardScheme("chrome", url::SCHEME_WITHOUT_PORT);
    url::AddStandardScheme("chrome-extension", url::SCHEME_WITHOUT_PORT);
    url::AddStandardScheme("chrome-devtools", url::SCHEME_WITHOUT_PORT);
    url::AddStandardScheme("chrome-search", url::SCHEME_WITHOUT_PORT);

    // Not using kExtensionScheme to avoid the dependency to extensions.
    ContentSettingsPattern::SetNonWildcardDomainNonPortScheme(
        "chrome-extension");
  }

  void Shutdown() override {
    ui::ResourceBundle::CleanupSharedInstance();

    base::TestSuite::Shutdown();
  }

  DISALLOW_COPY_AND_ASSIGN(ComponentsTestSuite);
};

class ComponentsUnitTestEventListener : public testing::EmptyTestEventListener {
 public:
  ComponentsUnitTestEventListener() {}
  ~ComponentsUnitTestEventListener() override {}

  void OnTestStart(const testing::TestInfo& test_info) override {
#if !defined(OS_IOS)
    content_initializer_.reset(new content::TestContentClientInitializer());
#endif
  }

  void OnTestEnd(const testing::TestInfo& test_info) override {
#if !defined(OS_IOS)
    content_initializer_.reset();
#endif
  }

 private:
#if !defined(OS_IOS)
  std::unique_ptr<content::TestContentClientInitializer> content_initializer_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ComponentsUnitTestEventListener);
};

}  // namespace

base::RunTestSuiteCallback GetLaunchCallback(int argc, char** argv) {
#if !defined(OS_IOS)
  auto test_suite = base::MakeUnique<content::UnitTestTestSuite>(
      new ComponentsTestSuite(argc, argv));
#else
  auto test_suite = base::MakeUnique<ComponentsTestSuite>(argc, argv);
#endif

  // The listener will set up common test environment for all components unit
  // tests.
  testing::TestEventListeners& listeners =
      testing::UnitTest::GetInstance()->listeners();
  listeners.Append(new ComponentsUnitTestEventListener());

#if !defined(OS_IOS)
  return base::Bind(&content::UnitTestTestSuite::Run, std::move(test_suite));
#else
  return base::Bind(&base::TestSuite::Run, std::move(test_suite));
#endif
}
