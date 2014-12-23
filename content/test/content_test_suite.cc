// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/content_test_suite.h"

#if defined(OS_ANDROID)
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <map>
#endif

#include "base/base_paths.h"
#include "base/logging.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/test_content_client_initializer.h"
#include "gpu/config/gpu_util.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "ui/gfx/win/dpi.h"
#endif

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#if !defined(OS_IOS)
#include "base/test/mock_chrome_application_mac.h"
#endif
#endif

#if !defined(OS_IOS)
#include "base/base_switches.h"
#include "base/command_line.h"
#include "media/base/media.h"
#include "ui/gl/gl_surface.h"
#endif

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "base/memory/linked_ptr.h"
#include "content/common/android/surface_texture_manager.h"
#include "ui/gl/android/scoped_java_surface.h"
#include "ui/gl/android/surface_texture.h"
#endif

namespace content {
namespace {

class TestInitializationListener : public testing::EmptyTestEventListener {
 public:
  TestInitializationListener() : test_content_client_initializer_(NULL) {
  }

  virtual void OnTestStart(const testing::TestInfo& test_info) override {
    test_content_client_initializer_ =
        new content::TestContentClientInitializer();
  }

  virtual void OnTestEnd(const testing::TestInfo& test_info) override {
    delete test_content_client_initializer_;
  }

 private:
  content::TestContentClientInitializer* test_content_client_initializer_;

  DISALLOW_COPY_AND_ASSIGN(TestInitializationListener);
};

#if defined(OS_ANDROID)
class SurfaceTextureManagerImpl : public SurfaceTextureManager {
 public:
  // Overridden from SurfaceTextureManager:
  void RegisterSurfaceTexture(int surface_texture_id,
                              int client_id,
                              gfx::SurfaceTexture* surface_texture) override {
    surfaces_[surface_texture_id] =
        make_linked_ptr(new gfx::ScopedJavaSurface(surface_texture));
  }
  void UnregisterSurfaceTexture(int surface_texture_id,
                                int client_id) override {
    surfaces_.erase(surface_texture_id);
  }
  gfx::AcceleratedWidget AcquireNativeWidgetForSurfaceTexture(
      int surface_texture_id) override {
    JNIEnv* env = base::android::AttachCurrentThread();
    return ANativeWindow_fromSurface(
        env, surfaces_[surface_texture_id]->j_surface().obj());
  }

 private:
  typedef std::map<int, linked_ptr<gfx::ScopedJavaSurface>> SurfaceMap;
  SurfaceMap surfaces_;
};
#endif

}  // namespace

ContentTestSuite::ContentTestSuite(int argc, char** argv)
    : ContentTestSuiteBase(argc, argv) {
}

ContentTestSuite::~ContentTestSuite() {
}

void ContentTestSuite::Initialize() {
#if defined(OS_MACOSX)
  base::mac::ScopedNSAutoreleasePool autorelease_pool;
#if !defined(OS_IOS)
  mock_cr_app::RegisterMockCrApp();
#endif
#endif

#if defined(OS_WIN)
  gfx::InitDeviceScaleFactor(1.0f);
#endif

  ContentTestSuiteBase::Initialize();
  {
    ContentClient client;
    ContentTestSuiteBase::RegisterContentSchemes(&client);
  }
  RegisterPathProvider();
#if !defined(OS_IOS)
  media::InitializeMediaLibraryForTesting();
  // When running in a child process for Mac sandbox tests, the sandbox exists
  // to initialize GL, so don't do it here.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kTestChildProcess)) {
    gfx::GLSurface::InitializeOneOffForTests();
    gpu::ApplyGpuDriverBugWorkarounds(base::CommandLine::ForCurrentProcess());
  }
#endif
  testing::TestEventListeners& listeners =
      testing::UnitTest::GetInstance()->listeners();
  listeners.Append(new TestInitializationListener);
#if defined(OS_ANDROID)
  SurfaceTextureManager::InitInstance(new SurfaceTextureManagerImpl);
#endif
}

}  // namespace content
