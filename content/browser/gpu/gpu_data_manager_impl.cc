// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/gpu_data_manager_impl.h"

#if defined(OS_MACOSX)
#include <CoreGraphics/CGDisplayConfiguration.h>
#endif

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/sys_info.h"
#include "base/values.h"
#include "base/version.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/gpu/gpu_info_collector.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/gpu_data_manager_observer.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "ui/gfx/gl/gl_implementation.h"
#include "ui/gfx/gl/gl_switches.h"
#include "webkit/plugins/plugin_switches.h"

using content::BrowserThread;
using content::GpuDataManagerObserver;
using content::GpuFeatureType;

namespace {

#if defined(OS_MACOSX)
void DisplayReconfigCallback(CGDirectDisplayID display,
                             CGDisplayChangeSummaryFlags flags,
                             void* gpu_data_manager) {
  // TODO(zmo): this logging is temporary for crbug 88008 and will be removed.
  LOG(INFO) << "Display re-configuration: flags = 0x"
            << base::StringPrintf("%04x", flags);
  if (flags & kCGDisplayAddFlag) {
    GpuDataManagerImpl* manager =
        reinterpret_cast<GpuDataManagerImpl*>(gpu_data_manager);
    DCHECK(manager);
    manager->HandleGpuSwitch();
  }
}
#endif

}  // namespace anonymous

// static
content::GpuDataManager* content::GpuDataManager::GetInstance() {
  return GpuDataManagerImpl::GetInstance();
}

// static
GpuDataManagerImpl* GpuDataManagerImpl::GetInstance() {
  return Singleton<GpuDataManagerImpl>::get();
}

GpuDataManagerImpl::GpuDataManagerImpl()
    : complete_gpu_info_already_requested_(false),
      complete_gpu_info_available_(false),
      gpu_feature_type_(content::GPU_FEATURE_TYPE_UNKNOWN),
      preliminary_gpu_feature_type_(content::GPU_FEATURE_TYPE_UNKNOWN),
      observer_list_(new GpuDataManagerObserverList),
      software_rendering_(false),
      card_blacklisted_(false) {
  Initialize();
}

void GpuDataManagerImpl::Initialize() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDisableAcceleratedCompositing)) {
    command_line->AppendSwitch(switches::kDisableAccelerated2dCanvas);
    command_line->AppendSwitch(switches::kDisableAcceleratedLayers);
  }

  if (!command_line->HasSwitch(switches::kSkipGpuDataLoading)) {
    content::GPUInfo gpu_info;
    gpu_info_collector::CollectPreliminaryGraphicsInfo(&gpu_info);
    {
      base::AutoLock auto_lock(gpu_info_lock_);
      gpu_info_ = gpu_info;
    }
  }

#if defined(OS_MACOSX)
  CGDisplayRegisterReconfigurationCallback(DisplayReconfigCallback, this);
#endif
}

GpuDataManagerImpl::~GpuDataManagerImpl() {
#if defined(OS_MACOSX)
  CGDisplayRemoveReconfigurationCallback(DisplayReconfigCallback, this);
#endif
}

void GpuDataManagerImpl::RequestCompleteGpuInfoIfNeeded() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (complete_gpu_info_already_requested_ || complete_gpu_info_available_)
    return;
  complete_gpu_info_already_requested_ = true;

  GpuProcessHost::SendOnIO(
      GpuProcessHost::GPU_PROCESS_KIND_UNSANDBOXED,
      content::CAUSE_FOR_GPU_LAUNCH_GPUDATAMANAGER_REQUESTCOMPLETEGPUINFOIFNEEDED,
      new GpuMsg_CollectGraphicsInfo());
}

bool GpuDataManagerImpl::IsCompleteGPUInfoAvailable() const {
  return complete_gpu_info_available_;
}

void GpuDataManagerImpl::UpdateGpuInfo(const content::GPUInfo& gpu_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  complete_gpu_info_available_ =
      complete_gpu_info_available_ || gpu_info.finalized;
  complete_gpu_info_already_requested_ =
      complete_gpu_info_already_requested_ || gpu_info.finalized;
  {
    base::AutoLock auto_lock(gpu_info_lock_);
    if (!Merge(&gpu_info_, gpu_info))
      return;
    content::GetContentClient()->SetGpuInfo(gpu_info_);
  }

  // We have to update GpuFeatureType before notify all the observers.
  NotifyGpuInfoUpdate();
}

content::GPUInfo GpuDataManagerImpl::GetGPUInfo() const {
  return gpu_info_;
}

void GpuDataManagerImpl::AddLogMessage(Value* msg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  log_messages_.Append(msg);
}

GpuFeatureType GpuDataManagerImpl::GetGpuFeatureType() {
  if (software_rendering_) {
    GpuFeatureType flags;

    // Skia's software rendering is probably more efficient than going through
    // software emulation of the GPU, so use that.
    flags = content::GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS;
    return flags;
  }

  return gpu_feature_type_;
}

bool GpuDataManagerImpl::GpuAccessAllowed() {
  if (software_rendering_)
    return true;

  if (!gpu_info_.gpu_accessible)
    return false;

  // We only need to block GPU process if more features are disallowed other
  // than those in the preliminary gpu feature flags because the latter work
  // through renderer commandline switches.
  uint32 mask = ~(preliminary_gpu_feature_type_);
  return (gpu_feature_type_ & mask) == 0;
}

void GpuDataManagerImpl::AddObserver(GpuDataManagerObserver* observer) {
  observer_list_->AddObserver(observer);
}

void GpuDataManagerImpl::RemoveObserver(GpuDataManagerObserver* observer) {
  observer_list_->RemoveObserver(observer);
}

void GpuDataManagerImpl::AppendRendererCommandLine(
    CommandLine* command_line) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(command_line);

  uint32 flags = GetGpuFeatureType();
  if ((flags & content::GPU_FEATURE_TYPE_WEBGL)) {
    if (!command_line->HasSwitch(switches::kDisableExperimentalWebGL))
      command_line->AppendSwitch(switches::kDisableExperimentalWebGL);
    if (!command_line->HasSwitch(switches::kDisablePepper3dForUntrustedUse))
      command_line->AppendSwitch(switches::kDisablePepper3dForUntrustedUse);
  }
  if ((flags & content::GPU_FEATURE_TYPE_MULTISAMPLING) &&
      !command_line->HasSwitch(switches::kDisableGLMultisampling))
    command_line->AppendSwitch(switches::kDisableGLMultisampling);
  if ((flags & content::GPU_FEATURE_TYPE_ACCELERATED_COMPOSITING) &&
      !command_line->HasSwitch(switches::kDisableAcceleratedCompositing))
    command_line->AppendSwitch(switches::kDisableAcceleratedCompositing);
  if ((flags & content::GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS) &&
      !command_line->HasSwitch(switches::kDisableAccelerated2dCanvas))
    command_line->AppendSwitch(switches::kDisableAccelerated2dCanvas);
}

void GpuDataManagerImpl::AppendGpuCommandLine(
    CommandLine* command_line) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(command_line);

  std::string use_gl =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(switches::kUseGL);
  FilePath swiftshader_path =
      CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          switches::kSwiftShaderPath);
  uint32 flags = GetGpuFeatureType();
  if ((flags & content::GPU_FEATURE_TYPE_MULTISAMPLING) &&
      !command_line->HasSwitch(switches::kDisableGLMultisampling))
    command_line->AppendSwitch(switches::kDisableGLMultisampling);

  if (software_rendering_) {
    command_line->AppendSwitchASCII(switches::kUseGL, "swiftshader");
    if (swiftshader_path.empty())
      swiftshader_path = swiftshader_path_;
  } else if ((flags & (content::GPU_FEATURE_TYPE_WEBGL |
                content::GPU_FEATURE_TYPE_ACCELERATED_COMPOSITING |
                content::GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS)) &&
      (use_gl == "any")) {
    command_line->AppendSwitchASCII(
        switches::kUseGL, gfx::kGLImplementationOSMesaName);
  } else if (!use_gl.empty()) {
    command_line->AppendSwitchASCII(switches::kUseGL, use_gl);
  }

  if (!swiftshader_path.empty())
    command_line->AppendSwitchPath(switches::kSwiftShaderPath,
                                   swiftshader_path);

  {
    base::AutoLock auto_lock(gpu_info_lock_);
    if (gpu_info_.optimus)
      command_line->AppendSwitch(switches::kReduceGpuSandbox);
  }
}

void GpuDataManagerImpl::SetGpuFeatureType(GpuFeatureType feature_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  UpdateGpuFeatureType(feature_type);
  preliminary_gpu_feature_type_ = gpu_feature_type_;
}

void GpuDataManagerImpl::HandleGpuSwitch() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  content::GPUInfo gpu_info;
  gpu_info_collector::CollectVideoCardInfo(&gpu_info);
  LOG(INFO) << "Switching to use GPU: vendor_id = 0x"
            << base::StringPrintf("%04x", gpu_info.vendor_id)
            << ", device_id = 0x"
            << base::StringPrintf("%04x", gpu_info.device_id);
  // TODO(zmo): update gpu_info_, re-run blacklist logic, maybe close and
  // relaunch GPU process.
}

void GpuDataManagerImpl::NotifyGpuInfoUpdate() {
  observer_list_->Notify(&GpuDataManagerObserver::OnGpuInfoUpdate);
}

void GpuDataManagerImpl::UpdateGpuFeatureType(
    GpuFeatureType embedder_feature_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  CommandLine* command_line = CommandLine::ForCurrentProcess();
  int flags = embedder_feature_type;

  // Force disable using the GPU for these features, even if they would
  // otherwise be allowed.
  if (card_blacklisted_ ||
      command_line->HasSwitch(switches::kBlacklistAcceleratedCompositing)) {
    flags |= content::GPU_FEATURE_TYPE_ACCELERATED_COMPOSITING;
  }
  if (card_blacklisted_ ||
      command_line->HasSwitch(switches::kBlacklistWebGL)) {
    flags |= content::GPU_FEATURE_TYPE_WEBGL;
  }
  gpu_feature_type_ = static_cast<GpuFeatureType>(flags);

  EnableSoftwareRenderingIfNecessary();
}

void GpuDataManagerImpl::RegisterSwiftShaderPath(const FilePath& path) {
  swiftshader_path_ = path;
  EnableSoftwareRenderingIfNecessary();
}

const base::ListValue& GpuDataManagerImpl::GetLogMessages() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return log_messages_;
}

void GpuDataManagerImpl::EnableSoftwareRenderingIfNecessary() {
  if (!GpuAccessAllowed() ||
      (gpu_feature_type_ & content::GPU_FEATURE_TYPE_WEBGL)) {
#if defined(ENABLE_SWIFTSHADER)
    if (!swiftshader_path_.empty() &&
        !CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kDisableSoftwareRasterizer))
      software_rendering_ = true;
#endif
  }
}

bool GpuDataManagerImpl::ShouldUseSoftwareRendering() {
  return software_rendering_;
}

void GpuDataManagerImpl::BlacklistCard() {
  card_blacklisted_ = true;

  {
    base::AutoLock auto_lock(gpu_info_lock_);
    int flags = gpu_feature_type_;
    flags |= content::GPU_FEATURE_TYPE_ACCELERATED_COMPOSITING |
             content::GPU_FEATURE_TYPE_WEBGL;
    gpu_feature_type_ = static_cast<GpuFeatureType>(flags);
  }

  EnableSoftwareRenderingIfNecessary();
  NotifyGpuInfoUpdate();
}

bool GpuDataManagerImpl::Merge(content::GPUInfo* object,
                               const content::GPUInfo& other) {
  if (object->device_id != other.device_id ||
      object->vendor_id != other.vendor_id) {
    *object = other;
    return true;
  }

  bool changed = false;
  if (!object->finalized) {
    object->finalized = other.finalized;
    object->initialization_time = other.initialization_time;
    object->optimus |= other.optimus;

    if (object->driver_vendor.empty()) {
      changed |= object->driver_vendor != other.driver_vendor;
      object->driver_vendor = other.driver_vendor;
    }
    if (object->driver_version.empty()) {
      changed |= object->driver_version != other.driver_version;
      object->driver_version = other.driver_version;
    }
    if (object->driver_date.empty()) {
      changed |= object->driver_date != other.driver_date;
      object->driver_date = other.driver_date;
    }
    if (object->pixel_shader_version.empty()) {
      changed |= object->pixel_shader_version != other.pixel_shader_version;
      object->pixel_shader_version = other.pixel_shader_version;
    }
    if (object->vertex_shader_version.empty()) {
      changed |= object->vertex_shader_version != other.vertex_shader_version;
      object->vertex_shader_version = other.vertex_shader_version;
    }
    if (object->gl_version.empty()) {
      changed |= object->gl_version != other.gl_version;
      object->gl_version = other.gl_version;
    }
    if (object->gl_version_string.empty()) {
      changed |= object->gl_version_string != other.gl_version_string;
      object->gl_version_string = other.gl_version_string;
    }
    if (object->gl_vendor.empty()) {
      changed |= object->gl_vendor != other.gl_vendor;
      object->gl_vendor = other.gl_vendor;
    }
    if (object->gl_renderer.empty()) {
      changed |= object->gl_renderer != other.gl_renderer;
      object->gl_renderer = other.gl_renderer;
    }
    if (object->gl_extensions.empty()) {
      changed |= object->gl_extensions != other.gl_extensions;
      object->gl_extensions = other.gl_extensions;
    }
    object->can_lose_context = other.can_lose_context;
    object->software_rendering = other.software_rendering;
    object->gpu_accessible = other.gpu_accessible;
#if defined(OS_WIN)
    if (object->dx_diagnostics.values.size() == 0 &&
        object->dx_diagnostics.children.size() == 0) {
      object->dx_diagnostics = other.dx_diagnostics;
      changed = true;
    }
#endif
  }
  return changed;
}
