// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/ipc/service/gpu_jpeg_decode_accelerator_factory_provider.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/media_switches.h"
#include "media/gpu/fake_jpeg_decode_accelerator.h"

#if defined(OS_CHROMEOS) && defined(ARCH_CPU_X86_FAMILY)
#define USE_VAAPI_JDA
#endif

#if defined(OS_CHROMEOS) && defined(USE_V4L2_CODEC) && \
    defined(ARCH_CPU_ARM_FAMILY)
#define USE_V4L2_JDA
#endif

#if defined(USE_VAAPI_JDA)
#include "media/gpu/vaapi_jpeg_decode_accelerator.h"
#endif

#if defined(USE_V4L2_JDA)
#include "media/gpu/v4l2_device.h"
#include "media/gpu/v4l2_jpeg_decode_accelerator.h"
#endif

namespace media {

namespace {

#if defined(USE_V4L2_JDA)
std::unique_ptr<JpegDecodeAccelerator> CreateV4L2JDA(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner) {
  std::unique_ptr<JpegDecodeAccelerator> decoder;
  scoped_refptr<V4L2Device> device = V4L2Device::Create();
  if (device) {
    decoder.reset(
        new V4L2JpegDecodeAccelerator(device, std::move(io_task_runner)));
  }
  return decoder;
}
#endif

#if defined(USE_VAAPI_JDA)
std::unique_ptr<JpegDecodeAccelerator> CreateVaapiJDA(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner) {
  return base::MakeUnique<VaapiJpegDecodeAccelerator>(
      std::move(io_task_runner));
}
#endif

std::unique_ptr<JpegDecodeAccelerator> CreateFakeJDA(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner) {
  return base::MakeUnique<FakeJpegDecodeAccelerator>(std::move(io_task_runner));
}

}  // namespace

// static
bool GpuJpegDecodeAcceleratorFactoryProvider::
    IsAcceleratedJpegDecodeSupported() {
  auto accelerator_factory_functions = GetAcceleratorFactories();
  for (const auto& create_jda_function : accelerator_factory_functions) {
    std::unique_ptr<JpegDecodeAccelerator> accelerator =
        create_jda_function.Run(base::ThreadTaskRunnerHandle::Get());
    if (accelerator && accelerator->IsSupported())
      return true;
  }
  return false;
}

// static
std::vector<GpuJpegDecodeAcceleratorFactoryProvider::CreateAcceleratorCB>
GpuJpegDecodeAcceleratorFactoryProvider::GetAcceleratorFactories() {
  // This list is ordered by priority of use.
  std::vector<CreateAcceleratorCB> result;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseFakeJpegDecodeAccelerator)) {
    result.push_back(base::Bind(&CreateFakeJDA));
  } else {
#if defined(USE_V4L2_JDA)
    result.push_back(base::Bind(&CreateV4L2JDA));
#endif
#if defined(USE_VAAPI_JDA)
    result.push_back(base::Bind(&CreateVaapiJDA));
#endif
  }
  return result;
}

}  // namespace media
