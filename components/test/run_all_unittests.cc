// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "content/public/test/test_content_client_initializer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"

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
#if !defined(OS_IOS)
    gfx::GLSurface::InitializeOneOffForTests(true);
#endif
#if defined(OS_ANDROID)
    // Register JNI bindings for android.
    JNIEnv* env = base::android::AttachCurrentThread();
    gfx::android::RegisterJni(env);
    ui::android::RegisterJni(env);
#endif

    // TODO(tfarina): This should be changed to InitSharedInstanceWithPakFile()
    // so we can load our pak file instead of chrome.pak.
    ui::ResourceBundle::InitSharedInstanceWithLocale("en-US", NULL);
  }

  virtual void Shutdown() OVERRIDE {
    ui::ResourceBundle::CleanupSharedInstance();
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
