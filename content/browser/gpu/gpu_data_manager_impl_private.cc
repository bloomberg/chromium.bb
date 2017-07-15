// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/gpu_data_manager_impl_private.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "base/version.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/gpu_data_manager_observer.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/web_preferences.h"
#include "gpu/command_buffer/service/gpu_preferences.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/config/gpu_driver_bug_list_autogen.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_info_collector.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/config/gpu_util.h"
#include "gpu/config/software_rendering_list_autogen.h"
#include "gpu/ipc/common/memory_stats.h"
#include "gpu/ipc/service/switches.h"
#include "media/media_features.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/switches.h"
#include "ui/gl/gl_features.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gpu_switching_manager.h"

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_switches.h"
#endif
#if defined(OS_MACOSX)
#include <ApplicationServices/ApplicationServices.h>
#endif  // OS_MACOSX
#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif  // OS_WIN

namespace content {

namespace {

#if defined(OS_WIN)

enum WinSubVersion {
  kWinOthers = 0,
  kWinXP,
  kWinVista,
  kWin7,
  kWin8,
  kWin8_1,
  kWin10,
  kWin10_TH2,
  kWin10_RS1,
  kWin10_RS2,
  kNumWinSubVersions
};

int GetGpuBlacklistHistogramValueWin(gpu::GpuFeatureStatus status) {
  static WinSubVersion sub_version = kNumWinSubVersions;
  if (sub_version == kNumWinSubVersions) {
    sub_version = kWinOthers;
    switch (base::win::GetVersion()) {
      case base::win::VERSION_PRE_XP:
      case base::win::VERSION_XP:
      case base::win::VERSION_SERVER_2003:
      case base::win::VERSION_VISTA:
      case base::win::VERSION_WIN_LAST:
        break;
      case base::win::VERSION_WIN7:
        sub_version = kWin7;
        break;
      case base::win::VERSION_WIN8:
        sub_version = kWin8;
        break;
      case base::win::VERSION_WIN8_1:
        sub_version = kWin8_1;
        break;
      case base::win::VERSION_WIN10:
        sub_version = kWin10;
        break;
      case base::win::VERSION_WIN10_TH2:
        sub_version = kWin10_TH2;
        break;
      case base::win::VERSION_WIN10_RS1:
        sub_version = kWin10_RS1;
        break;
      case base::win::VERSION_WIN10_RS2:
        sub_version = kWin10_RS2;
        break;
    }
  }
  int entry_index = static_cast<int>(sub_version) * gpu::kGpuFeatureStatusMax;
  switch (status) {
    case gpu::kGpuFeatureStatusEnabled:
      break;
    case gpu::kGpuFeatureStatusBlacklisted:
      entry_index++;
      break;
    case gpu::kGpuFeatureStatusDisabled:
      entry_index += 2;
      break;
    case gpu::kGpuFeatureStatusUndefined:
    case gpu::kGpuFeatureStatusMax:
      NOTREACHED();
      break;
  }
  return entry_index;
}
#endif  // OS_WIN

// Send UMA histograms about the enabled features and GPU properties.
void UpdateStats(const gpu::GPUInfo& gpu_info,
                 const gpu::GpuBlacklist* blacklist,
                 const std::set<int>& blacklisted_features) {
  uint32_t max_entry_id = blacklist->max_entry_id();
  if (max_entry_id == 0) {
    // GPU Blacklist was not loaded.  No need to go further.
    return;
  }

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  // Use entry 0 to capture the total number of times that data
  // was recorded in this histogram in order to have a convenient
  // denominator to compute blacklist percentages for the rest of the
  // entries.
  UMA_HISTOGRAM_EXACT_LINEAR("GPU.BlacklistTestResultsPerEntry", 0,
                             max_entry_id + 1);

  if (blacklisted_features.size() != 0) {
    std::vector<uint32_t> flag_entries;
    blacklist->GetDecisionEntries(&flag_entries);
    DCHECK_GT(flag_entries.size(), 0u);
    for (size_t i = 0; i < flag_entries.size(); ++i) {
      UMA_HISTOGRAM_EXACT_LINEAR("GPU.BlacklistTestResultsPerEntry",
                                 flag_entries[i], max_entry_id + 1);
    }
  }

  const gpu::GpuFeatureType kGpuFeatures[] = {
      gpu::GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS,
      gpu::GPU_FEATURE_TYPE_GPU_COMPOSITING,
      gpu::GPU_FEATURE_TYPE_GPU_RASTERIZATION,
      gpu::GPU_FEATURE_TYPE_ACCELERATED_WEBGL, gpu::GPU_FEATURE_TYPE_WEBGL2};
  const std::string kGpuBlacklistFeatureHistogramNames[] = {
      "GPU.BlacklistFeatureTestResults.Accelerated2dCanvas",
      "GPU.BlacklistFeatureTestResults.GpuCompositing",
      "GPU.BlacklistFeatureTestResults.GpuRasterization",
      "GPU.BlacklistFeatureTestResults.Webgl",
      "GPU.BlacklistFeatureTestResults.Webgl2"};
  const bool kGpuFeatureUserFlags[] = {
      command_line.HasSwitch(switches::kDisableAccelerated2dCanvas),
      command_line.HasSwitch(switches::kDisableGpu),
      command_line.HasSwitch(switches::kDisableGpuRasterization),
      command_line.HasSwitch(switches::kDisableExperimentalWebGL),
      (!command_line.HasSwitch(switches::kEnableES3APIs) ||
       command_line.HasSwitch(switches::kDisableES3APIs))};
#if defined(OS_WIN)
  const std::string kGpuBlacklistFeatureHistogramNamesWin[] = {
      "GPU.BlacklistFeatureTestResultsWindows.Accelerated2dCanvas",
      "GPU.BlacklistFeatureTestResultsWindows.GpuCompositing",
      "GPU.BlacklistFeatureTestResultsWindows.GpuRasterization",
      "GPU.BlacklistFeatureTestResultsWindows.Webgl",
      "GPU.BlacklistFeatureTestResultsWindows.Webgl2"};
#endif
  const size_t kNumFeatures =
      sizeof(kGpuFeatures) / sizeof(gpu::GpuFeatureType);
  for (size_t i = 0; i < kNumFeatures; ++i) {
    // We can't use UMA_HISTOGRAM_ENUMERATION here because the same name is
    // expected if the macro is used within a loop.
    gpu::GpuFeatureStatus value = gpu::kGpuFeatureStatusEnabled;
    if (blacklisted_features.count(kGpuFeatures[i]))
      value = gpu::kGpuFeatureStatusBlacklisted;
    else if (kGpuFeatureUserFlags[i])
      value = gpu::kGpuFeatureStatusDisabled;
    base::HistogramBase* histogram_pointer = base::LinearHistogram::FactoryGet(
        kGpuBlacklistFeatureHistogramNames[i], 1, gpu::kGpuFeatureStatusMax,
        gpu::kGpuFeatureStatusMax + 1,
        base::HistogramBase::kUmaTargetedHistogramFlag);
    histogram_pointer->Add(value);
#if defined(OS_WIN)
    histogram_pointer = base::LinearHistogram::FactoryGet(
        kGpuBlacklistFeatureHistogramNamesWin[i], 1,
        kNumWinSubVersions * gpu::kGpuFeatureStatusMax,
        kNumWinSubVersions * gpu::kGpuFeatureStatusMax + 1,
        base::HistogramBase::kUmaTargetedHistogramFlag);
    histogram_pointer->Add(GetGpuBlacklistHistogramValueWin(value));
#endif
  }
}

void UpdateDriverBugListStats(const gpu::GpuDriverBugList* bug_list,
                              const std::set<int>& workarounds) {
  uint32_t max_entry_id = bug_list->max_entry_id();
  if (max_entry_id == 0) {
    // Driver bug list was not loaded.  No need to go further.
    return;
  }

  // Use entry 0 to capture the total number of times that data was recorded
  // in this histogram in order to have a convenient denominator to compute
  // driver bug list percentages for the rest of the entries.
  UMA_HISTOGRAM_SPARSE_SLOWLY("GPU.DriverBugTestResultsPerEntry", 0);

  if (workarounds.size() != 0) {
    std::vector<uint32_t> flag_entries;
    bug_list->GetDecisionEntries(&flag_entries);
    DCHECK_GT(flag_entries.size(), 0u);
    for (size_t i = 0; i < flag_entries.size(); ++i) {
      UMA_HISTOGRAM_SPARSE_SLOWLY("GPU.DriverBugTestResultsPerEntry",
                                  flag_entries[i]);
    }
  }
}

// Combine the integers into a string, seperated by ','.
std::string IntSetToString(const std::set<int>& list) {
  std::string rt;
  for (std::set<int>::const_iterator it = list.begin();
       it != list.end(); ++it) {
    if (!rt.empty())
      rt += ",";
    rt += base::IntToString(*it);
  }
  return rt;
}

#if defined(OS_MACOSX)
void DisplayReconfigCallback(CGDirectDisplayID display,
                             CGDisplayChangeSummaryFlags flags,
                             void* gpu_data_manager) {
  if (flags == kCGDisplayBeginConfigurationFlag)
    return; // This call contains no information about the display change

  GpuDataManagerImpl* manager =
      reinterpret_cast<GpuDataManagerImpl*>(gpu_data_manager);
  DCHECK(manager);

  bool gpu_changed = false;
  if (flags & kCGDisplayAddFlag) {
    gpu::GPUInfo gpu_info;
    if (gpu::CollectBasicGraphicsInfo(&gpu_info) == gpu::kCollectInfoSuccess) {
      gpu_changed = manager->UpdateActiveGpu(gpu_info.active_gpu().vendor_id,
                                             gpu_info.active_gpu().device_id);
    }
  }

  if (gpu_changed)
    manager->HandleGpuSwitch();
}
#endif  // OS_MACOSX

// Block all domains' use of 3D APIs for this many milliseconds if
// approaching a threshold where system stability might be compromised.
const int64_t kBlockAllDomainsMs = 10000;
const int kNumResetsWithinDuration = 1;

// Enums for UMA histograms.
enum BlockStatusHistogram {
  BLOCK_STATUS_NOT_BLOCKED,
  BLOCK_STATUS_SPECIFIC_DOMAIN_BLOCKED,
  BLOCK_STATUS_ALL_DOMAINS_BLOCKED,
  BLOCK_STATUS_MAX
};

bool ShouldDisableHardwareAcceleration() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableGpu);
}

void OnVideoMemoryUsageStats(
    const base::Callback<void(const gpu::VideoMemoryUsageStats& stats)>&
        callback,
    const gpu::VideoMemoryUsageStats& stats) {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(callback, stats));
}

void RequestVideoMemoryUsageStats(
    const base::Callback<void(const gpu::VideoMemoryUsageStats& stats)>&
        callback,
    GpuProcessHost* host) {
  if (!host)
    return;
  host->gpu_service()->GetVideoMemoryUsageStats(
      base::Bind(&OnVideoMemoryUsageStats, callback));
}

void UpdateGpuInfoOnIO(const gpu::GPUInfo& gpu_info) {
  // This function is called on the IO thread, but GPUInfo on GpuDataManagerImpl
  // should be updated on the UI thread (since it can call into functions that
  // expect to run in the UI thread, e.g. ContentClient::SetGpuInfo()).
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          [](const gpu::GPUInfo& gpu_info) {
            TRACE_EVENT0("test_gpu", "OnGraphicsInfoCollected");
            GpuDataManagerImpl::GetInstance()->UpdateGpuInfo(gpu_info);
          },
          gpu_info));
}

}  // namespace anonymous

void GpuDataManagerImplPrivate::InitializeForTesting(
    const gpu::GpuControlListData& gpu_blacklist_data,
    const gpu::GPUInfo& gpu_info) {
  // This function is for testing only, so disable histograms.
  update_histograms_ = false;

  // Prevent all further initialization.
  finalized_ = true;

  gpu::GpuControlListData gpu_driver_bug_list_data;
  InitializeImpl(gpu_blacklist_data, gpu_driver_bug_list_data, gpu_info);
}

bool GpuDataManagerImplPrivate::IsFeatureBlacklisted(int feature) const {
#if defined(OS_CHROMEOS)
  if (feature == gpu::GPU_FEATURE_TYPE_PANEL_FITTING &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisablePanelFitting)) {
    return true;
  }
#endif  // OS_CHROMEOS

  // SwiftShader blacklists all features
  return use_swiftshader_ || (blacklisted_features_.count(feature) == 1);
}

bool GpuDataManagerImplPrivate::IsFeatureEnabled(int feature) const {
  DCHECK_EQ(feature, gpu::GPU_FEATURE_TYPE_GPU_RASTERIZATION);
  return gpu_feature_info_
             .status_values[gpu::GPU_FEATURE_TYPE_GPU_RASTERIZATION] ==
         gpu::kGpuFeatureStatusEnabled;
}

bool GpuDataManagerImplPrivate::IsWebGLEnabled() const {
  return use_swiftshader_ ||
         !blacklisted_features_.count(gpu::GPU_FEATURE_TYPE_ACCELERATED_WEBGL);
}

bool GpuDataManagerImplPrivate::IsDriverBugWorkaroundActive(int feature) const {
  return (gpu_driver_bugs_.count(feature) == 1);
}

size_t GpuDataManagerImplPrivate::GetBlacklistedFeatureCount() const {
  // SwiftShader blacklists all features
  return use_swiftshader_ ? gpu::NUMBER_OF_GPU_FEATURE_TYPES
                          : blacklisted_features_.size();
}

gpu::GPUInfo GpuDataManagerImplPrivate::GetGPUInfo() const {
  return gpu_info_;
}

bool GpuDataManagerImplPrivate::GpuAccessAllowed(
    std::string* reason) const {
  if (use_swiftshader_)
    return true;

  if (!gpu_process_accessible_) {
    if (reason) {
      *reason = "GPU process launch failed.";
    }
    return false;
  }

  if (in_process_gpu_)
    return true;

  if (card_blacklisted_) {
    if (reason) {
      *reason = "GPU access is disabled ";
      base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
      if (command_line->HasSwitch(switches::kDisableGpu))
        *reason += "through commandline switch --disable-gpu.";
      else
        *reason += "in chrome://settings.";
    }
    return false;
  }

  if (preliminary_blacklisted_features_initialized_) {
    // We only need to block GPU process if more features are disallowed other
    // than those in the preliminary gpu feature flags because the latter work
    // through renderer commandline switches. WebGL and WebGL2 should not matter
    // because their context creation can always be rejected on the GPU process
    // side.
    std::set<int> feature_diffs;
    std::set_difference(blacklisted_features_.begin(),
                        blacklisted_features_.end(),
                        preliminary_blacklisted_features_.begin(),
                        preliminary_blacklisted_features_.end(),
                        std::inserter(feature_diffs, feature_diffs.begin()));
    if (feature_diffs.size()) {
      // TODO(zmo): Other features might also be OK to ignore here.
      feature_diffs.erase(gpu::GPU_FEATURE_TYPE_ACCELERATED_WEBGL);
      feature_diffs.erase(gpu::GPU_FEATURE_TYPE_WEBGL2);
      feature_diffs.erase(gpu::GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS);
    }
    if (feature_diffs.size()) {
      if (reason) {
        *reason = "Features are disabled on full but not preliminary GPU info.";
      }
      return false;
    }
  }

  if (blacklisted_features_.size() == gpu::NUMBER_OF_GPU_FEATURE_TYPES) {
    // On Linux, we use cached GL strings to make blacklist decsions at browser
    // startup time. We need to launch the GPU process to validate these
    // strings even if all features are blacklisted. If all GPU features are
    // disabled, the GPU process will only initialize GL bindings, create a GL
    // context, and collect full GPU info.
#if !defined(OS_LINUX)
    if (reason) {
      *reason = "All GPU features are blacklisted.";
    }
    return false;
#endif
  }

  return true;
}

void GpuDataManagerImplPrivate::RequestCompleteGpuInfoIfNeeded() {
  if (complete_gpu_info_already_requested_ || IsCompleteGpuInfoAvailable() ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kGpuTestingNoCompleteInfoCollection)) {
    return;
  }

  complete_gpu_info_already_requested_ = true;

  GpuProcessHost::CallOnIO(
#if defined(OS_WIN)
      GpuProcessHost::GPU_PROCESS_KIND_UNSANDBOXED,
#else
      GpuProcessHost::GPU_PROCESS_KIND_SANDBOXED,
#endif
      true /* force_create */, base::Bind([](GpuProcessHost* host) {
        if (!host)
          return;
        host->gpu_service()->RequestCompleteGpuInfo(
            base::Bind(&UpdateGpuInfoOnIO));
      }));
}

bool GpuDataManagerImplPrivate::IsEssentialGpuInfoAvailable() const {
  return (gpu_info_.basic_info_state != gpu::kCollectInfoNone &&
          gpu_info_.context_info_state != gpu::kCollectInfoNone) ||
         use_swiftshader_;
}

bool GpuDataManagerImplPrivate::IsCompleteGpuInfoAvailable() const {
#if defined(OS_WIN)
  if (gpu_info_.dx_diagnostics_info_state == gpu::kCollectInfoNone)
    return false;
#endif
  return IsEssentialGpuInfoAvailable();
}

void GpuDataManagerImplPrivate::RequestVideoMemoryUsageStatsUpdate(
    const base::Callback<void(const gpu::VideoMemoryUsageStats& stats)>&
        callback) const {
  GpuProcessHost::CallOnIO(GpuProcessHost::GPU_PROCESS_KIND_SANDBOXED,
                           false /* force_create */,
                           base::Bind(&RequestVideoMemoryUsageStats, callback));
}

bool GpuDataManagerImplPrivate::ShouldUseSwiftShader() const {
  return use_swiftshader_;
}

void GpuDataManagerImplPrivate::AddObserver(GpuDataManagerObserver* observer) {
  GpuDataManagerImpl::UnlockedSession session(owner_);
  observer_list_->AddObserver(observer);
}

void GpuDataManagerImplPrivate::RemoveObserver(
    GpuDataManagerObserver* observer) {
  GpuDataManagerImpl::UnlockedSession session(owner_);
  observer_list_->RemoveObserver(observer);
}

void GpuDataManagerImplPrivate::UnblockDomainFrom3DAPIs(const GURL& url) {
  // This method must do two things:
  //
  //  1. If the specific domain is blocked, then unblock it.
  //
  //  2. Reset our notion of how many GPU resets have occurred recently.
  //     This is necessary even if the specific domain was blocked.
  //     Otherwise, if we call Are3DAPIsBlocked with the same domain right
  //     after unblocking it, it will probably still be blocked because of
  //     the recent GPU reset caused by that domain.
  //
  // These policies could be refined, but at a certain point the behavior
  // will become difficult to explain.
  std::string domain = GetDomainFromURL(url);

  blocked_domains_.erase(domain);
  timestamps_of_gpu_resets_.clear();
}

void GpuDataManagerImplPrivate::SetGLStrings(const std::string& gl_vendor,
                                             const std::string& gl_renderer,
                                             const std::string& gl_version) {
  if (gl_vendor.empty() && gl_renderer.empty() && gl_version.empty())
    return;

  if (!is_initialized_) {
    post_init_tasks_.push_back(
        base::Bind(&GpuDataManagerImplPrivate::SetGLStrings,
                   base::Unretained(this), gl_vendor, gl_renderer, gl_version));
    return;
  }

  // If GPUInfo already got GL strings, do nothing.  This is for the rare
  // situation where GPU process collected GL strings before this call.
  if (!gpu_info_.gl_vendor.empty() ||
      !gpu_info_.gl_renderer.empty() ||
      !gpu_info_.gl_version.empty())
    return;

  gpu::GPUInfo gpu_info = gpu_info_;

  gpu_info.gl_vendor = gl_vendor;
  gpu_info.gl_renderer = gl_renderer;
  gpu_info.gl_version = gl_version;

  gpu::IdentifyActiveGPU(&gpu_info);
  gpu::CollectDriverInfoGL(&gpu_info);

  UpdateGpuInfo(gpu_info);
  UpdateGpuSwitchingManager(gpu_info);
  UpdatePreliminaryBlacklistedFeatures();
}

void GpuDataManagerImplPrivate::GetGLStrings(std::string* gl_vendor,
                                             std::string* gl_renderer,
                                             std::string* gl_version) {
  DCHECK(gl_vendor && gl_renderer && gl_version);

  *gl_vendor = gpu_info_.gl_vendor;
  *gl_renderer = gpu_info_.gl_renderer;
  *gl_version = gpu_info_.gl_version;
}

void GpuDataManagerImplPrivate::Initialize() {
  TRACE_EVENT0("startup", "GpuDataManagerImpl::Initialize");
  if (finalized_) {
    DVLOG(0) << "GpuDataManagerImpl marked as finalized; skipping Initialize";
    return;
  }

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kSkipGpuDataLoading)) {
    RunPostInitTasks();
    return;
  }

  gpu::GPUInfo gpu_info;
  const char* softwareGLImplementationName =
      gl::GetGLImplementationName(gl::GetSoftwareGLImplementation());
  const bool force_software_gl =
      (command_line->GetSwitchValueASCII(switches::kUseGL) ==
       softwareGLImplementationName) ||
      command_line->HasSwitch(switches::kOverrideUseSoftwareGLForTests) ||
      command_line->HasSwitch(switches::kHeadless);
  if (force_software_gl) {
    // If using the OSMesa GL implementation, use fake vendor and device ids to
    // make sure it never gets blacklisted. This is better than simply
    // cancelling GPUInfo gathering as it allows us to proceed with loading the
    // blacklist below which may have non-device specific entries we want to
    // apply anyways (e.g., OS version blacklisting).
    gpu_info.gpu.vendor_id = 0xffff;
    gpu_info.gpu.device_id = 0xffff;

    // Also declare the driver_vendor to be <software GL> to be able to
    // specify exceptions based on driver_vendor==<software GL> for some
    // blacklist rules.
    gpu_info.driver_vendor = softwareGLImplementationName;

    // We are not going to call CollectBasicGraphicsInfo.
    // So mark it as collected.
    gpu_info.basic_info_state = gpu::kCollectInfoSuccess;
  } else {
    // Skip collecting the basic driver info if SetGpuInfo() is already called.
    if (IsCompleteGpuInfoAvailable()) {
      gpu_info = gpu_info_;
    } else {
      TRACE_EVENT0("startup",
                   "GpuDataManagerImpl::Initialize:CollectBasicGraphicsInfo");
      gpu::CollectBasicGraphicsInfo(&gpu_info);
    }

    if (command_line->HasSwitch(switches::kGpuTestingVendorId) &&
        command_line->HasSwitch(switches::kGpuTestingDeviceId)) {
      base::HexStringToUInt(
          command_line->GetSwitchValueASCII(switches::kGpuTestingVendorId),
          &gpu_info.gpu.vendor_id);
      base::HexStringToUInt(
          command_line->GetSwitchValueASCII(switches::kGpuTestingDeviceId),
          &gpu_info.gpu.device_id);
      gpu_info.gpu.active = true;
      gpu_info.secondary_gpus.clear();
    }

    gpu::ParseSecondaryGpuDevicesFromCommandLine(*command_line, &gpu_info);

    if (command_line->HasSwitch(switches::kGpuTestingDriverDate)) {
      gpu_info.driver_date =
          command_line->GetSwitchValueASCII(switches::kGpuTestingDriverDate);
    }
  }
#if defined(ARCH_CPU_X86_FAMILY)
  if (!gpu_info.gpu.vendor_id || !gpu_info.gpu.device_id) {
    gpu_info.context_info_state = gpu::kCollectInfoNonFatalFailure;
#if defined(OS_WIN)
    gpu_info.dx_diagnostics_info_state = gpu::kCollectInfoNonFatalFailure;
#endif  // OS_WIN
  }
#endif  // ARCH_CPU_X86_FAMILY

  gpu::GpuControlListData gpu_blacklist_data;
  if (!force_software_gl &&
      !command_line->HasSwitch(switches::kIgnoreGpuBlacklist) &&
      !command_line->HasSwitch(switches::kUseGpuInTests)) {
    gpu_blacklist_data = {gpu::kSoftwareRenderingListVersion,
                          gpu::kSoftwareRenderingListEntryCount,
                          gpu::kSoftwareRenderingListEntries};
  }
  gpu::GpuControlListData gpu_driver_bug_list_data;
  if (!force_software_gl &&
      !command_line->HasSwitch(switches::kDisableGpuDriverBugWorkarounds)) {
    gpu_driver_bug_list_data = {gpu::kGpuDriverBugListVersion,
                                gpu::kGpuDriverBugListEntryCount,
                                gpu::kGpuDriverBugListEntries};
  }
  InitializeImpl(gpu_blacklist_data, gpu_driver_bug_list_data, gpu_info);

  if (in_process_gpu_) {
    command_line->AppendSwitch(switches::kDisableGpuWatchdog);
    AppendGpuCommandLine(command_line);
  }
}

void GpuDataManagerImplPrivate::UpdateGpuInfoHelper() {
  GetContentClient()->SetGpuInfo(gpu_info_);

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  std::string os_version;
  if (command_line->HasSwitch(switches::kGpuTestingOsVersion))
    os_version =
        command_line->GetSwitchValueASCII(switches::kGpuTestingOsVersion);

  if (gpu_blacklist_) {
    std::set<int> features = gpu_blacklist_->MakeDecision(
        gpu::GpuControlList::kOsAny, os_version, gpu_info_);
    if (update_histograms_)
      UpdateStats(gpu_info_, gpu_blacklist_.get(), features);

    UpdateBlacklistedFeatures(features);
  }

  std::string command_line_disable_gl_extensions;
  std::vector<std::string> disabled_driver_bug_exts;
  // Holds references to strings in the above two variables.
  std::set<base::StringPiece> disabled_ext_set;

  // Merge disabled extensions from the command line with gpu driver bug list.
  if (command_line) {
    command_line_disable_gl_extensions =
        command_line->GetSwitchValueASCII(switches::kDisableGLExtensions);
    std::vector<base::StringPiece> disabled_command_line_exts =
        base::SplitStringPiece(command_line_disable_gl_extensions, ", ;",
                               base::KEEP_WHITESPACE,
                               base::SPLIT_WANT_NONEMPTY);
    disabled_ext_set.insert(disabled_command_line_exts.begin(),
                            disabled_command_line_exts.end());
  }

  if (gpu_driver_bug_list_) {
    gpu_driver_bugs_ = gpu_driver_bug_list_->MakeDecision(
        gpu::GpuControlList::kOsAny, os_version, gpu_info_);

    disabled_driver_bug_exts = gpu_driver_bug_list_->GetDisabledExtensions();
    disabled_ext_set.insert(disabled_driver_bug_exts.begin(),
                            disabled_driver_bug_exts.end());
    UpdateDriverBugListStats(gpu_driver_bug_list_.get(), gpu_driver_bugs_);
  }
  disabled_extensions_ =
      base::JoinString(std::vector<base::StringPiece>(disabled_ext_set.begin(),
                                                      disabled_ext_set.end()),
                       " ");

  gpu::GpuDriverBugList::AppendWorkaroundsFromCommandLine(
      &gpu_driver_bugs_, *base::CommandLine::ForCurrentProcess());

  // We have to update GpuFeatureType before notify all the observers.
  NotifyGpuInfoUpdate();
}

void GpuDataManagerImplPrivate::UpdateGpuInfo(const gpu::GPUInfo& gpu_info) {
  // No further update of gpu_info if falling back to SwiftShader.
  if (use_swiftshader_)
    return;

  bool was_info_available = IsCompleteGpuInfoAvailable();
  gpu::MergeGPUInfo(&gpu_info_, gpu_info);
  if (IsCompleteGpuInfoAvailable()) {
    complete_gpu_info_already_requested_ = true;
  } else if (was_info_available) {
    // Allow future requests to go through properly.
    complete_gpu_info_already_requested_ = false;
  }

  UpdateGpuInfoHelper();
}

void GpuDataManagerImplPrivate::UpdateGpuFeatureInfo(
    const gpu::GpuFeatureInfo& gpu_feature_info) {
  if (!use_swiftshader_) {
    gpu_feature_info_ = gpu_feature_info;
  }
}

void GpuDataManagerImplPrivate::AppendRendererCommandLine(
    base::CommandLine* command_line) const {
  DCHECK(command_line);

  if (ShouldDisableAcceleratedVideoDecode(command_line))
    command_line->AppendSwitch(switches::kDisableAcceleratedVideoDecode);

#if defined(USE_AURA)
  if (!CanUseGpuBrowserCompositor())
    command_line->AppendSwitch(switches::kDisableGpuCompositing);
#endif

  if (IsGpuSchedulerEnabled())
    command_line->AppendSwitch(switches::kEnableGpuAsyncWorkerContext);
}

void GpuDataManagerImplPrivate::AppendGpuCommandLine(
    base::CommandLine* command_line) const {
  DCHECK(command_line);

  std::string use_gl =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kUseGL);
  if (gpu_driver_bugs_.find(gpu::DISABLE_D3D11) != gpu_driver_bugs_.end())
    command_line->AppendSwitch(switches::kDisableD3D11);
  if (gpu_driver_bugs_.find(gpu::DISABLE_ES3_GL_CONTEXT) !=
      gpu_driver_bugs_.end()) {
    command_line->AppendSwitch(switches::kDisableES3GLContext);
  }
  if (gpu_driver_bugs_.find(gpu::DISABLE_DIRECT_COMPOSITION) !=
      gpu_driver_bugs_.end()) {
    command_line->AppendSwitch(switches::kDisableDirectComposition);
  }
  if (use_swiftshader_) {
    command_line->AppendSwitchASCII(
        switches::kUseGL, gl::kGLImplementationSwiftShaderForWebGLName);
  } else if ((IsFeatureBlacklisted(gpu::GPU_FEATURE_TYPE_ACCELERATED_WEBGL) ||
              IsFeatureBlacklisted(gpu::GPU_FEATURE_TYPE_GPU_COMPOSITING) ||
              IsFeatureBlacklisted(
                  gpu::GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS)) &&
             (use_gl == "any")) {
    command_line->AppendSwitchASCII(
        switches::kUseGL,
        gl::GetGLImplementationName(gl::GetSoftwareGLImplementation()));
  } else if (!use_gl.empty()) {
    command_line->AppendSwitchASCII(switches::kUseGL, use_gl);
  }
  if (ui::GpuSwitchingManager::GetInstance()->SupportsDualGpus())
    command_line->AppendSwitchASCII(switches::kSupportsDualGpus, "true");
  else
    command_line->AppendSwitchASCII(switches::kSupportsDualGpus, "false");

  if (!gpu_driver_bugs_.empty()) {
    command_line->AppendSwitchASCII(switches::kGpuDriverBugWorkarounds,
                                    IntSetToString(gpu_driver_bugs_));
  }

  if (!disabled_extensions_.empty()) {
    command_line->AppendSwitchASCII(switches::kDisableGLExtensions,
                                    disabled_extensions_);
  }

  if (ShouldDisableAcceleratedVideoDecode(command_line)) {
    command_line->AppendSwitch(switches::kDisableAcceleratedVideoDecode);
  }

  if (gpu_driver_bugs_.find(gpu::CREATE_DEFAULT_GL_CONTEXT) !=
      gpu_driver_bugs_.end()) {
    command_line->AppendSwitch(switches::kCreateDefaultGLContext);
  }

#if defined(USE_OZONE)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableDrmAtomic)) {
    command_line->AppendSwitch(switches::kEnableDrmAtomic);
  }
#endif

  // Pass GPU and driver information to GPU process. We try to avoid full GPU
  // info collection at GPU process startup, but we need gpu vendor_id,
  // device_id, driver_vendor, driver_version for deciding whether we need to
  // collect full info (on Linux) and for crash reporting purpose.
  command_line->AppendSwitchASCII(switches::kGpuVendorID,
      base::StringPrintf("0x%04x", gpu_info_.gpu.vendor_id));
  command_line->AppendSwitchASCII(switches::kGpuDeviceID,
      base::StringPrintf("0x%04x", gpu_info_.gpu.device_id));
  command_line->AppendSwitchASCII(switches::kGpuDriverVendor,
      gpu_info_.driver_vendor);
  command_line->AppendSwitchASCII(switches::kGpuDriverVersion,
      gpu_info_.driver_version);
  command_line->AppendSwitchASCII(switches::kGpuDriverDate,
      gpu_info_.driver_date);

  gpu::GPUInfo::GPUDevice maybe_active_gpu_device;
  if (gpu_info_.gpu.active)
    maybe_active_gpu_device = gpu_info_.gpu;

  std::string vendor_ids_str;
  std::string device_ids_str;
  for (const auto& device : gpu_info_.secondary_gpus) {
    if (!vendor_ids_str.empty())
      vendor_ids_str += ";";
    if (!device_ids_str.empty())
      device_ids_str += ";";
    vendor_ids_str += base::StringPrintf("0x%04x", device.vendor_id);
    device_ids_str += base::StringPrintf("0x%04x", device.device_id);

    if (device.active)
      maybe_active_gpu_device = device;
  }

  if (!vendor_ids_str.empty() && !device_ids_str.empty()) {
    command_line->AppendSwitchASCII(switches::kGpuSecondaryVendorIDs,
                                    vendor_ids_str);
    command_line->AppendSwitchASCII(switches::kGpuSecondaryDeviceIDs,
                                    device_ids_str);
  }

  if (maybe_active_gpu_device.active) {
    command_line->AppendSwitchASCII(
        switches::kGpuActiveVendorID,
        base::StringPrintf("0x%04x", maybe_active_gpu_device.vendor_id));
    command_line->AppendSwitchASCII(
        switches::kGpuActiveDeviceID,
        base::StringPrintf("0x%04x", maybe_active_gpu_device.device_id));
  }
}

void GpuDataManagerImplPrivate::UpdateRendererWebPrefs(
    WebPreferences* prefs) const {
  DCHECK(prefs);

  if (!IsWebGLEnabled()) {
    prefs->experimental_webgl_enabled = false;
    prefs->pepper_3d_enabled = false;
  }
  if (IsFeatureBlacklisted(gpu::GPU_FEATURE_TYPE_FLASH3D))
    prefs->flash_3d_enabled = false;
  if (IsFeatureBlacklisted(gpu::GPU_FEATURE_TYPE_FLASH_STAGE3D)) {
    prefs->flash_stage3d_enabled = false;
    prefs->flash_stage3d_baseline_enabled = false;
  }
  if (IsFeatureBlacklisted(gpu::GPU_FEATURE_TYPE_FLASH_STAGE3D_BASELINE))
    prefs->flash_stage3d_baseline_enabled = false;
  if (IsFeatureBlacklisted(gpu::GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS))
    prefs->accelerated_2d_canvas_enabled = false;

#if defined(USE_AURA)
  if (!CanUseGpuBrowserCompositor()) {
    prefs->accelerated_2d_canvas_enabled = false;
    prefs->pepper_3d_enabled = false;
  }
#endif

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (!ShouldDisableAcceleratedVideoDecode(command_line) &&
      !command_line->HasSwitch(switches::kDisableAcceleratedVideoDecode)) {
    prefs->pepper_accelerated_video_decode_enabled = true;
  }
  prefs->disable_2d_canvas_copy_on_write =
      IsDriverBugWorkaroundActive(gpu::BROKEN_EGL_IMAGE_REF_COUNTING) &&
      command_line->HasSwitch(switches::kEnableThreadedTextureMailboxes);
}

void GpuDataManagerImplPrivate::UpdateGpuPreferences(
    gpu::GpuPreferences* gpu_preferences) const {
  DCHECK(gpu_preferences);

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  if (IsGpuSchedulerEnabled())
    gpu_preferences->enable_gpu_scheduler = true;

  if (ShouldDisableAcceleratedVideoDecode(command_line))
    gpu_preferences->disable_accelerated_video_decode = true;

  gpu_preferences->enable_es3_apis =
      (command_line->HasSwitch(switches::kEnableES3APIs) ||
       !IsFeatureBlacklisted(gpu::GPU_FEATURE_TYPE_WEBGL2)) &&
      !command_line->HasSwitch(switches::kDisableES3APIs);
}

void GpuDataManagerImplPrivate::DisableHardwareAcceleration() {
  if (!is_initialized_) {
    post_init_tasks_.push_back(
        base::Bind(&GpuDataManagerImplPrivate::DisableHardwareAcceleration,
                   base::Unretained(this)));
    return;
  }

  card_blacklisted_ = true;

  for (int i = 0; i < gpu::NUMBER_OF_GPU_FEATURE_TYPES; ++i)
    blacklisted_features_.insert(i);

  EnableSwiftShaderIfNecessary();
  NotifyGpuInfoUpdate();
}

void GpuDataManagerImplPrivate::SetGpuInfo(const gpu::GPUInfo& gpu_info) {
  DCHECK(!is_initialized_);
  gpu_info_ = gpu_info;
  DCHECK(IsCompleteGpuInfoAvailable());
}

std::string GpuDataManagerImplPrivate::GetBlacklistVersion() const {
  if (gpu_blacklist_)
    return gpu_blacklist_->version();
  return "0";
}

std::string GpuDataManagerImplPrivate::GetDriverBugListVersion() const {
  if (gpu_driver_bug_list_)
    return gpu_driver_bug_list_->version();
  return "0";
}

void GpuDataManagerImplPrivate::GetBlacklistReasons(
    base::ListValue* reasons) const {
  if (gpu_blacklist_)
    gpu_blacklist_->GetReasons(reasons, "disabledFeatures");
  if (gpu_driver_bug_list_)
    gpu_driver_bug_list_->GetReasons(reasons, "workarounds");
}

std::vector<std::string>
GpuDataManagerImplPrivate::GetDriverBugWorkarounds() const {
  std::vector<std::string> workarounds;
  for (std::set<int>::const_iterator it = gpu_driver_bugs_.begin();
       it != gpu_driver_bugs_.end(); ++it) {
    workarounds.push_back(
        gpu::GpuDriverBugWorkaroundTypeToString(
            static_cast<gpu::GpuDriverBugWorkaroundType>(*it)));
  }
  return workarounds;
}

void GpuDataManagerImplPrivate::AddLogMessage(
    int level, const std::string& header, const std::string& message) {
  log_messages_.push_back(LogMessage(level, header, message));
}

void GpuDataManagerImplPrivate::ProcessCrashed(
    base::TerminationStatus exit_code) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    // Unretained is ok, because it's posted to UI thread, the thread
    // where the singleton GpuDataManagerImpl lives until the end.
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&GpuDataManagerImpl::ProcessCrashed,
                   base::Unretained(owner_),
                   exit_code));
    return;
  }
  {
    gpu_info_.process_crash_count = GpuProcessHost::gpu_crash_count();
    GpuDataManagerImpl::UnlockedSession session(owner_);
    observer_list_->Notify(
        FROM_HERE, &GpuDataManagerObserver::OnGpuProcessCrashed, exit_code);
  }
}

std::unique_ptr<base::ListValue> GpuDataManagerImplPrivate::GetLogMessages()
    const {
  auto value = base::MakeUnique<base::ListValue>();
  for (size_t ii = 0; ii < log_messages_.size(); ++ii) {
    std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
    dict->SetInteger("level", log_messages_[ii].level);
    dict->SetString("header", log_messages_[ii].header);
    dict->SetString("message", log_messages_[ii].message);
    value->Append(std::move(dict));
  }
  return value;
}

void GpuDataManagerImplPrivate::HandleGpuSwitch() {
  GpuDataManagerImpl::UnlockedSession session(owner_);
  // Notify observers in the browser process.
  ui::GpuSwitchingManager::GetInstance()->NotifyGpuSwitched();
  // Pass the notification to the GPU process to notify observers there.
  GpuProcessHost::CallOnIO(GpuProcessHost::GPU_PROCESS_KIND_SANDBOXED,
                           false /* force_create */,
                           base::Bind([](GpuProcessHost* host) {
                             if (host)
                               host->gpu_service()->GpuSwitched();
                           }));
}

bool GpuDataManagerImplPrivate::UpdateActiveGpu(uint32_t vendor_id,
                                                uint32_t device_id) {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  // For tests, only the gpu process is allowed to detect the active gpu device
  // using information on the actual loaded GL driver.
  if (command_line->HasSwitch(switches::kGpuTestingVendorId) &&
      command_line->HasSwitch(switches::kGpuTestingDeviceId)) {
    return false;
  }

  if (gpu_info_.gpu.vendor_id == vendor_id &&
      gpu_info_.gpu.device_id == device_id) {
    // The primary GPU is active.
    if (gpu_info_.gpu.active)
      return false;
    gpu_info_.gpu.active = true;
    for (size_t ii = 0; ii < gpu_info_.secondary_gpus.size(); ++ii)
      gpu_info_.secondary_gpus[ii].active = false;
  } else {
    // A secondary GPU is active.
    for (size_t ii = 0; ii < gpu_info_.secondary_gpus.size(); ++ii) {
      if (gpu_info_.secondary_gpus[ii].vendor_id == vendor_id &&
          gpu_info_.secondary_gpus[ii].device_id == device_id) {
        if (gpu_info_.secondary_gpus[ii].active)
          return false;
        gpu_info_.secondary_gpus[ii].active = true;
      } else {
        gpu_info_.secondary_gpus[ii].active = false;
      }
    }
    gpu_info_.gpu.active = false;
  }
  UpdateGpuInfoHelper();
  return true;
}

bool GpuDataManagerImplPrivate::IsGpuSchedulerEnabled() const {
  return base::FeatureList::IsEnabled(features::kGpuScheduler);
}

bool GpuDataManagerImplPrivate::CanUseGpuBrowserCompositor() const {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableGpuCompositing))
    return false;
  if (ShouldUseSwiftShader())
    return false;
  if (IsFeatureBlacklisted(gpu::GPU_FEATURE_TYPE_GPU_COMPOSITING))
    return false;
  return true;
}

bool GpuDataManagerImplPrivate::ShouldDisableAcceleratedVideoDecode(
    const base::CommandLine* command_line) const {
  // Make sure that we initialize the experiment first to make sure that
  // statistics are bucket correctly in all cases.
  // This experiment is temporary and will be removed once enough data
  // to resolve crbug/442039 has been collected.
  const std::string group_name = base::FieldTrialList::FindFullName(
      "DisableAcceleratedVideoDecode");
  if (command_line->HasSwitch(switches::kDisableAcceleratedVideoDecode)) {
    // It was already disabled on the command line.
    return false;
  }
  if (IsFeatureBlacklisted(gpu::GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE))
    return true;
  if (group_name == "Disabled")
    return true;

  // Accelerated decode is never available with --disable-gpu.
  return ShouldDisableHardwareAcceleration();
}

void GpuDataManagerImplPrivate::GetDisabledExtensions(
    std::string* disabled_extensions) const {
  DCHECK(disabled_extensions);
  *disabled_extensions = disabled_extensions_;
}

void GpuDataManagerImplPrivate::BlockDomainFrom3DAPIs(
    const GURL& url, GpuDataManagerImpl::DomainGuilt guilt) {
  BlockDomainFrom3DAPIsAtTime(url, guilt, base::Time::Now());
}

bool GpuDataManagerImplPrivate::Are3DAPIsBlocked(const GURL& top_origin_url,
                                                 int render_process_id,
                                                 int render_frame_id,
                                                 ThreeDAPIType requester) {
  bool blocked = Are3DAPIsBlockedAtTime(top_origin_url, base::Time::Now()) !=
      GpuDataManagerImpl::DOMAIN_BLOCK_STATUS_NOT_BLOCKED;
  if (blocked) {
    // Unretained is ok, because it's posted to UI thread, the thread
    // where the singleton GpuDataManagerImpl lives until the end.
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&GpuDataManagerImpl::Notify3DAPIBlocked,
                   base::Unretained(owner_), top_origin_url, render_process_id,
                   render_frame_id, requester));
  }

  return blocked;
}

void GpuDataManagerImplPrivate::DisableDomainBlockingFor3DAPIsForTesting() {
  domain_blocking_enabled_ = false;
}

// static
GpuDataManagerImplPrivate* GpuDataManagerImplPrivate::Create(
    GpuDataManagerImpl* owner) {
  return new GpuDataManagerImplPrivate(owner);
}

GpuDataManagerImplPrivate::GpuDataManagerImplPrivate(GpuDataManagerImpl* owner)
    : complete_gpu_info_already_requested_(false),
      preliminary_blacklisted_features_initialized_(false),
      observer_list_(new GpuDataManagerObserverList),
      use_swiftshader_(false),
      card_blacklisted_(false),
      update_histograms_(true),
      domain_blocking_enabled_(true),
      owner_(owner),
      gpu_process_accessible_(true),
      is_initialized_(false),
      finalized_(false),
      in_process_gpu_(false) {
  DCHECK(owner_);
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (ShouldDisableHardwareAcceleration())
    DisableHardwareAcceleration();

  if (command_line->HasSwitch(switches::kSingleProcess) ||
      command_line->HasSwitch(switches::kInProcessGPU)) {
    in_process_gpu_ = true;
  }

#if defined(OS_MACOSX)
  CGDisplayRegisterReconfigurationCallback(DisplayReconfigCallback, owner_);
#endif  // OS_MACOSX

  // For testing only.
  if (command_line->HasSwitch(switches::kDisableDomainBlockingFor3DAPIs)) {
    domain_blocking_enabled_ = false;
  }
}

GpuDataManagerImplPrivate::~GpuDataManagerImplPrivate() {
#if defined(OS_MACOSX)
  CGDisplayRemoveReconfigurationCallback(DisplayReconfigCallback, owner_);
#endif
}

void GpuDataManagerImplPrivate::InitializeImpl(
    const gpu::GpuControlListData& gpu_blacklist_data,
    const gpu::GpuControlListData& gpu_driver_bug_list_data,
    const gpu::GPUInfo& gpu_info) {
  const bool log_gpu_control_list_decisions =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kLogGpuControlListDecisions);

  if (gpu_blacklist_data.entry_count) {
    gpu_blacklist_ = gpu::GpuBlacklist::Create(gpu_blacklist_data);
    if (log_gpu_control_list_decisions)
      gpu_blacklist_->EnableControlListLogging("gpu_blacklist");
  }
  if (gpu_driver_bug_list_data.entry_count) {
    gpu_driver_bug_list_ =
        gpu::GpuDriverBugList::Create(gpu_driver_bug_list_data);
    if (log_gpu_control_list_decisions)
      gpu_driver_bug_list_->EnableControlListLogging("gpu_driver_bug_list");
  }

  gpu_info_ = gpu_info;
  UpdateGpuInfo(gpu_info);
  UpdateGpuSwitchingManager(gpu_info);
  UpdatePreliminaryBlacklistedFeatures();

  RunPostInitTasks();
}

void GpuDataManagerImplPrivate::RunPostInitTasks() {
  // Set initialized before running callbacks.
  is_initialized_ = true;

  for (const auto& callback : post_init_tasks_)
    callback.Run();
  post_init_tasks_.clear();
}

void GpuDataManagerImplPrivate::UpdateBlacklistedFeatures(
    const std::set<int>& features) {
  blacklisted_features_ = features;

  // Force disable using the GPU for these features, even if they would
  // otherwise be allowed.
  if (card_blacklisted_) {
    blacklisted_features_.insert(gpu::GPU_FEATURE_TYPE_GPU_COMPOSITING);
    blacklisted_features_.insert(gpu::GPU_FEATURE_TYPE_ACCELERATED_WEBGL);
    blacklisted_features_.insert(gpu::GPU_FEATURE_TYPE_WEBGL2);
  }

  EnableSwiftShaderIfNecessary();
}

void GpuDataManagerImplPrivate::UpdatePreliminaryBlacklistedFeatures() {
  preliminary_blacklisted_features_ = blacklisted_features_;
  preliminary_blacklisted_features_initialized_ = true;
}

void GpuDataManagerImplPrivate::UpdateGpuSwitchingManager(
    const gpu::GPUInfo& gpu_info) {
  // The vendor IDs might be 0 on non-PCI devices (like Android), but
  // the length of the vector is all we care about in most cases.
  std::vector<uint32_t> vendor_ids;
  vendor_ids.push_back(gpu_info.gpu.vendor_id);
  for (const auto& device : gpu_info.secondary_gpus) {
    vendor_ids.push_back(device.vendor_id);
  }
  ui::GpuSwitchingManager::GetInstance()->SetGpuVendorIds(vendor_ids);
  gpu::InitializeDualGpusIfSupported(gpu_driver_bugs_);
}

void GpuDataManagerImplPrivate::NotifyGpuInfoUpdate() {
  observer_list_->Notify(FROM_HERE, &GpuDataManagerObserver::OnGpuInfoUpdate);
}

void GpuDataManagerImplPrivate::EnableSwiftShaderIfNecessary() {
#if BUILDFLAG(ENABLE_SWIFTSHADER)
  if ((!GpuAccessAllowed(nullptr) ||
       blacklisted_features_.count(gpu::GPU_FEATURE_TYPE_ACCELERATED_WEBGL)) &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableSoftwareRasterizer)) {
    use_swiftshader_ = true;

    // Adjust GPU info for SwiftShader. Most of this data only affects the
    // chrome://gpu page, so it doesn't require querying the implementation.
    // It is important to reset the video and image decode/encode capabilities,
    // to prevent attempts to use them (crbug.com/702417).
    gpu_info_.driver_vendor = "Google Inc.";
    gpu_info_.driver_version = "3.3.0.2";
    gpu_info_.driver_date = "2017/04/07";
    gpu_info_.pixel_shader_version = "3.0";
    gpu_info_.vertex_shader_version = "3.0";
    gpu_info_.max_msaa_samples = "4";
    gpu_info_.gl_version = "OpenGL ES 2.0 SwiftShader";
    gpu_info_.gl_vendor = "Google Inc.";
    gpu_info_.gl_renderer = "Google SwiftShader";
    gpu_info_.gl_extensions = "";
    gpu_info_.gl_reset_notification_strategy = 0;
    gpu_info_.software_rendering = true;
    gpu_info_.passthrough_cmd_decoder = false;
    gpu_info_.supports_overlays = false;
    gpu_info_.basic_info_state = gpu::kCollectInfoSuccess;
    gpu_info_.context_info_state = gpu::kCollectInfoSuccess;
    gpu_info_.video_decode_accelerator_capabilities = {};
    gpu_info_.video_encode_accelerator_supported_profiles = {};
    gpu_info_.jpeg_decode_accelerator_supported = false;

    gpu_info_.gpu.active = false;
    for (auto& secondary_gpu : gpu_info_.secondary_gpus)
      secondary_gpu.active = false;

    for (auto& status : gpu_feature_info_.status_values)
      status = gpu::kGpuFeatureStatusBlacklisted;
  }
#endif
}

std::string GpuDataManagerImplPrivate::GetDomainFromURL(
    const GURL& url) const {
  // For the moment, we just use the host, or its IP address, as the
  // entry in the set, rather than trying to figure out the top-level
  // domain. This does mean that a.foo.com and b.foo.com will be
  // treated independently in the blocking of a given domain, but it
  // would require a third-party library to reliably figure out the
  // top-level domain from a URL.
  if (!url.has_host()) {
    return std::string();
  }

  return url.host();
}

void GpuDataManagerImplPrivate::BlockDomainFrom3DAPIsAtTime(
    const GURL& url,
    GpuDataManagerImpl::DomainGuilt guilt,
    base::Time at_time) {
  if (!domain_blocking_enabled_)
    return;

  std::string domain = GetDomainFromURL(url);

  DomainBlockEntry& entry = blocked_domains_[domain];
  entry.last_guilt = guilt;
  timestamps_of_gpu_resets_.push_back(at_time);
}

GpuDataManagerImpl::DomainBlockStatus
GpuDataManagerImplPrivate::Are3DAPIsBlockedAtTime(
    const GURL& url, base::Time at_time) const {
  if (!domain_blocking_enabled_)
    return GpuDataManagerImpl::DOMAIN_BLOCK_STATUS_NOT_BLOCKED;

  // Note: adjusting the policies in this code will almost certainly
  // require adjusting the associated unit tests.
  std::string domain = GetDomainFromURL(url);

  DomainBlockMap::const_iterator iter = blocked_domains_.find(domain);
  if (iter != blocked_domains_.end()) {
    // Err on the side of caution, and assume that if a particular
    // domain shows up in the block map, it's there for a good
    // reason and don't let its presence there automatically expire.
    return GpuDataManagerImpl::DOMAIN_BLOCK_STATUS_BLOCKED;
  }

  // Look at the timestamps of the recent GPU resets to see if there are
  // enough within the threshold which would cause us to blacklist all
  // domains. This doesn't need to be overly precise -- if time goes
  // backward due to a system clock adjustment, that's fine.
  //
  // TODO(kbr): make this pay attention to the TDR thresholds in the
  // Windows registry, but make sure it continues to be testable.
  {
    std::list<base::Time>::iterator iter = timestamps_of_gpu_resets_.begin();
    int num_resets_within_timeframe = 0;
    while (iter != timestamps_of_gpu_resets_.end()) {
      base::Time time = *iter;
      base::TimeDelta delta_t = at_time - time;

      // If this entry has "expired", just remove it.
      if (delta_t.InMilliseconds() > kBlockAllDomainsMs) {
        iter = timestamps_of_gpu_resets_.erase(iter);
        continue;
      }

      ++num_resets_within_timeframe;
      ++iter;
    }

    if (num_resets_within_timeframe >= kNumResetsWithinDuration) {
      UMA_HISTOGRAM_ENUMERATION("GPU.BlockStatusForClient3DAPIs",
                                BLOCK_STATUS_ALL_DOMAINS_BLOCKED,
                                BLOCK_STATUS_MAX);

      return GpuDataManagerImpl::DOMAIN_BLOCK_STATUS_ALL_DOMAINS_BLOCKED;
    }
  }

  UMA_HISTOGRAM_ENUMERATION("GPU.BlockStatusForClient3DAPIs",
                            BLOCK_STATUS_NOT_BLOCKED,
                            BLOCK_STATUS_MAX);

  return GpuDataManagerImpl::DOMAIN_BLOCK_STATUS_NOT_BLOCKED;
}

int64_t GpuDataManagerImplPrivate::GetBlockAllDomainsDurationInMs() const {
  return kBlockAllDomainsMs;
}

void GpuDataManagerImplPrivate::Notify3DAPIBlocked(const GURL& top_origin_url,
                                                   int render_process_id,
                                                   int render_frame_id,
                                                   ThreeDAPIType requester) {
  GpuDataManagerImpl::UnlockedSession session(owner_);
  observer_list_->Notify(FROM_HERE, &GpuDataManagerObserver::DidBlock3DAPIs,
                         top_origin_url, render_process_id, render_frame_id,
                         requester);
}

void GpuDataManagerImplPrivate::OnGpuProcessInitFailure() {
  gpu_process_accessible_ = false;
  gpu_info_.context_info_state = gpu::kCollectInfoFatalFailure;
#if defined(OS_WIN)
  gpu_info_.dx_diagnostics_info_state = gpu::kCollectInfoFatalFailure;
#endif
  complete_gpu_info_already_requested_ = true;
  // Some observers might be waiting.
  NotifyGpuInfoUpdate();
}

}  // namespace content
