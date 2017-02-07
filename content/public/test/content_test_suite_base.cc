// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/content_test_suite_base.h"

#include <memory>

#include "base/compiler_specific.h"
#include "base/metrics/statistics_recorder.h"
#include "base/test/test_suite.h"
#include "base/threading/sequenced_worker_pool.h"
#include "build/build_config.h"
#include "content/browser/gpu/gpu_main_thread_factory.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/utility_process_host_impl.h"
#include "content/common/url_schemes.h"
#include "content/gpu/in_process_gpu_thread.h"
#include "content/public/common/content_client.h"
#include "content/renderer/in_process_renderer_thread.h"
#include "content/utility/in_process_utility_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/ui_base_paths.h"

#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
#include "gin/v8_initializer.h"  // nogncheck
#endif

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "content/browser/android/browser_jni_registrar.h"
#include "content/common/android/common_jni_registrar.h"
#include "device/geolocation/android/geolocation_jni_registrar.h"
#include "media/base/android/media_jni_registrar.h"
#include "media/capture/video/android/capture_jni_registrar.h"
#include "net/android/net_jni_registrar.h"
#include "ui/android/ui_android_jni_registrar.h"
#include "ui/base/android/ui_base_jni_registrar.h"
#include "ui/gfx/android/gfx_jni_registrar.h"
#include "ui/gl/android/gl_jni_registrar.h"
#endif

#if defined(OS_ANDROID) && !defined(USE_AURA)
#include "content/public/browser/android/compositor.h"
#include "ui/shell_dialogs/android/shell_dialogs_jni_registrar.h"
#endif

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif


namespace content {

ContentTestSuiteBase::ContentTestSuiteBase(int argc, char** argv)
    : base::TestSuite(argc, argv) {
}

void ContentTestSuiteBase::Initialize() {
  base::TestSuite::Initialize();

  // Initialize the histograms subsystem, so that any histograms hit in tests
  // are correctly registered with the statistics recorder and can be queried
  // by tests.
  base::StatisticsRecorder::Initialize();

#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
  gin::V8Initializer::LoadV8Snapshot();
  gin::V8Initializer::LoadV8Natives();
#endif

#if defined(OS_ANDROID)
  // Register JNI bindings for android.
  JNIEnv* env = base::android::AttachCurrentThread();
  content::android::RegisterCommonJni(env);
  content::android::RegisterBrowserJni(env);
  device::android::RegisterGeolocationJni(env);
  gfx::android::RegisterJni(env);
  media::RegisterCaptureJni(env);
  media::RegisterJni(env);
  net::android::RegisterJni(env);
  ui::android::RegisterJni(env);
  ui::RegisterUIAndroidJni(env);
  ui::gl::android::RegisterJni(env);
#if !defined(USE_AURA)
  ui::shell_dialogs::RegisterJni(env);
  content::Compositor::Initialize();
#endif
#endif

  ui::MaterialDesignController::Initialize();
}

void ContentTestSuiteBase::RegisterContentSchemes(
    ContentClient* content_client) {
  SetContentClient(content_client);
  content::RegisterContentSchemes(false);
  SetContentClient(NULL);
}

void ContentTestSuiteBase::RegisterInProcessThreads() {
  UtilityProcessHostImpl::RegisterUtilityMainThreadFactory(
      CreateInProcessUtilityThread);
  RenderProcessHostImpl::RegisterRendererMainThreadFactory(
      CreateInProcessRendererThread);
  content::RegisterGpuMainThreadFactory(CreateInProcessGpuThread);
}

}  // namespace content
