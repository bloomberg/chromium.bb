// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/tests/webgpu_test.h"

#include <dawn/dawn.h>

#include "base/test/test_simple_task_runner.h"
#include "gpu/command_buffer/client/webgpu_implementation.h"
#include "gpu/command_buffer/service/webgpu_decoder.h"
#include "gpu/ipc/in_process_command_buffer.h"
#include "gpu/ipc/in_process_gpu_thread_holder.h"
#include "gpu/ipc/webgpu_in_process_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

WebGPUTest::Options::Options() = default;

WebGPUTest::WebGPUTest() = default;
WebGPUTest::~WebGPUTest() = default;

void WebGPUTest::SetUp() {
  gpu_thread_holder_ = std::make_unique<InProcessGpuThreadHolder>();
  gpu_thread_holder_->GetGpuPreferences()->enable_webgpu = true;
}

void WebGPUTest::TearDown() {
  context_.reset();
}

void WebGPUTest::Initialize(const Options& options) {
  ContextCreationAttribs attributes;
  attributes.bind_generates_resource = false;
  attributes.enable_gles2_interface = false;
  attributes.context_type = CONTEXT_TYPE_WEBGPU;

  static constexpr GpuMemoryBufferManager* memory_buffer_manager = nullptr;
  static constexpr ImageFactory* image_factory = nullptr;
  static constexpr GpuChannelManagerDelegate* channel_manager = nullptr;
  context_ = std::make_unique<WebGPUInProcessContext>();
  ContextResult result =
      context_->Initialize(gpu_thread_holder_->GetTaskExecutor(), attributes,
                           options.shared_memory_limits, memory_buffer_manager,
                           image_factory, channel_manager);
  DCHECK_EQ(result, ContextResult::kSuccess);

  DawnProcTable procs = webgpu()->GetProcs();
  dawnSetProcs(&procs);
}

webgpu::WebGPUInterface* WebGPUTest::webgpu() const {
  return context_->GetImplementation();
}

void WebGPUTest::RunPendingTasks() {
  context_->GetTaskRunner()->RunPendingTasks();
}

}  // namespace gpu
