// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/statistics_recorder.h"
#include "base/path_service.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "content/public/test/test_content_client_initializer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"

#if defined(OS_MACOSX)
#include "base/mac/bundle_locations.h"
#endif

#if !defined(OS_IOS)
#include "ui/gl/gl_surface.h"
#endif

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "ui/base/android/ui_base_jni_registrar.h"
#include "ui/gfx/android/gfx_jni_registrar.h"
#endif

namespace {

class ComponentsTestSuite : public base::TestSuite {
 public:
  ComponentsTestSuite(int argc, char** argv) : base::TestSuite(argc, argv) {}

 private:
  virtual void Initialize() OVERRIDE {
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
#endif

#if defined(OS_MACOSX) && !defined(OS_IOS)
    // Look in the framework bundle for resources.
    base::FilePath path;
    PathService::Get(base::DIR_EXE, &path);

    // TODO(tfarina): This is temporary. The right fix is to write a
    // framework-Info.plist and integrate that into the build.
    // Hardcode the framework name here to avoid having to depend on chrome's
    // common target for chrome::kFrameworkName.
#if defined(GOOGLE_CHROME_BUILD)
    path = path.AppendASCII("Google Chrome Framework.framework");
#elif defined(CHROMIUM_BUILD)
    path = path.AppendASCII("Chromium Framework.framework");
#else
#error Unknown branding
#endif

    base::mac::SetOverrideFrameworkBundlePath(path);
#endif

    ui::RegisterPathProvider();

    // TODO(tfarina): This should be changed to InitSharedInstanceWithPakFile()
    // so we can load our pak file instead of chrome.pak. crbug.com/348563
    ui::ResourceBundle::InitSharedInstanceWithLocale("en-US", NULL);
    base::FilePath resources_pack_path;
    PathService::Get(base::DIR_MODULE, &resources_pack_path);
    ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
        resources_pack_path.AppendASCII("resources.pak"),
        ui::SCALE_FACTOR_NONE);
  }

  virtual void Shutdown() OVERRIDE {
    ui::ResourceBundle::CleanupSharedInstance();

#if defined(OS_MACOSX) && !defined(OS_IOS)
  base::mac::SetOverrideFrameworkBundle(NULL);
#endif

    base::TestSuite::Shutdown();
  }

  DISALLOW_COPY_AND_ASSIGN(ComponentsTestSuite);
};

class ComponentsUnitTestEventListener : public testing::EmptyTestEventListener {
 public:
  ComponentsUnitTestEventListener() {}
  virtual ~ComponentsUnitTestEventListener() {}

  virtual void OnTestStart(const testing::TestInfo& test_info) OVERRIDE {
    content_initializer_.reset(new content::TestContentClientInitializer());
  }

  virtual void OnTestEnd(const testing::TestInfo& test_info) OVERRIDE {
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
