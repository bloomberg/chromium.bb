// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/gpu_data_manager.h"

#if defined(OS_MACOSX)
#include <CoreGraphics/CGDisplayConfiguration.h>
#endif

#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/stringprintf.h"
#include "base/string_number_conversions.h"
#include "base/sys_info.h"
#include "base/values.h"
#include "base/version.h"
#include "content/browser/browser_thread.h"
#include "content/browser/gpu/gpu_blacklist.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/common/content_client.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/gpu/gpu_info_collector.h"
#include "content/public/common/content_switches.h"
#include "ui/gfx/gl/gl_implementation.h"
#include "ui/gfx/gl/gl_switches.h"
#include "webkit/plugins/plugin_switches.h"

namespace {

enum GpuFeatureStatus {
    kGpuFeatureEnabled = 0,
    kGpuFeatureBlacklisted = 1,
    kGpuFeatureDisabled = 2,  // disabled by user but not blacklisted
    kGpuFeatureNumStatus
};

struct GpuFeatureInfo {
  std::string name;
  uint32 blocked;
  bool disabled;
  std::string disabled_description;
  bool fallback_to_software;
};

#if defined(OS_MACOSX)
void DisplayReconfigCallback(CGDirectDisplayID display,
                             CGDisplayChangeSummaryFlags flags,
                             void* gpu_data_manager) {
  // TODO(zmo): this logging is temporary for crbug 88008 and will be removed.
  LOG(INFO) << "Display re-configuration: flags = 0x"
            << base::StringPrintf("%04x", flags);
  if (flags & kCGDisplayAddFlag) {
    GpuDataManager* manager =
        reinterpret_cast<GpuDataManager*>(gpu_data_manager);
    DCHECK(manager);
    manager->HandleGpuSwitch();
  }
}
#endif

DictionaryValue* NewDescriptionValuePair(const std::string& desc,
    const std::string& value) {
  DictionaryValue* dict = new DictionaryValue();
  dict->SetString("description", desc);
  dict->SetString("value", value);
  return dict;
}

DictionaryValue* NewDescriptionValuePair(const std::string& desc,
    Value* value) {
  DictionaryValue* dict = new DictionaryValue();
  dict->SetString("description", desc);
  dict->Set("value", value);
  return dict;
}

Value* NewStatusValue(const char* name, const char* status) {
  DictionaryValue* value = new DictionaryValue();
  value->SetString("name", name);
  value->SetString("status", status);
  return value;
}

#if defined(OS_WIN)
enum WinSubVersion {
  kWinOthers = 0,
  kWinXP,
  kWinVista,
  kWin7,
  kNumWinSubVersions
};

// Output DxDiagNode tree as nested array of {description,value} pairs
ListValue* DxDiagNodeToList(const DxDiagNode& node) {
  ListValue* list = new ListValue();
  for (std::map<std::string, std::string>::const_iterator it =
      node.values.begin();
      it != node.values.end();
      ++it) {
    list->Append(NewDescriptionValuePair(it->first, it->second));
  }

  for (std::map<std::string, DxDiagNode>::const_iterator it =
      node.children.begin();
      it != node.children.end();
      ++it) {
    ListValue* sublist = DxDiagNodeToList(it->second);
    list->Append(NewDescriptionValuePair(it->first, sublist));
  }
  return list;
}

int GetGpuBlacklistHistogramValueWin(GpuFeatureStatus status) {
  static WinSubVersion sub_version = kNumWinSubVersions;
  if (sub_version == kNumWinSubVersions) {
    sub_version = kWinOthers;
    std::string version_str = base::SysInfo::OperatingSystemVersion();
    size_t pos = version_str.find_first_not_of("0123456789.");
    if (pos != std::string::npos)
      version_str = version_str.substr(0, pos);
    scoped_ptr<Version> os_version(Version::GetVersionFromString(version_str));
    if (os_version.get() && os_version->components().size() >= 2) {
      const std::vector<uint16>& version_numbers = os_version->components();
      if (version_numbers[0] == 5)
        sub_version = kWinXP;
      else if (version_numbers[0] == 6 && version_numbers[1] == 0)
        sub_version = kWinVista;
      else if (version_numbers[0] == 6 && version_numbers[1] == 1)
        sub_version = kWin7;
    }
  }
  int entry_index = static_cast<int>(sub_version) * kGpuFeatureNumStatus;
  switch (status) {
    case kGpuFeatureEnabled:
      break;
    case kGpuFeatureBlacklisted:
      entry_index++;
      break;
    case kGpuFeatureDisabled:
      entry_index += 2;
      break;
  }
  return entry_index;
}
#endif  // OS_WIN

}  // namespace anonymous

GpuDataManager::UserFlags::UserFlags()
    : disable_accelerated_2d_canvas_(false),
      disable_accelerated_compositing_(false),
      disable_accelerated_layers_(false),
      disable_experimental_webgl_(false),
      disable_gl_multisampling_(false),
      ignore_gpu_blacklist_(false) {
}

void GpuDataManager::UserFlags::Initialize() {
  const CommandLine& browser_command_line =
      *CommandLine::ForCurrentProcess();

  disable_accelerated_2d_canvas_ = browser_command_line.HasSwitch(
      switches::kDisableAccelerated2dCanvas);
  disable_accelerated_compositing_ = browser_command_line.HasSwitch(
      switches::kDisableAcceleratedCompositing);
  disable_accelerated_layers_ = browser_command_line.HasSwitch(
      switches::kDisableAcceleratedLayers);
  disable_experimental_webgl_ = browser_command_line.HasSwitch(
      switches::kDisableExperimentalWebGL);
  disable_gl_multisampling_ = browser_command_line.HasSwitch(
      switches::kDisableGLMultisampling);

  ignore_gpu_blacklist_ = browser_command_line.HasSwitch(
      switches::kIgnoreGpuBlacklist);

  use_gl_ = browser_command_line.GetSwitchValueASCII(switches::kUseGL);

  ApplyPolicies();
}

void GpuDataManager::UserFlags::ApplyPolicies() {
  if (disable_accelerated_compositing_) {
    disable_accelerated_2d_canvas_ = true;
    disable_accelerated_layers_ = true;
  }
}

GpuDataManager::GpuDataManager()
    : complete_gpu_info_already_requested_(false) {
  Initialize();
}

void GpuDataManager::Initialize() {
  // Certain tests doesn't go through the browser startup path that
  // initializes GpuDataManager on FILE thread; therefore, it is initialized
  // on UI thread later, and we skip the preliminary gpu info collection
  // in such situation.
  if (BrowserThread::CurrentlyOn(BrowserThread::FILE)) {
    GPUInfo gpu_info;
    gpu_info_collector::CollectPreliminaryGraphicsInfo(&gpu_info);
    UpdateGpuInfo(gpu_info);
  }

  user_flags_.Initialize();

#if defined(OS_MACOSX)
  CGDisplayRegisterReconfigurationCallback(DisplayReconfigCallback, this);
#endif
}

GpuDataManager::~GpuDataManager() {
#if defined(OS_MACOSX)
  CGDisplayRemoveReconfigurationCallback(DisplayReconfigCallback, this);
#endif
}

// static
GpuDataManager* GpuDataManager::GetInstance() {
  return Singleton<GpuDataManager>::get();
}

void GpuDataManager::RequestCompleteGpuInfoIfNeeded() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (complete_gpu_info_already_requested_)
    return;
  complete_gpu_info_already_requested_ = true;

  GpuProcessHost::SendOnIO(
      0,
      content::CAUSE_FOR_GPU_LAUNCH_GPUDATAMANAGER_REQUESTCOMPLETEGPUINFOIFNEEDED,
      new GpuMsg_CollectGraphicsInfo());
}

void GpuDataManager::UpdateGpuInfo(const GPUInfo& gpu_info) {
  {
    base::AutoLock auto_lock(gpu_info_lock_);
    if (!gpu_info_.Merge(gpu_info))
      return;
  }

  RunGpuInfoUpdateCallbacks();

  {
    base::AutoLock auto_lock(gpu_info_lock_);
    content::GetContentClient()->SetGpuInfo(gpu_info_);
  }

  UpdateGpuFeatureFlags();
}

const GPUInfo& GpuDataManager::gpu_info() const {
  base::AutoLock auto_lock(gpu_info_lock_);
  return gpu_info_;
}

Value* GpuDataManager::GetFeatureStatus() {
  bool gpu_access_blocked = !GpuAccessAllowed();

  uint32 flags = GetGpuFeatureFlags().flags();
  DictionaryValue* status = new DictionaryValue();

  const GpuFeatureInfo kGpuFeatureInfo[] = {
      {
          "2d_canvas",
          flags & GpuFeatureFlags::kGpuFeatureAccelerated2dCanvas,
          user_flags_.disable_accelerated_2d_canvas(),
          "Accelerated 2D canvas has been disabled at the command line.",
          true
      },
      {
          "compositing",
          flags & GpuFeatureFlags::kGpuFeatureAcceleratedCompositing,
          user_flags_.disable_accelerated_compositing(),
          "Accelerated compositing has been disabled, either via about:flags or"
          " command line. This adversely affects performance of all hardware"
          " accelerated features.",
          true
      },
      {
          "3d_css",
          flags & GpuFeatureFlags::kGpuFeatureAcceleratedCompositing,
          user_flags_.disable_accelerated_layers(),
          "Accelerated layers have been disabled at the command line.",
          false
      },
      {
          "webgl",
          flags & GpuFeatureFlags::kGpuFeatureWebgl,
          user_flags_.disable_experimental_webgl(),
          "WebGL has been disabled, either via about:flags or command line.",
          false
      },
      {
          "multisampling",
          flags & GpuFeatureFlags::kGpuFeatureMultisampling,
          user_flags_.disable_gl_multisampling(),
          "Multisampling has been disabled, either via about:flags or command"
          " line.",
          false
      }
  };
  const size_t kNumFeatures = sizeof(kGpuFeatureInfo) / sizeof(GpuFeatureInfo);

  // Build the feature_status field.
  {
    ListValue* feature_status_list = new ListValue();

    for (size_t i = 0; i < kNumFeatures; ++i) {
      std::string status;
      if (kGpuFeatureInfo[i].disabled) {
        status = "disabled";
        if (kGpuFeatureInfo[i].fallback_to_software)
          status += "_software";
        else
          status += "_off";
      } else if (kGpuFeatureInfo[i].blocked || gpu_access_blocked) {
        status = "unavailable";
        if (kGpuFeatureInfo[i].fallback_to_software)
          status += "_software";
        else
          status += "_off";
      } else {
        status = "enabled";
        if (kGpuFeatureInfo[i].name == "webgl" &&
            (user_flags_.disable_accelerated_compositing() ||
             (flags & GpuFeatureFlags::kGpuFeatureAcceleratedCompositing)))
          status += "_readback";
      }
      feature_status_list->Append(
          NewStatusValue(kGpuFeatureInfo[i].name.c_str(), status.c_str()));
    }

    status->Set("featureStatus", feature_status_list);
  }

  // Build the problems list.
  {
    ListValue* problem_list = new ListValue();

    if (gpu_access_blocked) {
      DictionaryValue* problem = new DictionaryValue();
      problem->SetString("description",
          "GPU process was unable to boot. Access to GPU disallowed.");
      problem->Set("crBugs", new ListValue());
      problem->Set("webkitBugs", new ListValue());
      problem_list->Append(problem);
    }

    for (size_t i = 0; i < kNumFeatures; ++i) {
      if (kGpuFeatureInfo[i].disabled) {
        DictionaryValue* problem = new DictionaryValue();
        problem->SetString(
            "description", kGpuFeatureInfo[i].disabled_description);
        problem->Set("crBugs", new ListValue());
        problem->Set("webkitBugs", new ListValue());
        problem_list->Append(problem);
      }
    }

    GpuBlacklist* blacklist = GetGpuBlacklist();
    if (blacklist && (!UseGLIsOSMesaOrAny()))
      blacklist->GetBlacklistReasons(problem_list);

    status->Set("problems", problem_list);
  }

  return status;
}

std::string GpuDataManager::GetBlacklistVersion() const {
  GpuBlacklist* blacklist = GetGpuBlacklist();
  if (blacklist != NULL) {
    uint16 version_major, version_minor;
    if (blacklist->GetVersion(&version_major, &version_minor)) {
      std::string version_string =
          base::UintToString(static_cast<unsigned>(version_major)) +
          "." +
          base::UintToString(static_cast<unsigned>(version_minor));
      return version_string;
    }
  }
  return "";
}

void GpuDataManager::AddLogMessage(Value* msg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  log_messages_.Append(msg);
}

const ListValue& GpuDataManager::log_messages() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return log_messages_;
}

GpuFeatureFlags GpuDataManager::GetGpuFeatureFlags() {
  if (UseGLIsOSMesaOrAny())
    return GpuFeatureFlags();
  return gpu_feature_flags_;
}

bool GpuDataManager::GpuAccessAllowed() {
  if (UseGLIsOSMesaOrAny())
    return true;

  // We only need to block GPU process if more features are disallowed other
  // than those in the preliminary gpu feature flags because the latter work
  // through renderer commandline switches.
  uint32 mask = (~(preliminary_gpu_feature_flags_.flags()));
  return (gpu_feature_flags_.flags() & mask) == 0;
}

void GpuDataManager::AddGpuInfoUpdateCallback(Callback0::Type* callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  gpu_info_update_callbacks_.insert(callback);
}

bool GpuDataManager::RemoveGpuInfoUpdateCallback(Callback0::Type* callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  std::set<Callback0::Type*>::iterator i =
      gpu_info_update_callbacks_.find(callback);
  if (i != gpu_info_update_callbacks_.end()) {
    gpu_info_update_callbacks_.erase(i);
    return true;
  }
  return false;
}

void GpuDataManager::AppendRendererCommandLine(
    CommandLine* command_line) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(command_line);

  uint32 flags = gpu_feature_flags_.flags();
  if ((flags & GpuFeatureFlags::kGpuFeatureWebgl)) {
    if (!command_line->HasSwitch(switches::kDisableExperimentalWebGL))
      command_line->AppendSwitch(switches::kDisableExperimentalWebGL);
    if (!command_line->HasSwitch(switches::kDisablePepper3dForUntrustedUse))
      command_line->AppendSwitch(switches::kDisablePepper3dForUntrustedUse);
  }
  if ((flags & GpuFeatureFlags::kGpuFeatureMultisampling) &&
      !command_line->HasSwitch(switches::kDisableGLMultisampling))
    command_line->AppendSwitch(switches::kDisableGLMultisampling);
  if ((flags & GpuFeatureFlags::kGpuFeatureAcceleratedCompositing) &&
      !command_line->HasSwitch(switches::kDisableAcceleratedCompositing))
    command_line->AppendSwitch(switches::kDisableAcceleratedCompositing);
  if ((flags & GpuFeatureFlags::kGpuFeatureAccelerated2dCanvas) &&
      !command_line->HasSwitch(switches::kDisableAccelerated2dCanvas))
    command_line->AppendSwitch(switches::kDisableAccelerated2dCanvas);
}

void GpuDataManager::AppendGpuCommandLine(
    CommandLine* command_line) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(command_line);

  uint32 flags = gpu_feature_flags_.flags();
  if ((flags & GpuFeatureFlags::kGpuFeatureMultisampling) &&
      !command_line->HasSwitch(switches::kDisableGLMultisampling))
    command_line->AppendSwitch(switches::kDisableGLMultisampling);

  if ((flags & (GpuFeatureFlags::kGpuFeatureWebgl |
                GpuFeatureFlags::kGpuFeatureAcceleratedCompositing |
                GpuFeatureFlags::kGpuFeatureAccelerated2dCanvas)) &&
      (user_flags_.use_gl() == "any")) {
    command_line->AppendSwitchASCII(
        switches::kUseGL, gfx::kGLImplementationOSMesaName);
  } else if (!user_flags_.use_gl().empty()) {
    command_line->AppendSwitchASCII(switches::kUseGL, user_flags_.use_gl());
  }
}

void GpuDataManager::SetBuiltInGpuBlacklist(GpuBlacklist* built_in_list) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(built_in_list);
  uint16 version_major, version_minor;
  bool succeed = built_in_list->GetVersion(
      &version_major, &version_minor);
  DCHECK(succeed);
  gpu_blacklist_.reset(built_in_list);
  UpdateGpuFeatureFlags();
  preliminary_gpu_feature_flags_ = gpu_feature_flags_;
  VLOG(1) << "Using software rendering list version "
          << version_major << "." << version_minor;
}

void GpuDataManager::UpdateGpuBlacklist(
    GpuBlacklist* gpu_blacklist, bool preliminary) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(gpu_blacklist);

  scoped_ptr<GpuBlacklist> updated_list(gpu_blacklist);

  uint16 updated_version_major, updated_version_minor;
  if (!updated_list->GetVersion(
          &updated_version_major, &updated_version_minor))
    return;

  uint16 current_version_major, current_version_minor;
  bool succeed = gpu_blacklist_->GetVersion(
      &current_version_major, &current_version_minor);
  DCHECK(succeed);
  if (updated_version_major < current_version_major ||
      (updated_version_major == current_version_major &&
       updated_version_minor <= current_version_minor))
    return;

  gpu_blacklist_.reset(updated_list.release());
  UpdateGpuFeatureFlags();
  if (preliminary)
    preliminary_gpu_feature_flags_ = gpu_feature_flags_;
  VLOG(1) << "Using software rendering list version "
          << updated_version_major << "." << updated_version_minor;
}

void GpuDataManager::HandleGpuSwitch() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  GPUInfo gpu_info;
  gpu_info_collector::CollectVideoCardInfo(&gpu_info);
  LOG(INFO) << "Switching to use GPU: vendor_id = 0x"
            << base::StringPrintf("%04x", gpu_info.vendor_id)
            << ", device_id = 0x"
            << base::StringPrintf("%04x", gpu_info.device_id);
  // TODO(zmo): update gpu_info_, re-run blacklist logic, maybe close and
  // relaunch GPU process.
}

DictionaryValue* GpuDataManager::GpuInfoAsDictionaryValue() const {
  ListValue* basic_info = new ListValue();
  basic_info->Append(NewDescriptionValuePair(
      "Initialization time",
      base::Int64ToString(gpu_info().initialization_time.InMilliseconds())));
  basic_info->Append(NewDescriptionValuePair(
      "Vendor Id", base::StringPrintf("0x%04x", gpu_info().vendor_id)));
  basic_info->Append(NewDescriptionValuePair(
      "Device Id", base::StringPrintf("0x%04x", gpu_info().device_id)));
  basic_info->Append(NewDescriptionValuePair("Driver vendor",
                                             gpu_info().driver_vendor));
  basic_info->Append(NewDescriptionValuePair("Driver version",
                                             gpu_info().driver_version));
  basic_info->Append(NewDescriptionValuePair("Driver date",
                                             gpu_info().driver_date));
  basic_info->Append(NewDescriptionValuePair("Pixel shader version",
                                             gpu_info().pixel_shader_version));
  basic_info->Append(NewDescriptionValuePair("Vertex shader version",
                                             gpu_info().vertex_shader_version));
  basic_info->Append(NewDescriptionValuePair("GL version",
                                             gpu_info().gl_version));
  basic_info->Append(NewDescriptionValuePair("GL_VENDOR",
                                             gpu_info().gl_vendor));
  basic_info->Append(NewDescriptionValuePair("GL_RENDERER",
                                             gpu_info().gl_renderer));
  basic_info->Append(NewDescriptionValuePair("GL_VERSION",
                                             gpu_info().gl_version_string));
  basic_info->Append(NewDescriptionValuePair("GL_EXTENSIONS",
                                             gpu_info().gl_extensions));

  DictionaryValue* info = new DictionaryValue();
  info->Set("basic_info", basic_info);

#if defined(OS_WIN)
  Value* dx_info;
  if (gpu_info().dx_diagnostics.children.size())
    dx_info = DxDiagNodeToList(gpu_info().dx_diagnostics);
  else
    dx_info = Value::CreateNullValue();
  info->Set("diagnostics", dx_info);
#endif

  return info;
}

void GpuDataManager::RunGpuInfoUpdateCallbacks() {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(this, &GpuDataManager::RunGpuInfoUpdateCallbacks));
    return;
  }

  std::set<Callback0::Type*>::iterator i = gpu_info_update_callbacks_.begin();
  for (; i != gpu_info_update_callbacks_.end(); ++i) {
    (*i)->Run();
  }
}

void GpuDataManager::UpdateGpuFeatureFlags() {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(this, &GpuDataManager::UpdateGpuFeatureFlags));
    return;
  }

  GpuBlacklist* gpu_blacklist = GetGpuBlacklist();
  // We don't set a lock around modifying gpu_feature_flags_ since it's just an
  // int.
  if (!gpu_blacklist) {
    gpu_feature_flags_.set_flags(0);
    return;
  }

  {
    base::AutoLock auto_lock(gpu_info_lock_);
    gpu_feature_flags_ = gpu_blacklist->DetermineGpuFeatureFlags(
        GpuBlacklist::kOsAny, NULL, gpu_info_);
  }

  // Notify clients that GpuInfo state has changed
  RunGpuInfoUpdateCallbacks();

  uint32 flags = gpu_feature_flags_.flags();
  uint32 max_entry_id = gpu_blacklist->max_entry_id();
  bool disabled = false;
  if (flags == 0) {
    UMA_HISTOGRAM_ENUMERATION("GPU.BlacklistTestResultsPerEntry",
        0, max_entry_id + 1);
  } else {
    std::vector<uint32> flag_entries;
    gpu_blacklist->GetGpuFeatureFlagEntries(
        GpuFeatureFlags::kGpuFeatureAll, flag_entries, disabled);
    DCHECK_GT(flag_entries.size(), 0u);
    for (size_t i = 0; i < flag_entries.size(); ++i) {
      UMA_HISTOGRAM_ENUMERATION("GPU.BlacklistTestResultsPerEntry",
          flag_entries[i], max_entry_id + 1);
    }
  }

  // This counts how many users are affected by a disabled entry - this allows
  // us to understand the impact of an entry before enable it.
  std::vector<uint32> flag_disabled_entries;
  disabled = true;
  gpu_blacklist->GetGpuFeatureFlagEntries(
      GpuFeatureFlags::kGpuFeatureAll, flag_disabled_entries, disabled);
  for (size_t i = 0; i < flag_disabled_entries.size(); ++i) {
    UMA_HISTOGRAM_ENUMERATION("GPU.BlacklistTestResultsPerDisabledEntry",
        flag_disabled_entries[i], max_entry_id + 1);
  }

  const GpuFeatureFlags::GpuFeatureType kGpuFeatures[] = {
      GpuFeatureFlags::kGpuFeatureAccelerated2dCanvas,
      GpuFeatureFlags::kGpuFeatureAcceleratedCompositing,
      GpuFeatureFlags::kGpuFeatureWebgl
  };
  const std::string kGpuBlacklistFeatureHistogramNames[] = {
      "GPU.BlacklistFeatureTestResults.Accelerated2dCanvas",
      "GPU.BlacklistFeatureTestResults.AcceleratedCompositing",
      "GPU.BlacklistFeatureTestResults.Webgl"
  };
  const bool kGpuFeatureUserFlags[] = {
      user_flags_.disable_accelerated_2d_canvas(),
      user_flags_.disable_accelerated_compositing(),
      user_flags_.disable_experimental_webgl()
  };
#if defined(OS_WIN)
  const std::string kGpuBlacklistFeatureHistogramNamesWin[] = {
      "GPU.BlacklistFeatureTestResultsWindows.Accelerated2dCanvas",
      "GPU.BlacklistFeatureTestResultsWindows.AcceleratedCompositing",
      "GPU.BlacklistFeatureTestResultsWindows.Webgl"
  };
#endif
  const size_t kNumFeatures =
      sizeof(kGpuFeatures) / sizeof(GpuFeatureFlags::GpuFeatureType);
  for (size_t i = 0; i < kNumFeatures; ++i) {
    // We can't use UMA_HISTOGRAM_ENUMERATION here because the same name is
    // expected if the macro is used within a loop.
    GpuFeatureStatus value = kGpuFeatureEnabled;
    if (flags & kGpuFeatures[i])
      value = kGpuFeatureBlacklisted;
    else if (kGpuFeatureUserFlags[i])
      value = kGpuFeatureDisabled;
    base::Histogram* histogram_pointer = base::LinearHistogram::FactoryGet(
        kGpuBlacklistFeatureHistogramNames[i],
        1, kGpuFeatureNumStatus, kGpuFeatureNumStatus + 1,
        base::Histogram::kUmaTargetedHistogramFlag);
    histogram_pointer->Add(value);
#if defined(OS_WIN)
    histogram_pointer = base::LinearHistogram::FactoryGet(
        kGpuBlacklistFeatureHistogramNamesWin[i],
        1, kNumWinSubVersions * kGpuFeatureNumStatus,
        kNumWinSubVersions * kGpuFeatureNumStatus + 1,
        base::Histogram::kUmaTargetedHistogramFlag);
    histogram_pointer->Add(GetGpuBlacklistHistogramValueWin(value));
#endif
  }
}

GpuBlacklist* GpuDataManager::GetGpuBlacklist() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (user_flags_.ignore_gpu_blacklist())
    return NULL;
  // No need to return an empty blacklist.
  if (gpu_blacklist_.get() != NULL && gpu_blacklist_->max_entry_id() == 0)
    return NULL;
  return gpu_blacklist_.get();
}

bool GpuDataManager::UseGLIsOSMesaOrAny() {
  return (user_flags_.use_gl() == "any" ||
          user_flags_.use_gl() == gfx::kGLImplementationOSMesaName);
}

