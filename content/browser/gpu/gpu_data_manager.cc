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
#include "content/common/content_switches.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/gpu/gpu_info_collector.h"
#include "ui/gfx/gl/gl_implementation.h"
#include "ui/gfx/gl/gl_switches.h"
#include "webkit/plugins/plugin_switches.h"

namespace {

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

#if defined(OS_WIN)
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

#endif  // OS_WIN

std::string GetOSString() {
  std::string rt;
#if defined(OS_CHROMEOS)
  rt = "ChromeOS";
#elif defined(OS_WIN)
  rt = "Win";
  std::string version_str = base::SysInfo::OperatingSystemVersion();
  size_t pos = version_str.find_first_not_of("0123456789.");
  if (pos != std::string::npos)
    version_str = version_str.substr(0, pos);
  scoped_ptr<Version> os_version(Version::GetVersionFromString(version_str));
  if (os_version.get() && os_version->components().size() >= 2) {
    const std::vector<uint16>& version_numbers = os_version->components();
    if (version_numbers[0] == 5)
      rt += "XP";
    else if (version_numbers[0] == 6 && version_numbers[1] == 0)
      rt += "Vista";
    else if (version_numbers[0] == 6 && version_numbers[1] == 1)
      rt += "7";
  }
#elif defined(OS_LINUX)
  rt = "Linux";
#elif defined(OS_MACOSX)
  rt = "Mac";
#else
  rt = "UnknownOS";
#endif
  return rt;
}

}  // namespace anonymous

GpuDataManager::GpuDataManager()
    : complete_gpu_info_already_requested_(false) {
  // Certain tests doesn't go through the browser startup path that
  // initializes GpuDataManager on FILE thread; therefore, it is initialized
  // on UI thread later, and we skip the preliminary gpu info collection
  // in such situation.
  if (!BrowserThread::CurrentlyOn(BrowserThread::FILE))
    return;

  GPUInfo gpu_info;
  gpu_info_collector::CollectPreliminaryGraphicsInfo(&gpu_info);
  UpdateGpuInfo(gpu_info);

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
  const CommandLine& browser_command_line = *CommandLine::ForCurrentProcess();
  if (gpu_blacklist_.get())
    return gpu_blacklist_->GetFeatureStatus(GpuAccessAllowed(),
        browser_command_line.HasSwitch(
            switches::kDisableAcceleratedCompositing),
        browser_command_line.HasSwitch(
            switches::kDisableAccelerated2dCanvas),
        browser_command_line.HasSwitch(switches::kDisableExperimentalWebGL),
        browser_command_line.HasSwitch(switches::kDisableGLMultisampling));
  return NULL;
}

std::string GpuDataManager::GetBlacklistVersion() const {
  if (gpu_blacklist_.get() != NULL) {
    uint16 version_major, version_minor;
    if (gpu_blacklist_->GetVersion(&version_major,
                                   &version_minor)) {
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
  return gpu_feature_flags_;
}

bool GpuDataManager::GpuAccessAllowed() {
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
  if (flags == 0) {
    UMA_HISTOGRAM_ENUMERATION("GPU.BlacklistTestResultsPerEntry",
        0, max_entry_id + 1);
  } else {
    std::vector<uint32> flag_entries;
    gpu_blacklist->GetGpuFeatureFlagEntries(
        GpuFeatureFlags::kGpuFeatureAll, flag_entries);
    DCHECK_GT(flag_entries.size(), 0u);
    for (size_t i = 0; i < flag_entries.size(); ++i) {
      UMA_HISTOGRAM_ENUMERATION("GPU.BlacklistTestResultsPerEntry",
          flag_entries[i], max_entry_id + 1);
    }
  }

  const GpuFeatureFlags::GpuFeatureType kGpuFeatures[] = {
      GpuFeatureFlags::kGpuFeatureAccelerated2dCanvas,
      GpuFeatureFlags::kGpuFeatureAcceleratedCompositing,
      GpuFeatureFlags::kGpuFeatureWebgl
  };
  const std::string kOSString = GetOSString();
  const std::string kGpuBlacklistFeatureHistogramNames[] = {
      "GPU.BlacklistAccelerated2dCanvasTestResults" + kOSString,
      "GPU.BlacklistAcceleratedCompositingTestResults" + kOSString,
      "GPU.BlacklistWebglTestResults" + kOSString
  };
  const size_t kNumFeatures =
      sizeof(kGpuFeatures) / sizeof(GpuFeatureFlags::GpuFeatureType);
  for (size_t i = 0; i < kNumFeatures; ++i) {
    // We can't use UMA_HISTOGRAM_ENUMERATION here because the same name is
    // expected if the macro is used within a loop.
    base::Histogram* histogram_pointer = base::LinearHistogram::FactoryGet(
        kGpuBlacklistFeatureHistogramNames[i], 1, 2, 3,
        base::Histogram::kUmaTargetedHistogramFlag);
    histogram_pointer->Add((flags & kGpuFeatures[i]) ? 1 : 0);
  }
}

GpuBlacklist* GpuDataManager::GetGpuBlacklist() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const CommandLine& browser_command_line = *CommandLine::ForCurrentProcess();
  if (browser_command_line.HasSwitch(switches::kIgnoreGpuBlacklist) ||
      browser_command_line.GetSwitchValueASCII(
          switches::kUseGL) == gfx::kGLImplementationOSMesaName)
    return NULL;
  // No need to return an empty blacklist.
  if (gpu_blacklist_.get() != NULL && gpu_blacklist_->max_entry_id() == 0)
    return NULL;
  return gpu_blacklist_.get();
}
