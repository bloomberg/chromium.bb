// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/task_manager.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/i18n/number_formatting.h"
#include "base/i18n/rtl.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/process_util.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/background/background_contents_service.h"
#include "chrome/browser/background/background_contents_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/task_manager/task_manager_resource_providers.h"
#include "chrome/browser/task_manager/task_manager_worker_resource_provider.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/gpu_data_manager_observer.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/result_codes.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/ui_resources.h"
#include "third_party/icu/public/i18n/unicode/coll.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/gfx/image/image_skia.h"

#if defined(OS_WIN)
#include "chrome/browser/task_manager/task_manager_os_resources_win.h"
#endif

using content::BrowserThread;
using content::OpenURLParams;
using content::Referrer;
using content::ResourceRequestInfo;
using content::WebContents;

namespace {

// The delay between updates of the information (in ms).
#if defined(OS_MACOSX)
// Match Activity Monitor's default refresh rate.
const int kUpdateTimeMs = 2000;
#else
const int kUpdateTimeMs = 1000;
#endif

template <class T>
int ValueCompare(T value1, T value2) {
  if (value1 < value2)
    return -1;
  if (value1 == value2)
    return 0;
  return 1;
}

// Used when one or both of the results to compare are unavailable.
int OrderUnavailableValue(bool v1, bool v2) {
  if (!v1 && !v2)
    return 0;
  return v1 ? 1 : -1;
}

// Used by TaskManagerModel::CompareValues(). See it for details of return
// value.
template <class T>
int ValueCompareMember(const TaskManagerModel* model,
                       bool (TaskManagerModel::*f)(int, T*) const,
                       int row1,
                       int row2) {
  T value1;
  T value2;
  bool value1_valid = (model->*f)(row1, &value1);
  bool value2_valid = (model->*f)(row2, &value2);
  return value1_valid && value2_valid ? ValueCompare(value1, value2) :
      OrderUnavailableValue(value1_valid, value2_valid);
}

string16 FormatStatsSize(const WebKit::WebCache::ResourceTypeStat& stat) {
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_CACHE_SIZE_CELL_TEXT,
      ui::FormatBytesWithUnits(stat.size, ui::DATA_UNITS_KIBIBYTE, false),
      ui::FormatBytesWithUnits(stat.liveSize, ui::DATA_UNITS_KIBIBYTE, false));
}

// Returns true if the specified id should use the first value in the group.
bool IsSharedByGroup(int col_id) {
  switch (col_id) {
    case IDS_TASK_MANAGER_PRIVATE_MEM_COLUMN:
    case IDS_TASK_MANAGER_SHARED_MEM_COLUMN:
    case IDS_TASK_MANAGER_PHYSICAL_MEM_COLUMN:
    case IDS_TASK_MANAGER_CPU_COLUMN:
    case IDS_TASK_MANAGER_PROCESS_ID_COLUMN:
    case IDS_TASK_MANAGER_JAVASCRIPT_MEMORY_ALLOCATED_COLUMN:
    case IDS_TASK_MANAGER_SQLITE_MEMORY_USED_COLUMN:
    case IDS_TASK_MANAGER_WEBCORE_IMAGE_CACHE_COLUMN:
    case IDS_TASK_MANAGER_WEBCORE_SCRIPTS_CACHE_COLUMN:
    case IDS_TASK_MANAGER_WEBCORE_CSS_CACHE_COLUMN:
      return true;
    default:
      return false;
  }
}

}  // namespace

class TaskManagerModelGpuDataManagerObserver
    : public content::GpuDataManagerObserver {
 public:
  TaskManagerModelGpuDataManagerObserver() {
    content::GpuDataManager::GetInstance()->AddObserver(this);
  }

  virtual ~TaskManagerModelGpuDataManagerObserver() {
    content::GpuDataManager::GetInstance()->RemoveObserver(this);
  }

  static void NotifyVideoMemoryUsageStats(
      content::GPUVideoMemoryUsageStats video_memory_usage_stats) {
    TaskManager::GetInstance()->model()->NotifyVideoMemoryUsageStats(
        video_memory_usage_stats);
  }

  virtual void OnVideoMemoryUsageStatsUpdate(
      const content::GPUVideoMemoryUsageStats& video_memory_usage_stats)
          OVERRIDE {
    if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
      NotifyVideoMemoryUsageStats(video_memory_usage_stats);
    } else {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE, base::Bind(
              &TaskManagerModelGpuDataManagerObserver::
                  NotifyVideoMemoryUsageStats,
              video_memory_usage_stats));
    }
  }
};

TaskManagerModel::PerResourceValues::PerResourceValues()
    : is_title_valid(false),
      is_profile_name_valid(false),
      network_usage(0),
      is_process_id_valid(false),
      process_id(0),
      is_goats_teleported_valid(false),
      goats_teleported(0),
      is_webcore_stats_valid(false),
      is_fps_valid(false),
      fps(0),
      is_sqlite_memory_bytes_valid(false),
      sqlite_memory_bytes(0),
      is_v8_memory_valid(false),
      v8_memory_allocated(0),
      v8_memory_used(0) {
}

TaskManagerModel::PerResourceValues::~PerResourceValues() {
}

TaskManagerModel::PerProcessValues::PerProcessValues()
    : is_cpu_usage_valid(false),
      cpu_usage(0),
      is_private_and_shared_valid(false),
      private_bytes(0),
      shared_bytes(0),
      is_physical_memory_valid(false),
      physical_memory(0),
      is_video_memory_valid(false),
      video_memory(0),
      video_memory_has_duplicates(false),
      is_gdi_handles_valid(false),
      gdi_handles(0),
      gdi_handles_peak(0),
      is_user_handles_valid(0),
      user_handles(0),
      user_handles_peak(0) {
}

TaskManagerModel::PerProcessValues::~PerProcessValues() {
}

////////////////////////////////////////////////////////////////////////////////
// TaskManagerModel class
////////////////////////////////////////////////////////////////////////////////

TaskManagerModel::TaskManagerModel(TaskManager* task_manager)
    : pending_video_memory_usage_stats_update_(false),
      update_requests_(0),
      listen_requests_(0),
      update_state_(IDLE),
      goat_salt_(base::RandUint64()),
      last_unique_id_(0) {
  AddResourceProvider(
      new TaskManagerBrowserProcessResourceProvider(task_manager));
  AddResourceProvider(
      new TaskManagerBackgroundContentsResourceProvider(task_manager));
  AddResourceProvider(new TaskManagerTabContentsResourceProvider(task_manager));
  AddResourceProvider(new TaskManagerPanelResourceProvider(task_manager));
  AddResourceProvider(
      new TaskManagerChildProcessResourceProvider(task_manager));
  AddResourceProvider(
      new TaskManagerExtensionProcessResourceProvider(task_manager));
  AddResourceProvider(
      new TaskManagerGuestResourceProvider(task_manager));

#if defined(ENABLE_NOTIFICATIONS)
  TaskManager::ResourceProvider* provider =
      TaskManagerNotificationResourceProvider::Create(task_manager);
  if (provider)
    AddResourceProvider(provider);
#endif

  AddResourceProvider(new TaskManagerWorkerResourceProvider(task_manager));
}

void TaskManagerModel::AddObserver(TaskManagerModelObserver* observer) {
  observer_list_.AddObserver(observer);
}

void TaskManagerModel::RemoveObserver(TaskManagerModelObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

int TaskManagerModel::ResourceCount() const {
  return resources_.size();
}

int TaskManagerModel::GroupCount() const {
  return group_map_.size();
}

int64 TaskManagerModel::GetNetworkUsage(int index) const {
  return GetNetworkUsage(GetResource(index));
}

double TaskManagerModel::GetCPUUsage(int index) const {
  return GetCPUUsage(GetResource(index));
}

base::ProcessId TaskManagerModel::GetProcessId(int index) const {
  PerResourceValues& values(GetPerResourceValues(index));
  if (!values.is_process_id_valid) {
    values.is_process_id_valid = true;
    values.process_id = base::GetProcId(GetResource(index)->GetProcess());
  }
  return values.process_id;
}

base::ProcessHandle TaskManagerModel::GetProcess(int index) const {
  return GetResource(index)->GetProcess();
}

int TaskManagerModel::GetResourceUniqueId(int index) const {
  return GetResource(index)->get_unique_id();
}

int TaskManagerModel::GetResourceIndexByUniqueId(const int unique_id) const {
  for (int resource_index = 0; resource_index < ResourceCount();
       ++resource_index) {
    if (GetResourceUniqueId(resource_index) == unique_id)
      return resource_index;
  }
  return -1;
}

string16 TaskManagerModel::GetResourceById(int index, int col_id) const {
  if (IsSharedByGroup(col_id) && !IsResourceFirstInGroup(index))
    return string16();

  switch (col_id) {
    case IDS_TASK_MANAGER_TASK_COLUMN:
      return GetResourceTitle(index);

    case IDS_TASK_MANAGER_PROFILE_NAME_COLUMN:
      return GetResourceProfileName(index);

    case IDS_TASK_MANAGER_NET_COLUMN:
      return GetResourceNetworkUsage(index);

    case IDS_TASK_MANAGER_CPU_COLUMN:
      return GetResourceCPUUsage(index);

    case IDS_TASK_MANAGER_PRIVATE_MEM_COLUMN:
      return GetResourcePrivateMemory(index);

    case IDS_TASK_MANAGER_SHARED_MEM_COLUMN:
      return GetResourceSharedMemory(index);

    case IDS_TASK_MANAGER_PHYSICAL_MEM_COLUMN:
      return GetResourcePhysicalMemory(index);

    case IDS_TASK_MANAGER_PROCESS_ID_COLUMN:
      return GetResourceProcessId(index);

    case IDS_TASK_MANAGER_GDI_HANDLES_COLUMN:
      return GetResourceGDIHandles(index);

    case IDS_TASK_MANAGER_USER_HANDLES_COLUMN:
      return GetResourceUSERHandles(index);

    case IDS_TASK_MANAGER_GOATS_TELEPORTED_COLUMN:
      return GetResourceGoatsTeleported(index);

    case IDS_TASK_MANAGER_WEBCORE_IMAGE_CACHE_COLUMN:
      return GetResourceWebCoreImageCacheSize(index);

    case IDS_TASK_MANAGER_WEBCORE_SCRIPTS_CACHE_COLUMN:
      return GetResourceWebCoreScriptsCacheSize(index);

    case IDS_TASK_MANAGER_WEBCORE_CSS_CACHE_COLUMN:
      return GetResourceWebCoreCSSCacheSize(index);

    case IDS_TASK_MANAGER_FPS_COLUMN:
      return GetResourceFPS(index);

    case IDS_TASK_MANAGER_VIDEO_MEMORY_COLUMN:
      return GetResourceVideoMemory(index);

    case IDS_TASK_MANAGER_SQLITE_MEMORY_USED_COLUMN:
      return GetResourceSqliteMemoryUsed(index);

    case IDS_TASK_MANAGER_JAVASCRIPT_MEMORY_ALLOCATED_COLUMN:
      return GetResourceV8MemoryAllocatedSize(index);

    default:
      NOTREACHED();
      return string16();
  }
}

const string16& TaskManagerModel::GetResourceTitle(int index) const {
  PerResourceValues& values = GetPerResourceValues(index);
  if (!values.is_title_valid) {
    values.is_title_valid = true;
    values.title = GetResource(index)->GetTitle();
  }
  return values.title;
}

const string16& TaskManagerModel::GetResourceProfileName(int index) const {
  PerResourceValues& values(GetPerResourceValues(index));
  if (!values.is_profile_name_valid) {
    values.is_profile_name_valid = true;
    values.profile_name = GetResource(index)->GetProfileName();
  }
  return values.profile_name;
}

string16 TaskManagerModel::GetResourceNetworkUsage(int index) const {
  int64 net_usage = GetNetworkUsage(index);
  if (net_usage == -1)
    return l10n_util::GetStringUTF16(IDS_TASK_MANAGER_NA_CELL_TEXT);
  if (net_usage == 0)
    return ASCIIToUTF16("0");
  string16 net_byte = ui::FormatSpeed(net_usage);
  // Force number string to have LTR directionality.
  return base::i18n::GetDisplayStringInLTRDirectionality(net_byte);
}

string16 TaskManagerModel::GetResourceCPUUsage(int index) const {
  return UTF8ToUTF16(base::StringPrintf(
#if defined(OS_MACOSX)
      // Activity Monitor shows %cpu with one decimal digit -- be
      // consistent with that.
      "%.1f",
#else
      "%.0f",
#endif
      GetCPUUsage(GetResource(index))));
}

string16 TaskManagerModel::GetResourcePrivateMemory(int index) const {
  size_t private_mem;
  if (!GetPrivateMemory(index, &private_mem))
    return ASCIIToUTF16("N/A");
  return GetMemCellText(private_mem);
}

string16 TaskManagerModel::GetResourceSharedMemory(int index) const {
  size_t shared_mem;
  if (!GetSharedMemory(index, &shared_mem))
    return ASCIIToUTF16("N/A");
  return GetMemCellText(shared_mem);
}

string16 TaskManagerModel::GetResourcePhysicalMemory(int index) const {
  size_t phys_mem;
  GetPhysicalMemory(index, &phys_mem);
  return GetMemCellText(phys_mem);
}

string16 TaskManagerModel::GetResourceProcessId(int index) const {
  return base::IntToString16(GetProcessId(index));
}

string16 TaskManagerModel::GetResourceGDIHandles(int index) const {
  size_t current, peak;
  GetGDIHandles(index, &current, &peak);
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_HANDLES_CELL_TEXT,
      base::IntToString16(current), base::IntToString16(peak));
}

string16 TaskManagerModel::GetResourceUSERHandles(int index) const {
  size_t current, peak;
  GetUSERHandles(index, &current, &peak);
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_HANDLES_CELL_TEXT,
      base::IntToString16(current), base::IntToString16(peak));
}

string16 TaskManagerModel::GetResourceWebCoreImageCacheSize(
    int index) const {
  if (!CacheWebCoreStats(index))
    return l10n_util::GetStringUTF16(IDS_TASK_MANAGER_NA_CELL_TEXT);
  return FormatStatsSize(GetPerResourceValues(index).webcore_stats.images);
}

string16 TaskManagerModel::GetResourceWebCoreScriptsCacheSize(
    int index) const {
  if (!CacheWebCoreStats(index))
    return l10n_util::GetStringUTF16(IDS_TASK_MANAGER_NA_CELL_TEXT);
  return FormatStatsSize(GetPerResourceValues(index).webcore_stats.scripts);
}

string16 TaskManagerModel::GetResourceWebCoreCSSCacheSize(
    int index) const {
  if (!CacheWebCoreStats(index))
    return l10n_util::GetStringUTF16(IDS_TASK_MANAGER_NA_CELL_TEXT);
  return FormatStatsSize(
      GetPerResourceValues(index).webcore_stats.cssStyleSheets);
}

string16 TaskManagerModel::GetResourceVideoMemory(int index) const {
  size_t video_memory;
  bool has_duplicates;
  if (!GetVideoMemory(index, &video_memory, &has_duplicates) || !video_memory)
    return ASCIIToUTF16("N/A");
  if (has_duplicates) {
    return ASCIIToUTF16("(") +
        GetMemCellText(video_memory) +
        ASCIIToUTF16(")");
  }
  return GetMemCellText(video_memory);
}

string16 TaskManagerModel::GetResourceFPS(
    int index) const {
  float fps = 0;
  if (!GetFPS(index, &fps))
    return l10n_util::GetStringUTF16(IDS_TASK_MANAGER_NA_CELL_TEXT);
  return UTF8ToUTF16(base::StringPrintf("%.0f", fps));
}

string16 TaskManagerModel::GetResourceSqliteMemoryUsed(int index) const {
  size_t bytes = 0;
  if (!GetSqliteMemoryUsedBytes(index, &bytes))
    return l10n_util::GetStringUTF16(IDS_TASK_MANAGER_NA_CELL_TEXT);
  return GetMemCellText(bytes);
}

string16 TaskManagerModel::GetResourceGoatsTeleported(int index) const {
  CHECK_LT(index, ResourceCount());
  return base::FormatNumber(GetGoatsTeleported(index));
}

string16 TaskManagerModel::GetResourceV8MemoryAllocatedSize(
    int index) const {
  size_t memory_allocated = 0, memory_used = 0;
  if (!GetV8MemoryUsed(index, &memory_used) ||
      !GetV8Memory(index, &memory_allocated))
    return l10n_util::GetStringUTF16(IDS_TASK_MANAGER_NA_CELL_TEXT);
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_CACHE_SIZE_CELL_TEXT,
      ui::FormatBytesWithUnits(memory_allocated,
                               ui::DATA_UNITS_KIBIBYTE,
                               false),
      ui::FormatBytesWithUnits(memory_used,
                               ui::DATA_UNITS_KIBIBYTE,
                               false));
}

bool TaskManagerModel::GetPrivateMemory(int index, size_t* result) const {
  *result = 0;
  base::ProcessHandle handle = GetResource(index)->GetProcess();
  if (!CachePrivateAndSharedMemory(handle))
    return false;
  *result = per_process_cache_[handle].private_bytes;
  return true;
}

bool TaskManagerModel::GetSharedMemory(int index, size_t* result) const {
  *result = 0;
  base::ProcessHandle handle = GetResource(index)->GetProcess();
  if (!CachePrivateAndSharedMemory(handle))
    return false;
  *result = per_process_cache_[handle].shared_bytes;
  return true;
}

bool TaskManagerModel::GetPhysicalMemory(int index, size_t* result) const {
  *result = 0;

  base::ProcessHandle handle = GetResource(index)->GetProcess();
  PerProcessValues& values(per_process_cache_[handle]);

  if (!values.is_physical_memory_valid) {
    base::WorkingSetKBytes ws_usage;
    MetricsMap::const_iterator iter = metrics_map_.find(handle);
    if (iter == metrics_map_.end() ||
        !iter->second->GetWorkingSetKBytes(&ws_usage))
      return false;

    // Memory = working_set.private + working_set.shareable.
    // We exclude the shared memory.
    values.is_physical_memory_valid = true;
    values.physical_memory = iter->second->GetWorkingSetSize();
    values.physical_memory -= ws_usage.shared * 1024;
  }
  *result = values.physical_memory;
  return true;
}

void TaskManagerModel::GetGDIHandles(int index,
                                     size_t* current,
                                     size_t* peak) const {
  *current = 0;
  *peak = 0;
#if defined(OS_WIN)
  base::ProcessHandle handle = GetResource(index)->GetProcess();
  PerProcessValues& values(per_process_cache_[handle]);

  if (!values.is_gdi_handles_valid) {
    GetWinGDIHandles(GetResource(index)->GetProcess(),
                     &values.gdi_handles,
                     &values.gdi_handles_peak);
    values.is_gdi_handles_valid = true;
  }
  *current = values.gdi_handles;
  *peak = values.gdi_handles_peak;
#endif
}

void TaskManagerModel::GetUSERHandles(int index,
                                      size_t* current,
                                      size_t* peak) const {
  *current = 0;
  *peak = 0;
#if defined(OS_WIN)
  base::ProcessHandle handle = GetResource(index)->GetProcess();
  PerProcessValues& values(per_process_cache_[handle]);

  if (!values.is_user_handles_valid) {
    GetWinUSERHandles(GetResource(index)->GetProcess(),
                      &values.user_handles,
                      &values.user_handles_peak);
    values.is_user_handles_valid = true;
  }
  *current = values.user_handles;
  *peak = values.user_handles_peak;
#endif
}

bool TaskManagerModel::GetWebCoreCacheStats(
    int index,
    WebKit::WebCache::ResourceTypeStats* result) const {
  if (!CacheWebCoreStats(index))
    return false;
  *result = GetPerResourceValues(index).webcore_stats;
  return true;
}

bool TaskManagerModel::GetVideoMemory(int index,
                                      size_t* video_memory,
                                      bool* has_duplicates) const {
  *video_memory = 0;
  *has_duplicates = false;

  base::ProcessId pid = GetProcessId(index);
  PerProcessValues& values(
      per_process_cache_[GetResource(index)->GetProcess()]);
  if (!values.is_video_memory_valid) {
    content::GPUVideoMemoryUsageStats::ProcessMap::const_iterator i =
        video_memory_usage_stats_.process_map.find(pid);
    if (i == video_memory_usage_stats_.process_map.end())
      return false;
    values.is_video_memory_valid = true;
    values.video_memory = i->second.video_memory;
    values.video_memory_has_duplicates = i->second.has_duplicates;
  }
  *video_memory = values.video_memory;
  *has_duplicates = values.video_memory_has_duplicates;
  return true;
}

bool TaskManagerModel::GetFPS(int index, float* result) const {
  *result = 0;
  PerResourceValues& values(GetPerResourceValues(index));
  if (!values.is_fps_valid) {
    if (!GetResource(index)->ReportsFPS())
      return false;
    values.is_fps_valid = true;
    values.fps = GetResource(index)->GetFPS();
  }
  *result = values.fps;
  return true;
}

bool TaskManagerModel::GetSqliteMemoryUsedBytes(
    int index,
    size_t* result) const {
  *result = 0;
  PerResourceValues& values(GetPerResourceValues(index));
  if (!values.is_sqlite_memory_bytes_valid) {
    if (!GetResource(index)->ReportsSqliteMemoryUsed())
      return false;
    values.is_sqlite_memory_bytes_valid = true;
    values.sqlite_memory_bytes = GetResource(index)->SqliteMemoryUsedBytes();
  }
  *result = values.sqlite_memory_bytes;
  return true;
}

bool TaskManagerModel::GetV8Memory(int index, size_t* result) const {
  *result = 0;
  if (!CacheV8Memory(index))
    return false;
  *result = GetPerResourceValues(index).v8_memory_allocated;
  return true;
}

bool TaskManagerModel::GetV8MemoryUsed(int index, size_t* result) const {
  *result = 0;
  if (!CacheV8Memory(index))
    return false;
  *result = GetPerResourceValues(index).v8_memory_used;
  return true;
}

bool TaskManagerModel::CanActivate(int index) const {
  CHECK_LT(index, ResourceCount());
  return GetResourceWebContents(index) != NULL;
}

bool TaskManagerModel::CanInspect(int index) const {
  return GetResource(index)->CanInspect();
}

void TaskManagerModel::Inspect(int index) const {
  CHECK_LT(index, ResourceCount());
  GetResource(index)->Inspect();
}

int TaskManagerModel::GetGoatsTeleported(int index) const {
  PerResourceValues& values(GetPerResourceValues(index));
  if (!values.is_goats_teleported_valid) {
    values.is_goats_teleported_valid = true;
    values.goats_teleported = goat_salt_ * (index + 1);
    values.goats_teleported = (values.goats_teleported >> 16) & 255;
  }
  return values.goats_teleported;
}

bool TaskManagerModel::IsResourceFirstInGroup(int index) const {
  TaskManager::Resource* resource = GetResource(index);
  GroupMap::const_iterator iter = group_map_.find(resource->GetProcess());
  DCHECK(iter != group_map_.end());
  const ResourceList* group = iter->second;
  return ((*group)[0] == resource);
}

bool TaskManagerModel::IsResourceLastInGroup(int index) const {
  TaskManager::Resource* resource = GetResource(index);
  GroupMap::const_iterator iter = group_map_.find(resource->GetProcess());
  DCHECK(iter != group_map_.end());
  const ResourceList* group = iter->second;
  return (group->back() == resource);
}

bool TaskManagerModel::IsBackgroundResource(int index) const {
  return GetResource(index)->IsBackground();
}

gfx::ImageSkia TaskManagerModel::GetResourceIcon(int index) const {
  gfx::ImageSkia icon = GetResource(index)->GetIcon();
  if (!icon.isNull())
    return icon;

  static gfx::ImageSkia* default_icon = ResourceBundle::GetSharedInstance().
      GetImageSkiaNamed(IDR_DEFAULT_FAVICON);
  return *default_icon;
}

TaskManagerModel::GroupRange
TaskManagerModel::GetGroupRangeForResource(int index) const {
  TaskManager::Resource* resource = GetResource(index);
  GroupMap::const_iterator group_iter =
      group_map_.find(resource->GetProcess());
  DCHECK(group_iter != group_map_.end());
  ResourceList* group = group_iter->second;
  DCHECK(group);
  if (group->size() == 1) {
    return std::make_pair(index, 1);
  } else {
    for (int i = index; i >= 0; --i) {
      if (GetResource(i) == (*group)[0])
        return std::make_pair(i, group->size());
    }
    NOTREACHED();
    return std::make_pair(-1, -1);
  }
}

int TaskManagerModel::GetGroupIndexForResource(int index) const {
  int group_index = -1;
  for (int i = 0; i <= index; ++i) {
    if (IsResourceFirstInGroup(i))
        group_index++;
  }

  DCHECK_NE(group_index, -1);
  return group_index;
}

int TaskManagerModel::GetResourceIndexForGroup(int group_index,
                                               int index_in_group) const {
  int group_count = -1;
  int count_in_group = -1;
  for (int i = 0; i < ResourceCount(); ++i) {
    if (IsResourceFirstInGroup(i))
      group_count++;

    if (group_count == group_index) {
      count_in_group++;
      if (count_in_group == index_in_group)
        return i;
    } else if (group_count > group_index) {
      break;
    }
  }

  NOTREACHED();
  return -1;
}

int TaskManagerModel::CompareValues(int row1, int row2, int col_id) const {
  CHECK(row1 < ResourceCount() && row2 < ResourceCount());
  switch (col_id) {
    case IDS_TASK_MANAGER_TASK_COLUMN: {
      static icu::Collator* collator = NULL;
      if (!collator) {
        UErrorCode create_status = U_ZERO_ERROR;
        collator = icu::Collator::createInstance(create_status);
        if (!U_SUCCESS(create_status)) {
          collator = NULL;
          NOTREACHED();
        }
      }
      const string16& title1 = GetResourceTitle(row1);
      const string16& title2 = GetResourceTitle(row2);
      UErrorCode compare_status = U_ZERO_ERROR;
      UCollationResult compare_result = collator->compare(
          static_cast<const UChar*>(title1.c_str()),
          static_cast<int>(title1.length()),
          static_cast<const UChar*>(title2.c_str()),
          static_cast<int>(title2.length()),
          compare_status);
      DCHECK(U_SUCCESS(compare_status));
      return compare_result;
    }

    case IDS_TASK_MANAGER_PROFILE_NAME_COLUMN: {
      const string16& profile1 = GetResourceProfileName(row1);
      const string16& profile2 = GetResourceProfileName(row2);
      return profile1.compare(0, profile1.length(), profile2, 0,
                              profile2.length());
    }

    case IDS_TASK_MANAGER_NET_COLUMN:
      return ValueCompare(GetNetworkUsage(GetResource(row1)),
                          GetNetworkUsage(GetResource(row2)));

    case IDS_TASK_MANAGER_CPU_COLUMN:
      return ValueCompare(GetCPUUsage(GetResource(row1)),
                          GetCPUUsage(GetResource(row2)));

    case IDS_TASK_MANAGER_PRIVATE_MEM_COLUMN:
      return ValueCompareMember(
          this, &TaskManagerModel::GetPrivateMemory, row1, row2);

    case IDS_TASK_MANAGER_SHARED_MEM_COLUMN:
      return ValueCompareMember(
          this, &TaskManagerModel::GetSharedMemory, row1, row2);

    case IDS_TASK_MANAGER_PHYSICAL_MEM_COLUMN:
      return ValueCompareMember(
          this, &TaskManagerModel::GetPhysicalMemory, row1, row2);

    case IDS_TASK_MANAGER_PROCESS_ID_COLUMN:
      return ValueCompare(GetProcessId(row1), GetProcessId(row2));

    case IDS_TASK_MANAGER_GDI_HANDLES_COLUMN: {
      size_t current1, peak1;
      size_t current2, peak2;
      GetGDIHandles(row1, &current1, &peak1);
      GetGDIHandles(row2, &current2, &peak2);
      return ValueCompare(current1, current2);
    }

    case IDS_TASK_MANAGER_USER_HANDLES_COLUMN: {
      size_t current1, peak1;
      size_t current2, peak2;
      GetUSERHandles(row1, &current1, &peak1);
      GetUSERHandles(row2, &current2, &peak2);
      return ValueCompare(current1, current2);
    }

    case IDS_TASK_MANAGER_WEBCORE_IMAGE_CACHE_COLUMN:
    case IDS_TASK_MANAGER_WEBCORE_SCRIPTS_CACHE_COLUMN:
    case IDS_TASK_MANAGER_WEBCORE_CSS_CACHE_COLUMN: {
      bool row1_stats_valid = CacheWebCoreStats(row1);
      bool row2_stats_valid = CacheWebCoreStats(row2);
      if (row1_stats_valid && row2_stats_valid) {
        const WebKit::WebCache::ResourceTypeStats& stats1(
            GetPerResourceValues(row1).webcore_stats);
        const WebKit::WebCache::ResourceTypeStats& stats2(
            GetPerResourceValues(row2).webcore_stats);
        switch (col_id) {
          case IDS_TASK_MANAGER_WEBCORE_IMAGE_CACHE_COLUMN:
            return ValueCompare(stats1.images.size, stats2.images.size);
          case IDS_TASK_MANAGER_WEBCORE_SCRIPTS_CACHE_COLUMN:
            return ValueCompare(stats1.scripts.size, stats2.scripts.size);
          case IDS_TASK_MANAGER_WEBCORE_CSS_CACHE_COLUMN:
            return ValueCompare(stats1.cssStyleSheets.size,
                                stats2.cssStyleSheets.size);
          default:
            NOTREACHED();
            return 0;
        }
      }
      return OrderUnavailableValue(row1_stats_valid, row2_stats_valid);
    }

    case IDS_TASK_MANAGER_FPS_COLUMN:
      return ValueCompareMember(
          this, &TaskManagerModel::GetFPS, row1, row2);

    case IDS_TASK_MANAGER_VIDEO_MEMORY_COLUMN: {
      size_t value1;
      size_t value2;
      bool has_duplicates;
      bool value1_valid = GetVideoMemory(row1, &value1, &has_duplicates);
      bool value2_valid = GetVideoMemory(row2, &value2, &has_duplicates);
      return value1_valid && value2_valid ? ValueCompare(value1, value2) :
          OrderUnavailableValue(value1_valid, value2_valid);
    }

    case IDS_TASK_MANAGER_GOATS_TELEPORTED_COLUMN:
      return ValueCompare(GetGoatsTeleported(row1), GetGoatsTeleported(row2));

    case IDS_TASK_MANAGER_JAVASCRIPT_MEMORY_ALLOCATED_COLUMN:
      return ValueCompareMember(
          this, &TaskManagerModel::GetV8Memory, row1, row2);

    case IDS_TASK_MANAGER_SQLITE_MEMORY_USED_COLUMN:
      return ValueCompareMember(
          this, &TaskManagerModel::GetSqliteMemoryUsedBytes, row1, row2);

    default:
      NOTREACHED();
      break;
  }
  return 0;
}

int TaskManagerModel::GetUniqueChildProcessId(int index) const {
  return GetResource(index)->GetUniqueChildProcessId();
}

TaskManager::Resource::Type TaskManagerModel::GetResourceType(int index) const {
  return GetResource(index)->GetType();
}

WebContents* TaskManagerModel::GetResourceWebContents(int index) const {
  return GetResource(index)->GetWebContents();
}

const extensions::Extension* TaskManagerModel::GetResourceExtension(
    int index) const {
  return GetResource(index)->GetExtension();
}

void TaskManagerModel::AddResource(TaskManager::Resource* resource) {
  resource->unique_id_ = ++last_unique_id_;

  base::ProcessHandle process = resource->GetProcess();

  ResourceList* group_entries = NULL;
  GroupMap::const_iterator group_iter = group_map_.find(process);
  int new_entry_index = 0;
  if (group_iter == group_map_.end()) {
    group_entries = new ResourceList();
    group_map_[process] = group_entries;
    group_entries->push_back(resource);

    // Not part of a group, just put at the end of the list.
    resources_.push_back(resource);
    new_entry_index = static_cast<int>(resources_.size() - 1);
  } else {
    group_entries = group_iter->second;
    group_entries->push_back(resource);

    // Insert the new entry right after the last entry of its group.
    ResourceList::iterator iter =
        std::find(resources_.begin(),
                  resources_.end(),
                  (*group_entries)[group_entries->size() - 2]);
    DCHECK(iter != resources_.end());
    new_entry_index = static_cast<int>(iter - resources_.begin()) + 1;
    resources_.insert(++iter, resource);
  }

  // Create the ProcessMetrics for this process if needed (not in map).
  if (metrics_map_.find(process) == metrics_map_.end()) {
    base::ProcessMetrics* pm =
#if !defined(OS_MACOSX)
        base::ProcessMetrics::CreateProcessMetrics(process);
#else
        base::ProcessMetrics::CreateProcessMetrics(
            process, content::BrowserChildProcessHost::GetPortProvider());
#endif

    metrics_map_[process] = pm;
  }

  // Notify the table that the contents have changed for it to redraw.
  FOR_EACH_OBSERVER(TaskManagerModelObserver, observer_list_,
                    OnItemsAdded(new_entry_index, 1));
}

void TaskManagerModel::RemoveResource(TaskManager::Resource* resource) {
  base::ProcessHandle process = resource->GetProcess();

  // Find the associated group.
  GroupMap::iterator group_iter = group_map_.find(process);
  DCHECK(group_iter != group_map_.end());
  ResourceList* group_entries = group_iter->second;

  // Remove the entry from the group map.
  ResourceList::iterator iter = std::find(group_entries->begin(),
                                          group_entries->end(),
                                          resource);
  DCHECK(iter != group_entries->end());
  group_entries->erase(iter);

  // If there are no more entries for that process, do the clean-up.
  if (group_entries->empty()) {
    delete group_entries;
    group_map_.erase(process);

    // Nobody is using this process, we don't need the process metrics anymore.
    MetricsMap::iterator pm_iter = metrics_map_.find(process);
    DCHECK(pm_iter != metrics_map_.end());
    if (pm_iter != metrics_map_.end()) {
      delete pm_iter->second;
      metrics_map_.erase(process);
    }
  }

  // Prepare to remove the entry from the model list.
  iter = std::find(resources_.begin(), resources_.end(), resource);
  DCHECK(iter != resources_.end());
  int index = static_cast<int>(iter - resources_.begin());

  // Notify the observers that the contents will change.
  FOR_EACH_OBSERVER(TaskManagerModelObserver, observer_list_,
                    OnItemsToBeRemoved(index, 1));

  // Now actually remove the entry from the model list.
  resources_.erase(iter);

  // Remove the entry from the network maps.
  ResourceValueMap::iterator net_iter =
      current_byte_count_map_.find(resource);
  if (net_iter != current_byte_count_map_.end())
    current_byte_count_map_.erase(net_iter);

  // Notify the table that the contents have changed.
  FOR_EACH_OBSERVER(TaskManagerModelObserver, observer_list_,
                    OnItemsRemoved(index, 1));
}

void TaskManagerModel::StartUpdating() {
  // Multiple StartUpdating requests may come in, and we only need to take
  // action the first time.
  update_requests_++;
  if (update_requests_ > 1)
    return;
  DCHECK_EQ(1, update_requests_);
  DCHECK_NE(TASK_PENDING, update_state_);

  // If update_state_ is STOPPING, it means a task is still pending.  Setting
  // it to TASK_PENDING ensures the tasks keep being posted (by Refresh()).
  if (update_state_ == IDLE) {
      MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&TaskManagerModel::RefreshCallback, this));
  }
  update_state_ = TASK_PENDING;

  // Notify resource providers that we are updating.
  StartListening();

  if (!resources_.empty()) {
    FOR_EACH_OBSERVER(TaskManagerModelObserver, observer_list_,
                      OnReadyPeriodicalUpdate());
  }
}

void TaskManagerModel::StopUpdating() {
  // Don't actually stop updating until we have heard as many calls as those
  // to StartUpdating.
  update_requests_--;
  if (update_requests_ > 0)
    return;
  // Make sure that update_requests_ cannot go negative.
  CHECK_EQ(0, update_requests_);
  DCHECK_EQ(TASK_PENDING, update_state_);
  update_state_ = STOPPING;

  // Notify resource providers that we are done updating.
  StopListening();
}

void TaskManagerModel::StartListening() {
  // Multiple StartListening requests may come in and we only need to take
  // action the first time.
  listen_requests_++;
  if (listen_requests_ > 1)
    return;
  DCHECK_EQ(1, listen_requests_);

  // Notify resource providers that we should start listening to events.
  for (ResourceProviderList::iterator iter = providers_.begin();
       iter != providers_.end(); ++iter) {
    (*iter)->StartUpdating();
  }
}

void TaskManagerModel::StopListening() {
  // Don't actually stop listening until we have heard as many calls as those
  // to StartListening.
  listen_requests_--;
  if (listen_requests_ > 0)
    return;

  DCHECK_EQ(0, listen_requests_);

  // Notify resource providers that we are done listening.
  for (ResourceProviderList::const_iterator iter = providers_.begin();
       iter != providers_.end(); ++iter) {
    (*iter)->StopUpdating();
  }

  // Must clear the resources before the next attempt to start listening.
  Clear();
}

void TaskManagerModel::Clear() {
  int size = ResourceCount();
  if (size > 0) {
    resources_.clear();

    // Clear the groups.
    STLDeleteValues(&group_map_);

    // Clear the process related info.
    STLDeleteValues(&metrics_map_);

    // Clear the network maps.
    current_byte_count_map_.clear();

    per_resource_cache_.clear();
    per_process_cache_.clear();

    FOR_EACH_OBSERVER(TaskManagerModelObserver, observer_list_,
                      OnItemsRemoved(0, size));
  }
  last_unique_id_ = 0;
}

void TaskManagerModel::ModelChanged() {
  // Notify the table that the contents have changed for it to redraw.
  FOR_EACH_OBSERVER(TaskManagerModelObserver, observer_list_, OnModelChanged());
}

void TaskManagerModel::NotifyResourceTypeStats(
    base::ProcessId renderer_id,
    const WebKit::WebCache::ResourceTypeStats& stats) {
  for (ResourceList::iterator it = resources_.begin();
       it != resources_.end(); ++it) {
    if (base::GetProcId((*it)->GetProcess()) == renderer_id) {
      (*it)->NotifyResourceTypeStats(stats);
    }
  }
}

void TaskManagerModel::NotifyFPS(base::ProcessId renderer_id,
                                 int routing_id,
                                 float fps) {
  for (ResourceList::iterator it = resources_.begin();
       it != resources_.end(); ++it) {
    if (base::GetProcId((*it)->GetProcess()) == renderer_id &&
        (*it)->GetRoutingID() == routing_id) {
      (*it)->NotifyFPS(fps);
    }
  }
}

void TaskManagerModel::NotifyVideoMemoryUsageStats(
    const content::GPUVideoMemoryUsageStats& video_memory_usage_stats) {
  DCHECK(pending_video_memory_usage_stats_update_);
  video_memory_usage_stats_ = video_memory_usage_stats;
  pending_video_memory_usage_stats_update_ = false;
}

void TaskManagerModel::NotifyV8HeapStats(base::ProcessId renderer_id,
                                         size_t v8_memory_allocated,
                                         size_t v8_memory_used) {
  for (ResourceList::iterator it = resources_.begin();
       it != resources_.end(); ++it) {
    if (base::GetProcId((*it)->GetProcess()) == renderer_id) {
      (*it)->NotifyV8HeapStats(v8_memory_allocated, v8_memory_used);
    }
  }
}

void TaskManagerModel::NotifyBytesRead(const net::URLRequest& request,
                                       int byte_count) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Only net::URLRequestJob instances created by the ResourceDispatcherHost
  // have an associated ResourceRequestInfo.
  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(&request);

  // have a render view associated.  All other jobs will have -1 returned for
  // the render process child and routing ids - the jobs may still match a
  // resource based on their origin id, otherwise BytesRead() will attribute
  // the activity to the Browser resource.
  int render_process_host_child_id = -1, routing_id = -1;
  if (info)
    info->GetAssociatedRenderView(&render_process_host_child_id, &routing_id);

  // Get the origin PID of the request's originator.  This will only be set for
  // plugins - for renderer or browser initiated requests it will be zero.
  int origin_pid = 0;
  if (info)
    origin_pid = info->GetOriginPID();

  if (bytes_read_buffer_.empty()) {
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&TaskManagerModel::NotifyMultipleBytesRead, this),
        base::TimeDelta::FromSeconds(1));
  }

  bytes_read_buffer_.push_back(
      BytesReadParam(origin_pid, render_process_host_child_id,
                     routing_id, byte_count));
}

TaskManagerModel::~TaskManagerModel() {
}

void TaskManagerModel::RefreshCallback() {
  DCHECK_NE(IDLE, update_state_);

  if (update_state_ == STOPPING) {
    // We have been asked to stop.
    update_state_ = IDLE;
    return;
  }

  Refresh();

  // Schedule the next update.
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&TaskManagerModel::RefreshCallback, this),
      base::TimeDelta::FromMilliseconds(kUpdateTimeMs));
}

void TaskManagerModel::Refresh() {
  goat_salt_ = base::RandUint64();

  per_resource_cache_.clear();
  per_process_cache_.clear();

  // Compute the CPU usage values.
  // Note that we compute the CPU usage for all resources (instead of doing it
  // lazily) as process_util::GetCPUUsage() returns the CPU usage since the last
  // time it was called, and not calling it everytime would skew the value the
  // next time it is retrieved (as it would be for more than 1 cycle).
  for (ResourceList::iterator iter = resources_.begin();
       iter != resources_.end(); ++iter) {
    base::ProcessHandle process = (*iter)->GetProcess();
    PerProcessValues& values(per_process_cache_[process]);
    if (values.is_cpu_usage_valid)
      continue;

    values.is_cpu_usage_valid = true;
    MetricsMap::iterator metrics_iter = metrics_map_.find(process);
    DCHECK(metrics_iter != metrics_map_.end());
    values.cpu_usage = metrics_iter->second->GetCPUUsage();
  }

  // Send a request to refresh GPU memory consumption values
  RefreshVideoMemoryUsageStats();

  // Compute the new network usage values.
  base::TimeDelta update_time =
      base::TimeDelta::FromMilliseconds(kUpdateTimeMs);
  for (ResourceValueMap::iterator iter = current_byte_count_map_.begin();
       iter != current_byte_count_map_.end(); ++iter) {
    PerResourceValues* values = &(per_resource_cache_[iter->first]);
    if (update_time > base::TimeDelta::FromSeconds(1))
      values->network_usage = iter->second / update_time.InSeconds();
    else
      values->network_usage = iter->second * (1 / update_time.InSeconds());

    // Then we reset the current byte count.
    iter->second = 0;
  }

  // Let resources update themselves if they need to.
  for (ResourceList::iterator iter = resources_.begin();
       iter != resources_.end(); ++iter) {
     (*iter)->Refresh();
  }

  if (!resources_.empty()) {
    FOR_EACH_OBSERVER(TaskManagerModelObserver, observer_list_,
                      OnItemsChanged(0, ResourceCount()));
  }
}

void TaskManagerModel::RefreshVideoMemoryUsageStats() {
  if (pending_video_memory_usage_stats_update_)
    return;

  if (!video_memory_usage_stats_observer_.get()) {
    video_memory_usage_stats_observer_.reset(
        new TaskManagerModelGpuDataManagerObserver());
  }
  pending_video_memory_usage_stats_update_ = true;
  content::GpuDataManager::GetInstance()->RequestVideoMemoryUsageStatsUpdate();
}

int64 TaskManagerModel::GetNetworkUsageForResource(
    TaskManager::Resource* resource) const {
  // Returns default of 0 if no network usage.
  return per_resource_cache_[resource].network_usage;
}

void TaskManagerModel::BytesRead(BytesReadParam param) {
  if (update_state_ != TASK_PENDING || listen_requests_ == 0) {
    // A notification sneaked in while we were stopping the updating, just
    // ignore it.
    return;
  }

  // TODO(jcampan): this should be improved once we have a better way of
  // linking a network notification back to the object that initiated it.
  TaskManager::Resource* resource = NULL;
  for (ResourceProviderList::iterator iter = providers_.begin();
       iter != providers_.end(); ++iter) {
    resource = (*iter)->GetResource(param.origin_pid,
                                    param.render_process_host_child_id,
                                    param.routing_id);
    if (resource)
      break;
  }

  if (resource == NULL) {
    // We can't match a resource to the notification.  That might mean the
    // tab that started a download was closed, or the request may have had
    // no originating resource associated with it in the first place.
    // We attribute orphaned/unaccounted activity to the Browser process.
    CHECK(param.origin_pid || (param.render_process_host_child_id != -1));
    param.origin_pid = 0;
    param.render_process_host_child_id = param.routing_id = -1;
    BytesRead(param);
    return;
  }

  // We do support network usage, mark the resource as such so it can report 0
  // instead of N/A.
  if (!resource->SupportNetworkUsage())
    resource->SetSupportNetworkUsage();

  ResourceValueMap::const_iterator iter_res =
      current_byte_count_map_.find(resource);
  if (iter_res == current_byte_count_map_.end())
    current_byte_count_map_[resource] = param.byte_count;
  else
    current_byte_count_map_[resource] = iter_res->second + param.byte_count;
}

void TaskManagerModel::MultipleBytesRead(
    const std::vector<BytesReadParam>* params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  for (std::vector<BytesReadParam>::const_iterator it = params->begin();
       it != params->end(); ++it) {
    BytesRead(*it);
  }
}

void TaskManagerModel::NotifyMultipleBytesRead() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!bytes_read_buffer_.empty());

  std::vector<BytesReadParam>* bytes_read_buffer =
      new std::vector<BytesReadParam>;
  bytes_read_buffer_.swap(*bytes_read_buffer);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&TaskManagerModel::MultipleBytesRead, this,
                 base::Owned(bytes_read_buffer)));
}

int64 TaskManagerModel::GetNetworkUsage(TaskManager::Resource* resource) const {
  int64 net_usage = GetNetworkUsageForResource(resource);
  if (net_usage == 0 && !resource->SupportNetworkUsage())
    return -1;
  return net_usage;
}

double TaskManagerModel::GetCPUUsage(TaskManager::Resource* resource) const {
  const PerProcessValues& values(per_process_cache_[resource->GetProcess()]);
  // Returns 0 if not valid, which is fine.
  return values.cpu_usage;
}

string16 TaskManagerModel::GetMemCellText(int64 number) const {
#if !defined(OS_MACOSX)
  string16 str = base::FormatNumber(number / 1024);

  // Adjust number string if necessary.
  base::i18n::AdjustStringForLocaleDirection(&str);
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_MEM_CELL_TEXT, str);
#else
  // System expectation is to show "100 kB", "200 MB", etc.
  // TODO(thakis): Switch to metric units (as opposed to powers of two).
  return ui::FormatBytes(number);
#endif
}

bool TaskManagerModel::CachePrivateAndSharedMemory(
    base::ProcessHandle handle) const {
  PerProcessValues& values(per_process_cache_[handle]);
  if (values.is_private_and_shared_valid)
    return true;

  MetricsMap::const_iterator iter = metrics_map_.find(handle);
  if (iter == metrics_map_.end() ||
      !iter->second->GetMemoryBytes(&values.private_bytes,
                                    &values.shared_bytes)) {
    return false;
  }

  values.is_private_and_shared_valid = true;
  return true;
}

bool TaskManagerModel::CacheWebCoreStats(int index) const {
  PerResourceValues& values(GetPerResourceValues(index));
  if (!values.is_webcore_stats_valid) {
    if (!GetResource(index)->ReportsCacheStats())
      return false;
    values.is_webcore_stats_valid = true;
    values.webcore_stats = GetResource(index)->GetWebCoreCacheStats();
  }
  return true;
}

bool TaskManagerModel::CacheV8Memory(int index) const {
  PerResourceValues& values(GetPerResourceValues(index));
  if (!values.is_v8_memory_valid) {
    if (!GetResource(index)->ReportsV8MemoryStats())
      return false;
    values.is_v8_memory_valid = true;
    values.v8_memory_allocated = GetResource(index)->GetV8MemoryAllocated();
    values.v8_memory_used = GetResource(index)->GetV8MemoryUsed();
  }
  return true;
}

void TaskManagerModel::AddResourceProvider(
    TaskManager::ResourceProvider* provider) {
  DCHECK(provider);
  providers_.push_back(provider);
}

TaskManagerModel::PerResourceValues& TaskManagerModel::GetPerResourceValues(
    int index) const {
  return per_resource_cache_[GetResource(index)];
}

TaskManager::Resource* TaskManagerModel::GetResource(int index) const {
  CHECK_GE(index, 0);
  CHECK_LT(index, static_cast<int>(resources_.size()));
  return resources_[index];
}

////////////////////////////////////////////////////////////////////////////////
// TaskManager class
////////////////////////////////////////////////////////////////////////////////

int TaskManager::Resource::GetRoutingID() const { return 0; }

bool TaskManager::Resource::ReportsCacheStats() const { return false; }

WebKit::WebCache::ResourceTypeStats
TaskManager::Resource::GetWebCoreCacheStats() const {
  return WebKit::WebCache::ResourceTypeStats();
}

bool TaskManager::Resource::ReportsFPS() const { return false; }

float TaskManager::Resource::GetFPS() const { return 0.0f; }

bool TaskManager::Resource::ReportsSqliteMemoryUsed() const { return false; }

size_t TaskManager::Resource::SqliteMemoryUsedBytes() const { return 0; }

const extensions::Extension* TaskManager::Resource::GetExtension() const {
  return NULL;
}

bool TaskManager::Resource::ReportsV8MemoryStats() const { return false; }

size_t TaskManager::Resource::GetV8MemoryAllocated() const { return 0; }

size_t TaskManager::Resource::GetV8MemoryUsed() const { return 0; }

bool TaskManager::Resource::CanInspect() const { return false; }

content::WebContents* TaskManager::Resource::GetWebContents() const {
  return NULL;
}

bool TaskManager::Resource::IsBackground() const { return false; }

// static
void TaskManager::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kTaskManagerWindowPlacement);
}

bool TaskManager::IsBrowserProcess(int index) const {
  // If some of the selection is out of bounds, ignore. This may happen when
  // killing a process that manages several pages.
  return index < model_->ResourceCount() &&
      model_->GetProcess(index) == base::GetCurrentProcessHandle();
}

void TaskManager::KillProcess(int index) {
  base::ProcessHandle process = model_->GetProcess(index);
  DCHECK(process);
  if (process != base::GetCurrentProcessHandle())
    base::KillProcess(process, content::RESULT_CODE_KILLED, false);
}

void TaskManager::ActivateProcess(int index) {
  // GetResourceWebContents returns a pointer to the relevant web contents for
  // the resource.  If the index doesn't correspond to any web contents
  // (i.e. refers to the Browser process or a plugin), GetWebContents will
  // return NULL.
  WebContents* chosen_web_contents = model_->GetResourceWebContents(index);
  if (chosen_web_contents && chosen_web_contents->GetDelegate())
    chosen_web_contents->GetDelegate()->ActivateContents(chosen_web_contents);
}

void TaskManager::AddResource(Resource* resource) {
  model_->AddResource(resource);
}

void TaskManager::RemoveResource(Resource* resource) {
  model_->RemoveResource(resource);
}

void TaskManager::OnWindowClosed() {
  model_->StopUpdating();
}

void TaskManager::ModelChanged() {
  model_->ModelChanged();
}

// static
TaskManager* TaskManager::GetInstance() {
  return Singleton<TaskManager>::get();
}

void TaskManager::OpenAboutMemory(chrome::HostDesktopType desktop_type) {
  Browser* browser = chrome::FindOrCreateTabbedBrowser(
      ProfileManager::GetDefaultProfileOrOffTheRecord(), desktop_type);
  chrome::NavigateParams params(browser, GURL(chrome::kChromeUIMemoryURL),
                                content::PAGE_TRANSITION_LINK);
  params.disposition = NEW_FOREGROUND_TAB;
  chrome::Navigate(&params);
}

namespace {

// Counts the number of extension background pages associated with this profile.
int CountExtensionBackgroundPagesForProfile(Profile* profile) {
  int count = 0;
  ExtensionProcessManager* manager =
      extensions::ExtensionSystem::Get(profile)->process_manager();
  if (!manager)
    return count;

  const ExtensionProcessManager::ExtensionHostSet& background_hosts =
      manager->background_hosts();
  for (ExtensionProcessManager::const_iterator iter = background_hosts.begin();
       iter != background_hosts.end(); ++iter) {
    ++count;
  }
  return count;
}

}  // namespace

// static
int TaskManager::GetBackgroundPageCount() {
  int count = 0;
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager)  // Null when running unit tests.
    return count;
  std::vector<Profile*> profiles(profile_manager->GetLoadedProfiles());
  for (std::vector<Profile*>::const_iterator iter = profiles.begin();
       iter != profiles.end();
       ++iter) {
    Profile* profile = *iter;
    // Count the number of Background Contents (background pages for hosted
    // apps). Incognito windows do not support hosted apps, so just check the
    // main profile.
    BackgroundContentsService* background_contents_service =
        BackgroundContentsServiceFactory::GetForProfile(profile);
    if (background_contents_service)
      count += background_contents_service->GetBackgroundContents().size();

    // Count the number of extensions with background pages (including
    // incognito).
    count += CountExtensionBackgroundPagesForProfile(profile);
    if (profile->HasOffTheRecordProfile()) {
      count += CountExtensionBackgroundPagesForProfile(
          profile->GetOffTheRecordProfile());
    }
  }
  return count;
}

TaskManager::TaskManager()
    : ALLOW_THIS_IN_INITIALIZER_LIST(model_(new TaskManagerModel(this))) {
}

TaskManager::~TaskManager() {
}
