// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/public/browser/gpu_data_manager_observer.h"
#include "content/public/common/content_switches.h"
#include "content/test/content_browser_test.h"

namespace content {

namespace {

class TestObserver : public GpuDataManagerObserver {
 public:
  explicit TestObserver(base::MessageLoop* message_loop)
      : message_loop_(message_loop) {
  }

  virtual ~TestObserver() { }

  virtual void OnGpuInfoUpdate() OVERRIDE {
    // Display GPU/Driver information.
    gpu::GPUInfo gpu_info =
        GpuDataManagerImpl::GetInstance()->GetGPUInfo();
    std::string vendor_id = base::StringPrintf(
        "0x%04x", gpu_info.gpu.vendor_id);
    std::string device_id = base::StringPrintf(
        "0x%04x", gpu_info.gpu.device_id);
    LOG(INFO) << "GPU[0]: vendor_id = " << vendor_id
              << ", device_id = " << device_id;
    for (size_t i = 0; i < gpu_info.secondary_gpus.size(); ++i) {
      gpu::GPUInfo::GPUDevice gpu = gpu_info.secondary_gpus[i];
      vendor_id = base::StringPrintf("0x%04x", gpu.vendor_id);
      device_id = base::StringPrintf("0x%04x", gpu.device_id);
      LOG(INFO) << "GPU[" << (i + 1)
                << "]: vendor_id = " << vendor_id
                << ", device_od = " << device_id;
    }
    LOG(INFO) << "GPU Driver: vendor = " << gpu_info.driver_vendor
              << ", version = " << gpu_info.driver_version
              << ", date = " << gpu_info.driver_date;

    // Display GL information.
    LOG(INFO) << "GL: vendor = " << gpu_info.gl_vendor
              << ", renderer = " << gpu_info.gl_renderer;

    // Display GL window system binding information.
    LOG(INFO) << "GL Window System: vendor = " << gpu_info.gl_ws_vendor
              << ", version = " << gpu_info.gl_ws_version;

    // Display OS information.
    LOG(INFO) << "OS = " << base::SysInfo::OperatingSystemName()
              << " " << base::SysInfo::OperatingSystemVersion();

    message_loop_->Quit();
  }

 private:
  base::MessageLoop* message_loop_;
};

}  // namespace anonymous

class GpuInfoBrowserTest : public ContentBrowserTest {
 public:
  GpuInfoBrowserTest()
      : message_loop_(base::MessageLoop::TYPE_UI) {
  }

  virtual void SetUp() {
    // We expect real pixel output for these tests.
    UseRealGLContexts();

    ContentBrowserTest::SetUp();
  }

  base::MessageLoop* GetMessageLoop() { return &message_loop_; }

 private:
  base::MessageLoop message_loop_;

  DISALLOW_COPY_AND_ASSIGN(GpuInfoBrowserTest);
};

IN_PROC_BROWSER_TEST_F(GpuInfoBrowserTest, MANUAL_DisplayGpuInfo) {
  // crbug.com/262287
#if defined(OS_MACOSX)
  // TODO(zmo): crashing on Mac, and also we don't have the full info
  // collected.
  return;
#endif
#if defined(OS_LINUX) && !defined(NDEBUG)
  // TODO(zmo): crashing on Linux Debug.
  return;
#endif
  TestObserver observer(GetMessageLoop());
  GpuDataManagerImpl::GetInstance()->AddObserver(&observer);
  GpuDataManagerImpl::GetInstance()->RequestCompleteGpuInfoIfNeeded();

  GetMessageLoop()->Run();
}

}  // namespace content

