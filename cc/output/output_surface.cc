// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/output_surface.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event.h"
#include "cc/output/managed_memory_policy.h"
#include "cc/output/output_surface_client.h"
#include "cc/output/output_surface_frame.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/skia/include/core/SkTraceMemoryDump.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/trace_util.h"

class SkDiscardableMemory;

namespace cc {

namespace {

// Constants used by SkiaGpuTraceMemoryDump to identify different memory types.
const char* kGLTextureBackingType = "gl_texture";
const char* kGLBufferBackingType = "gl_buffer";
const char* kGLRenderbufferBackingType = "gl_renderbuffer";

// Derives from SkTraceMemoryDump and implements graphics specific memory
// backing functionality.
class SkiaGpuTraceMemoryDump : public SkTraceMemoryDump {
 public:
  // This should never outlive the provided ProcessMemoryDump, as it should
  // always be scoped to a single OnMemoryDump funciton call.
  explicit SkiaGpuTraceMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                                  uint64_t share_group_tracing_guid)
      : pmd_(pmd), share_group_tracing_guid_(share_group_tracing_guid) {}

  // Overridden from SkTraceMemoryDump:
  void dumpNumericValue(const char* dump_name,
                        const char* value_name,
                        const char* units,
                        uint64_t value) override {
    auto* dump = GetOrCreateAllocatorDump(dump_name);
    dump->AddScalar(value_name, units, value);
  }

  void setMemoryBacking(const char* dump_name,
                        const char* backing_type,
                        const char* backing_object_id) override {
    const uint64_t tracing_process_id =
        base::trace_event::MemoryDumpManager::GetInstance()
            ->GetTracingProcessId();

    // For uniformity, skia provides this value as a string. Convert back to a
    // uint32_t.
    uint32_t gl_id =
        std::strtoul(backing_object_id, nullptr /* str_end */, 10 /* base */);

    // Populated in if statements below.
    base::trace_event::MemoryAllocatorDumpGuid guid;

    if (strcmp(backing_type, kGLTextureBackingType) == 0) {
      guid = gl::GetGLTextureClientGUIDForTracing(share_group_tracing_guid_,
                                                  gl_id);
    } else if (strcmp(backing_type, kGLBufferBackingType) == 0) {
      guid = gl::GetGLBufferGUIDForTracing(tracing_process_id, gl_id);
    } else if (strcmp(backing_type, kGLRenderbufferBackingType) == 0) {
      guid = gl::GetGLRenderbufferGUIDForTracing(tracing_process_id, gl_id);
    }

    if (!guid.empty()) {
      pmd_->CreateSharedGlobalAllocatorDump(guid);

      auto* dump = GetOrCreateAllocatorDump(dump_name);

      const int kImportance = 2;
      pmd_->AddOwnershipEdge(dump->guid(), guid, kImportance);
    }
  }

  void setDiscardableMemoryBacking(
      const char* dump_name,
      const SkDiscardableMemory& discardable_memory_object) override {
    // We don't use this class for dumping discardable memory.
    NOTREACHED();
  }

  LevelOfDetail getRequestedDetails() const override {
    // TODO(ssid): Use MemoryDumpArgs to create light dumps when requested
    // (crbug.com/499731).
    return kObjectsBreakdowns_LevelOfDetail;
  }

 private:
  // Helper to create allocator dumps.
  base::trace_event::MemoryAllocatorDump* GetOrCreateAllocatorDump(
      const char* dump_name) {
    auto* dump = pmd_->GetAllocatorDump(dump_name);
    if (!dump)
      dump = pmd_->CreateAllocatorDump(dump_name);
    return dump;
  }

  base::trace_event::ProcessMemoryDump* pmd_;
  uint64_t share_group_tracing_guid_;

  DISALLOW_COPY_AND_ASSIGN(SkiaGpuTraceMemoryDump);
};

}  // namespace

OutputSurface::OutputSurface(scoped_refptr<ContextProvider> context_provider)
    : context_provider_(std::move(context_provider)), weak_ptr_factory_(this) {
  DCHECK(context_provider_);
  thread_checker_.DetachFromThread();
}

OutputSurface::OutputSurface(
    std::unique_ptr<SoftwareOutputDevice> software_device)
    : software_device_(std::move(software_device)), weak_ptr_factory_(this) {
  DCHECK(software_device_);
  thread_checker_.DetachFromThread();
}

OutputSurface::OutputSurface(
    scoped_refptr<VulkanContextProvider> vulkan_context_provider)
    : vulkan_context_provider_(vulkan_context_provider),
      weak_ptr_factory_(this) {
  DCHECK(vulkan_context_provider_);
  thread_checker_.DetachFromThread();
}

OutputSurface::~OutputSurface() {
  // Is destroyed on the thread it is bound to.
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!client_)
    return;

  // Unregister any dump provider. Safe to call (no-op) if we have not yet
  // registered.
  if (base::ThreadTaskRunnerHandle::IsSet()) {
    base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
        this);
  }

  if (context_provider_) {
    context_provider_->SetLostContextCallback(
        ContextProvider::LostContextCallback());
  }
}

bool OutputSurface::BindToClient(OutputSurfaceClient* client) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(client);
  DCHECK(!client_);
  client_ = client;

  if (context_provider_) {
    if (!context_provider_->BindToCurrentThread())
      return false;

    context_provider_->SetLostContextCallback(base::Bind(
        &OutputSurface::DidLoseOutputSurface, base::Unretained(this)));
  }

  // In certain cases, ThreadTaskRunnerHandle isn't set (Android Webview).
  // Don't register a dump provider in these cases.
  // TODO(ericrk): Get this working in Android Webview. crbug.com/517156
  if (base::ThreadTaskRunnerHandle::IsSet()) {
    // Now that we are on the context thread, register a dump provider with this
    // thread's task runner. This will overwrite any previous dump provider
    // registered.
    base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        this, "OutputSurface", base::ThreadTaskRunnerHandle::Get());
  }
  return true;
}

void OutputSurface::Reshape(const gfx::Size& size,
                            float scale_factor,
                            const gfx::ColorSpace& color_space,
                            bool has_alpha) {
  device_color_space_ = color_space;
  if (size == surface_size_ && scale_factor == device_scale_factor_ &&
      has_alpha == has_alpha_)
    return;

  surface_size_ = size;
  device_scale_factor_ = scale_factor;
  has_alpha_ = has_alpha;
  if (context_provider_.get()) {
    context_provider_->ContextGL()->ResizeCHROMIUM(size.width(), size.height(),
                                                   scale_factor, has_alpha);
  }
  if (software_device_)
    software_device_->Resize(size, scale_factor);
}

void OutputSurface::PostSwapBuffersComplete() {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&OutputSurface::OnSwapBuffersComplete,
                            weak_ptr_factory_.GetWeakPtr()));
}

// We don't post tasks bound to the client directly since they might run
// after the OutputSurface has been destroyed.
void OutputSurface::OnSwapBuffersComplete() {
  client_->DidSwapBuffersComplete();
}

bool OutputSurface::OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                                 base::trace_event::ProcessMemoryDump* pmd) {
  if (auto* context_provider = this->context_provider()) {
    // No need to lock, main context provider is not shared.
    if (auto* gr_context = context_provider->GrContext()) {
      SkiaGpuTraceMemoryDump trace_memory_dump(
          pmd, context_provider->ContextSupport()->ShareGroupTracingGUID());
      gr_context->dumpMemoryStatistics(&trace_memory_dump);
    }
  }
  return true;
}

void OutputSurface::DidLoseOutputSurface() {
  TRACE_EVENT0("cc", "OutputSurface::DidLoseOutputSurface");
  client_->DidLoseOutputSurface();
}

}  // namespace cc
