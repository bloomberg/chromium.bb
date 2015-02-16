// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/statistics_recorder.h"
#include "base/path_service.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "content/public/test/test_content_client_initializer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "url/url_util.h"

#if !defined(OS_IOS)
#include "ui/gl/gl_surface.h"
#endif

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "components/invalidation/android/component_jni_registrar.h"
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
    gfx::GLSurface::InitializeOneOffForTests();
#endif
#if defined(OS_ANDROID)
    // Register JNI bindings for android.
    JNIEnv* env = base::android::AttachCurrentThread();
    gfx::android::RegisterJni(env);
    ui::android::RegisterJni(env);
    invalidation::android::RegisterInvalidationJni(env);
#endif

    ui::RegisterPathProvider();

    base::FilePath pak_path;
#if defined(OS_ANDROID)
    PathService::Get(ui::DIR_RESOURCE_PAKS_ANDROID, &pak_path);
#else
    PathService::Get(base::DIR_MODULE, &pak_path);
#endif
    ui::ResourceBundle::InitSharedInstanceWithPakPath(
        pak_path.AppendASCII("components_tests_resources.pak"));

    // These schemes need to be added globally to pass tests of
    // autocomplete_input_unittest.cc and content_settings_pattern*
    url::AddStandardScheme("chrome");
    url::AddStandardScheme("chrome-extension");
    url::AddStandardScheme("chrome-devtools");
    url::AddStandardScheme("chrome-search");

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
  virtual ~ComponentsUnitTestEventListener() {}

  virtual void OnTestStart(const testing::TestInfo& test_info) override {
    content_initializer_.reset(new content::TestContentClientInitializer());
  }

  virtual void OnTestEnd(const testing::TestInfo& test_info) override {
    content_initializer_.reset();
  }

 private:
  scoped_ptr<content::TestContentClientInitializer> content_initializer_;

  DISALLOW_COPY_AND_ASSIGN(ComponentsUnitTestEventListener);
};

}  // namespace

int main(int argc, char** argv) {
  ComponentsTestSuite test_suite(argc, argv);

  // The listener will set up common test environment for all components unit
  // tests.
  testing::TestEventListeners& listeners =
      testing::UnitTest::GetInstance()->listeners();
  listeners.Append(new ComponentsUnitTestEventListener());

  return base::LaunchUnitTests(
      argc, argv, base::Bind(&base::TestSuite::Run,
                             base::Unretained(&test_suite)));
}
