// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/new_task_manager_view.h"

#include <map>

#include "base/i18n/number_formatting.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/task_management/task_manager_interface.h"
#include "chrome/browser/task_management/task_manager_observer.h"
#include "chrome/browser/ui/user_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/nacl/browser/nacl_browser.h"
#include "content/public/common/result_codes.h"
#include "third_party/WebKit/public/web/WebCache.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/table_model_observer.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/table/table_grouper.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

#if defined(USE_ASH)
#include "ash/shelf/shelf_util.h"
#include "ash/wm/window_util.h"
#include "grit/ash_resources.h"
#endif  // defined(USE_ASH)

#if defined(OS_WIN)
#include "chrome/browser/shell_integration.h"
#include "ui/base/win/shell.h"
#include "ui/views/win/hwnd_util.h"
#endif  // defined(OS_WIN)

namespace task_management {

namespace {

NewTaskManagerView* g_task_manager_view = nullptr;

#if defined(OS_MACOSX)
// Match Activity Monitor's default refresh rate.
const int64 kRefreshTimeMS = 2000;

// Activity Monitor shows %cpu with one decimal digit -- be consistent with
// that.
const char kCpuTextFormatString[] = "%.1f";
#else
const int64 kRefreshTimeMS = 1000;
const char kCpuTextFormatString[] = "%.0f";
#endif  // defined(OS_MACOSX)

// The columns that are shared by a group will show the value of the column
// only once per group.
bool IsSharedByGroup(int column_id) {
  switch (column_id) {
    case IDS_TASK_MANAGER_PRIVATE_MEM_COLUMN:
    case IDS_TASK_MANAGER_SHARED_MEM_COLUMN:
    case IDS_TASK_MANAGER_PHYSICAL_MEM_COLUMN:
    case IDS_TASK_MANAGER_CPU_COLUMN:
    case IDS_TASK_MANAGER_PROCESS_ID_COLUMN:
    case IDS_TASK_MANAGER_JAVASCRIPT_MEMORY_ALLOCATED_COLUMN:
    case IDS_TASK_MANAGER_VIDEO_MEMORY_COLUMN:
    case IDS_TASK_MANAGER_SQLITE_MEMORY_USED_COLUMN:
    case IDS_TASK_MANAGER_WEBCORE_IMAGE_CACHE_COLUMN:
    case IDS_TASK_MANAGER_WEBCORE_SCRIPTS_CACHE_COLUMN:
    case IDS_TASK_MANAGER_WEBCORE_CSS_CACHE_COLUMN:
    case IDS_TASK_MANAGER_NACL_DEBUG_STUB_PORT_COLUMN:
      return true;
    default:
      return false;
  }
}

// Opens the "about:memory" for the "stats for nerds" link.
void OpenAboutMemory(chrome::HostDesktopType desktop_type) {
  Profile* profile = ProfileManager::GetLastUsedProfileAllowedByPolicy();
  if (profile->IsGuestSession() &&
      !g_browser_process->local_state()->GetBoolean(
          prefs::kBrowserGuestModeEnabled)) {
    UserManager::Show(base::FilePath(),
                      profiles::USER_MANAGER_NO_TUTORIAL,
                      profiles::USER_MANAGER_SELECT_PROFILE_CHROME_MEMORY);
    return;
  }

  chrome::NavigateParams params(profile,
                                GURL(chrome::kChromeUIMemoryURL),
                                ui::PAGE_TRANSITION_LINK);
  params.disposition = NEW_FOREGROUND_TAB;
  params.host_desktop_type = desktop_type;
  chrome::Navigate(&params);
}

// Used to sort various column values.
template <class T>
int ValueCompare(T value1, T value2) {
  if (value1 == value2)
    return 0;
  return value1 < value2 ? -1 : 1;
}

// Used when one or both of the results to compare are unavailable.
int OrderUnavailableValue(bool v1, bool v2) {
  if (!v1 && !v2)
    return 0;
  return v1 ? 1 : -1;
}

// A class to stringify the task manager's values into string16s and to
// cache the common strings that will be reused many times like "N/A" and so on.
class TaskManagerValuesStringifier {
 public:
  TaskManagerValuesStringifier()
      : n_a_string_(l10n_util::GetStringUTF16(IDS_TASK_MANAGER_NA_CELL_TEXT)),
        zero_string_(base::ASCIIToUTF16("0")),
        asterisk_string_(base::ASCIIToUTF16("*")),
        unknown_string_(l10n_util::GetStringUTF16(
            IDS_TASK_MANAGER_UNKNOWN_VALUE_TEXT)) {
  }

  ~TaskManagerValuesStringifier() {}

  base::string16 GetCpuUsageText(double cpu_usage) {
    return base::UTF8ToUTF16(base::StringPrintf(kCpuTextFormatString,
                                                cpu_usage));
  }

  base::string16 GetMemoryUsageText(int64 memory_usage, bool has_duplicates) {
    if (memory_usage == -1)
      return n_a_string_;

#if defined(OS_MACOSX)
    // System expectation is to show "100 kB", "200 MB", etc.
    // TODO(thakis): [This TODO has been taken as is from the old task manager]:
    // Switch to metric units (as opposed to powers of two).
    base::string16 memory_text = ui::FormatBytes(memory_usage);
#else
    base::string16 memory_text = base::FormatNumber(memory_usage / 1024);
    // Adjust number string if necessary.
    base::i18n::AdjustStringForLocaleDirection(&memory_text);
    memory_text = l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_MEM_CELL_TEXT,
                                             memory_text);
#endif  // defined(OS_MACOSX)

    if (has_duplicates)
      memory_text += asterisk_string_;

    return memory_text;
  }

  base::string16 GetIdleWakeupsText(int idle_wakeups) {
    if (idle_wakeups == -1)
      return n_a_string_;

    return base::FormatNumber(idle_wakeups);
  }

  base::string16 GetNaClPortText(int nacl_port) {
    if (nacl_port == nacl::kGdbDebugStubPortUnused)
      return n_a_string_;

    if (nacl_port == nacl::kGdbDebugStubPortUnknown)
      return unknown_string_;

    return base::IntToString16(nacl_port);
  }

  base::string16 GetWindowsHandlesText(int64 current, int64 peak) {
    return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_HANDLES_CELL_TEXT,
                                      base::IntToString16(current),
                                      base::IntToString16(peak));
  }

  base::string16 GetNetworkUsageText(int64 network_usage) {
    if (network_usage == -1)
      return n_a_string_;

    if (network_usage == 0)
      return zero_string_;

    base::string16 net_byte = ui::FormatSpeed(network_usage);
    // Force number string to have LTR directionality.
    return base::i18n::GetDisplayStringInLTRDirectionality(net_byte);
  }

  base::string16 GetProcessIdText(base::ProcessId proc_id) {
    return base::IntToString16(proc_id);
  }

  base::string16 FormatAllocatedAndUsedMemory(int64 allocated, int64 used) {
    return l10n_util::GetStringFUTF16(
        IDS_TASK_MANAGER_CACHE_SIZE_CELL_TEXT,
        ui::FormatBytesWithUnits(allocated, ui::DATA_UNITS_KIBIBYTE, false),
        ui::FormatBytesWithUnits(used, ui::DATA_UNITS_KIBIBYTE, false));
  }

  base::string16 GetWebCacheStatText(
      const blink::WebCache::ResourceTypeStat& stat) {
    return FormatAllocatedAndUsedMemory(stat.size, stat.liveSize);
  }

  const base::string16& n_a_string() const { return n_a_string_; }
  const base::string16& zero_string() const { return zero_string_; }
  const base::string16& asterisk_string() const { return asterisk_string_; }
  const base::string16& unknown_string() const { return unknown_string_; }

 private:
  // The localized string "N/A".
  const base::string16 n_a_string_;

  // The value 0 as a string "0".
  const base::string16 zero_string_;

  // The string "*" that is used to show that there exists duplicates in the
  // GPU memory.
  const base::string16 asterisk_string_;

  // The string "Unknown".
  const base::string16 unknown_string_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerValuesStringifier);
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////

// The table model of the task manager table view that will observe the
// task manager backend and adapt its interface to match the requirements of the
// TableView.
class NewTaskManagerView::TableModel
    : public TaskManagerObserver,
      public ui::TableModel,
      public views::TableGrouper {
 public:
  explicit TableModel(int64 refresh_flags);
  ~TableModel() override;

  // ui::TableModel:
  int RowCount() override;
  base::string16 GetText(int row, int column) override;
  gfx::ImageSkia GetIcon(int row) override;
  void SetObserver(ui::TableModelObserver* observer) override;
  int CompareValues(int row1, int row2, int column_id) override;

  // views::TableGrouper:
  void GetGroupRange(int model_index, views::GroupRange* range) override;

  // task_management::TaskManagerObserver:
  void OnTaskAdded(TaskId id) override;
  void OnTaskToBeRemoved(TaskId id) override;
  void OnTasksRefreshed(const TaskIdList& task_ids) override;

  // Start / stop observing the task manager.
  void StartUpdating();
  void StopUpdating();

  // Activates the browser tab associated with the process in the specified
  // |row_index|.
  void ActivateTask(int row_index);

  // Kills the process on which the task at |row_index| is running.
  void KillTask(int row_index);

  // Called when a column visibility is toggled from the context menu of the
  // table view. This will result in enabling or disabling some resources
  // refresh types in the task manager.
  void ToggleColumnVisibility(views::TableView* table, int column_id);

  // Checks if the task at |row_index| is running on the browser process.
  bool IsBrowserProcess(int row_index) const;

 private:
  void OnRefresh();

  // Checks whether the task at |row_index| is the first task in its process
  // group of tasks.
  bool IsTaskFirstInGroup(int row_index) const;

  // The table model observer that will be set by the table view of the task
  // manager.
  ui::TableModelObserver* table_model_observer_;

  // The sorted list of task IDs by process ID then by task ID.
  std::vector<TaskId> tasks_;

  // The owned task manager values stringifier that will be used to convert the
  // values to string16.
  TaskManagerValuesStringifier stringifier_;

  DISALLOW_COPY_AND_ASSIGN(TableModel);
};

////////////////////////////////////////////////////////////////////////////////
// NewTaskManagerView::TableModel Implementation:
////////////////////////////////////////////////////////////////////////////////

NewTaskManagerView::TableModel::TableModel(int64 refresh_flags)
    : TaskManagerObserver(base::TimeDelta::FromMilliseconds(kRefreshTimeMS),
                          refresh_flags),
      table_model_observer_(nullptr) {
}

NewTaskManagerView::TableModel::~TableModel() {
}

int NewTaskManagerView::TableModel::RowCount() {
  return static_cast<int>(tasks_.size());
}

base::string16 NewTaskManagerView::TableModel::GetText(int row, int column) {
  if (IsSharedByGroup(column) && !IsTaskFirstInGroup(row))
    return base::string16();

  switch (column) {
    case IDS_TASK_MANAGER_TASK_COLUMN:
      return observed_task_manager()->GetTitle(tasks_[row]);

    case IDS_TASK_MANAGER_PROFILE_NAME_COLUMN:
      return observed_task_manager()->GetProfileName(tasks_[row]);

    case IDS_TASK_MANAGER_NET_COLUMN:
      return stringifier_.GetNetworkUsageText(
          observed_task_manager()->GetNetworkUsage(tasks_[row]));

    case IDS_TASK_MANAGER_CPU_COLUMN:
      return stringifier_.GetCpuUsageText(
          observed_task_manager()->GetCpuUsage(tasks_[row]));

    case IDS_TASK_MANAGER_PRIVATE_MEM_COLUMN:
      return stringifier_.GetMemoryUsageText(
          observed_task_manager()->GetPrivateMemoryUsage(tasks_[row]), false);

    case IDS_TASK_MANAGER_SHARED_MEM_COLUMN:
      return stringifier_.GetMemoryUsageText(
          observed_task_manager()->GetSharedMemoryUsage(tasks_[row]), false);

    case IDS_TASK_MANAGER_PHYSICAL_MEM_COLUMN:
      return stringifier_.GetMemoryUsageText(
          observed_task_manager()->GetPhysicalMemoryUsage(tasks_[row]), false);

    case IDS_TASK_MANAGER_PROCESS_ID_COLUMN:
      return stringifier_.GetProcessIdText(
          observed_task_manager()->GetProcessId(tasks_[row]));

    case IDS_TASK_MANAGER_GDI_HANDLES_COLUMN: {
      int64 current, peak;
      observed_task_manager()->GetGDIHandles(tasks_[row], &current, &peak);
      return stringifier_.GetWindowsHandlesText(current, peak);
    }

    case IDS_TASK_MANAGER_USER_HANDLES_COLUMN: {
      int64 current, peak;
      observed_task_manager()->GetUSERHandles(tasks_[row], &current, &peak);
      return stringifier_.GetWindowsHandlesText(current, peak);
    }

    case IDS_TASK_MANAGER_IDLE_WAKEUPS_COLUMN:
      return stringifier_.GetIdleWakeupsText(
          observed_task_manager()->GetIdleWakeupsPerSecond(tasks_[row]));

    case IDS_TASK_MANAGER_WEBCORE_IMAGE_CACHE_COLUMN: {
      blink::WebCache::ResourceTypeStats stats;
      if (observed_task_manager()->GetWebCacheStats(tasks_[row], &stats))
        return stringifier_.GetWebCacheStatText(stats.images);
      return stringifier_.n_a_string();
    }

    case IDS_TASK_MANAGER_WEBCORE_SCRIPTS_CACHE_COLUMN: {
      blink::WebCache::ResourceTypeStats stats;
      if (observed_task_manager()->GetWebCacheStats(tasks_[row], &stats))
        return stringifier_.GetWebCacheStatText(stats.scripts);
      return stringifier_.n_a_string();
    }

    case IDS_TASK_MANAGER_WEBCORE_CSS_CACHE_COLUMN: {
      blink::WebCache::ResourceTypeStats stats;
      if (observed_task_manager()->GetWebCacheStats(tasks_[row], &stats))
        return stringifier_.GetWebCacheStatText(stats.cssStyleSheets);
      return stringifier_.n_a_string();
    }

    case IDS_TASK_MANAGER_VIDEO_MEMORY_COLUMN: {
      bool has_duplicates = false;
      return stringifier_.GetMemoryUsageText(
          observed_task_manager()->GetGpuMemoryUsage(tasks_[row],
                                                     &has_duplicates),
          has_duplicates);
    }

    case IDS_TASK_MANAGER_SQLITE_MEMORY_USED_COLUMN:
      return stringifier_.GetMemoryUsageText(
          observed_task_manager()->GetSqliteMemoryUsed(tasks_[row]), false);

    case IDS_TASK_MANAGER_JAVASCRIPT_MEMORY_ALLOCATED_COLUMN: {
      int64 v8_allocated, v8_used;
      if (observed_task_manager()->GetV8Memory(tasks_[row],
                                               &v8_allocated,
                                               &v8_used)) {
        return stringifier_.FormatAllocatedAndUsedMemory(v8_allocated,
                                                          v8_used);
      }
      return stringifier_.n_a_string();
    }

    case IDS_TASK_MANAGER_NACL_DEBUG_STUB_PORT_COLUMN:
      return stringifier_.GetNaClPortText(
          observed_task_manager()->GetNaClDebugStubPort(tasks_[row]));

    default:
      NOTREACHED();
      return base::string16();
  }
}

gfx::ImageSkia NewTaskManagerView::TableModel::GetIcon(int row) {
  return observed_task_manager()->GetIcon(tasks_[row]);
}

void NewTaskManagerView::TableModel::SetObserver(
    ui::TableModelObserver* observer) {
  table_model_observer_ = observer;
}

int NewTaskManagerView::TableModel::CompareValues(int row1,
                                                  int row2,
                                                  int column_id) {
  switch (column_id) {
    case IDS_TASK_MANAGER_TASK_COLUMN:
    case IDS_TASK_MANAGER_PROFILE_NAME_COLUMN:
      return TableModel::CompareValues(row1, row2, column_id);

    case IDS_TASK_MANAGER_NET_COLUMN:
      return ValueCompare(
          observed_task_manager()->GetNetworkUsage(tasks_[row1]),
          observed_task_manager()->GetNetworkUsage(tasks_[row2]));

    case IDS_TASK_MANAGER_CPU_COLUMN:
      return ValueCompare(observed_task_manager()->GetCpuUsage(tasks_[row1]),
                          observed_task_manager()->GetCpuUsage(tasks_[row2]));

    case IDS_TASK_MANAGER_PRIVATE_MEM_COLUMN:
      return ValueCompare(
          observed_task_manager()->GetPrivateMemoryUsage(tasks_[row1]),
          observed_task_manager()->GetPrivateMemoryUsage(tasks_[row2]));

    case IDS_TASK_MANAGER_SHARED_MEM_COLUMN:
      return ValueCompare(
          observed_task_manager()->GetSharedMemoryUsage(tasks_[row1]),
          observed_task_manager()->GetSharedMemoryUsage(tasks_[row2]));

    case IDS_TASK_MANAGER_PHYSICAL_MEM_COLUMN:
      return ValueCompare(
          observed_task_manager()->GetPhysicalMemoryUsage(tasks_[row1]),
          observed_task_manager()->GetPhysicalMemoryUsage(tasks_[row2]));

    case IDS_TASK_MANAGER_NACL_DEBUG_STUB_PORT_COLUMN:
      return ValueCompare(
          observed_task_manager()->GetNaClDebugStubPort(tasks_[row1]),
          observed_task_manager()->GetNaClDebugStubPort(tasks_[row2]));

    case IDS_TASK_MANAGER_PROCESS_ID_COLUMN:
      return ValueCompare(observed_task_manager()->GetProcessId(tasks_[row1]),
                          observed_task_manager()->GetProcessId(tasks_[row2]));

    case IDS_TASK_MANAGER_GDI_HANDLES_COLUMN: {
      int64 current1, peak1, current2, peak2;
      observed_task_manager()->GetGDIHandles(tasks_[row1], &current1, &peak1);
      observed_task_manager()->GetGDIHandles(tasks_[row2], &current2, &peak2);
      return ValueCompare(current1, current2);
    }

    case IDS_TASK_MANAGER_USER_HANDLES_COLUMN: {
      int64 current1, peak1, current2, peak2;
      observed_task_manager()->GetUSERHandles(tasks_[row1], &current1, &peak1);
      observed_task_manager()->GetUSERHandles(tasks_[row2], &current2, &peak2);
      return ValueCompare(current1, current2);
    }

    case IDS_TASK_MANAGER_IDLE_WAKEUPS_COLUMN:
      return ValueCompare(
          observed_task_manager()->GetIdleWakeupsPerSecond(tasks_[row1]),
          observed_task_manager()->GetIdleWakeupsPerSecond(tasks_[row2]));

    case IDS_TASK_MANAGER_WEBCORE_IMAGE_CACHE_COLUMN:
    case IDS_TASK_MANAGER_WEBCORE_SCRIPTS_CACHE_COLUMN:
    case IDS_TASK_MANAGER_WEBCORE_CSS_CACHE_COLUMN: {
      blink::WebCache::ResourceTypeStats stats1;
      blink::WebCache::ResourceTypeStats stats2;
      bool row1_stats_valid =
          observed_task_manager()->GetWebCacheStats(tasks_[row1], &stats1);
      bool row2_stats_valid =
          observed_task_manager()->GetWebCacheStats(tasks_[row2], &stats2);
      if (!row1_stats_valid || !row2_stats_valid)
        return OrderUnavailableValue(row1_stats_valid, row2_stats_valid);

      switch (column_id) {
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

    case IDS_TASK_MANAGER_VIDEO_MEMORY_COLUMN: {
      bool has_duplicates;
      return ValueCompare(
          observed_task_manager()->GetGpuMemoryUsage(tasks_[row1],
                                                     &has_duplicates),
          observed_task_manager()->GetGpuMemoryUsage(tasks_[row2],
                                                     &has_duplicates));
    }

    case IDS_TASK_MANAGER_JAVASCRIPT_MEMORY_ALLOCATED_COLUMN: {
      int64 allocated1, allocated2, used1, used2;
      observed_task_manager()->GetV8Memory(tasks_[row1], &allocated1, &used1);
      observed_task_manager()->GetV8Memory(tasks_[row2], &allocated2, &used2);
      return ValueCompare(allocated1, allocated2);
    }

    case IDS_TASK_MANAGER_SQLITE_MEMORY_USED_COLUMN:
      return ValueCompare(
          observed_task_manager()->GetSqliteMemoryUsed(tasks_[row1]),
          observed_task_manager()->GetSqliteMemoryUsed(tasks_[row2]));

    default:
      NOTREACHED();
      return 0;
  }
}

void NewTaskManagerView::TableModel::GetGroupRange(int model_index,
                                             views::GroupRange* range) {
  int i = model_index;
  for ( ; i >= 0; --i) {
    if (IsTaskFirstInGroup(i))
      break;
  }

  CHECK_GE(i, 0);

  range->start = i;
  range->length = observed_task_manager()->GetNumberOfTasksOnSameProcess(
      tasks_[model_index]);
}

void NewTaskManagerView::TableModel::OnTaskAdded(TaskId id) {
  // For the table view scrollbar to behave correctly we must inform it that
  // a new task has been added.

  // We will get a newly sorted list from the task manager as opposed to just
  // adding |id| to |tasks_| because we want to keep |tasks_| sorted by proc IDs
  // and then by Task IDs.
  tasks_ = observed_task_manager()->GetTaskIdsList();
  if (table_model_observer_)
    table_model_observer_->OnItemsAdded(RowCount() - 1, 1);
}

void NewTaskManagerView::TableModel::OnTaskToBeRemoved(TaskId id) {
  auto index = std::find(tasks_.begin(), tasks_.end(), id);
  if (index == tasks_.end())
    return;
  auto removed_index = index - tasks_.begin();
  tasks_.erase(index);
  if (table_model_observer_)
    table_model_observer_->OnItemsRemoved(removed_index, 1);
}

void NewTaskManagerView::TableModel::OnTasksRefreshed(
    const TaskIdList& task_ids) {
  tasks_ = task_ids;
  OnRefresh();
}

void NewTaskManagerView::TableModel::StartUpdating() {
  TaskManagerInterface::GetTaskManager()->AddObserver(this);
  tasks_ = observed_task_manager()->GetTaskIdsList();
  OnRefresh();
}

void NewTaskManagerView::TableModel::StopUpdating() {
  observed_task_manager()->RemoveObserver(this);
}

void NewTaskManagerView::TableModel::ActivateTask(int row_index) {
  observed_task_manager()->ActivateTask(tasks_[row_index]);
}

void NewTaskManagerView::TableModel::KillTask(int row_index) {
  base::ProcessId proc_id = observed_task_manager()->GetProcessId(
      tasks_[row_index]);

  DCHECK_NE(proc_id, base::GetCurrentProcId());

  base::Process process = base::Process::Open(proc_id);
  process.Terminate(content::RESULT_CODE_KILLED, false);
}

void NewTaskManagerView::TableModel::ToggleColumnVisibility(
    views::TableView* table,
    int column_id) {
  DCHECK(table);

  bool new_visibility = !table->IsColumnVisible(column_id);
  table->SetColumnVisibility(column_id, new_visibility);

  RefreshType type = REFRESH_TYPE_NONE;
  switch (column_id) {
    case IDS_TASK_MANAGER_NET_COLUMN:
      type = REFRESH_TYPE_NETWORK_USAGE;
      break;

    case IDS_TASK_MANAGER_CPU_COLUMN:
      type = REFRESH_TYPE_CPU;
      break;

    case IDS_TASK_MANAGER_PRIVATE_MEM_COLUMN:
    case IDS_TASK_MANAGER_SHARED_MEM_COLUMN:
    case IDS_TASK_MANAGER_PHYSICAL_MEM_COLUMN:
      type = REFRESH_TYPE_MEMORY;
      if (table->IsColumnVisible(IDS_TASK_MANAGER_PRIVATE_MEM_COLUMN) ||
          table->IsColumnVisible(IDS_TASK_MANAGER_SHARED_MEM_COLUMN) ||
          table->IsColumnVisible(IDS_TASK_MANAGER_SHARED_MEM_COLUMN)) {
        new_visibility = true;
      }
      break;

    case IDS_TASK_MANAGER_GDI_HANDLES_COLUMN:
    case IDS_TASK_MANAGER_USER_HANDLES_COLUMN:
      type = REFRESH_TYPE_HANDLES;
      if (table->IsColumnVisible(IDS_TASK_MANAGER_GDI_HANDLES_COLUMN) ||
          table->IsColumnVisible(IDS_TASK_MANAGER_USER_HANDLES_COLUMN)) {
        new_visibility = true;
      }
      break;

    case IDS_TASK_MANAGER_IDLE_WAKEUPS_COLUMN:
      type = REFRESH_TYPE_IDLE_WAKEUPS;
      break;

    case IDS_TASK_MANAGER_WEBCORE_IMAGE_CACHE_COLUMN:
    case IDS_TASK_MANAGER_WEBCORE_SCRIPTS_CACHE_COLUMN:
    case IDS_TASK_MANAGER_WEBCORE_CSS_CACHE_COLUMN:
      type = REFRESH_TYPE_WEBCACHE_STATS;
      if (table->IsColumnVisible(IDS_TASK_MANAGER_WEBCORE_IMAGE_CACHE_COLUMN) ||
          table->IsColumnVisible(
              IDS_TASK_MANAGER_WEBCORE_SCRIPTS_CACHE_COLUMN) ||
          table->IsColumnVisible(IDS_TASK_MANAGER_WEBCORE_CSS_CACHE_COLUMN)) {
        new_visibility = true;
      }
      break;

    case IDS_TASK_MANAGER_VIDEO_MEMORY_COLUMN:
      type = REFRESH_TYPE_GPU_MEMORY;
      break;

    case IDS_TASK_MANAGER_SQLITE_MEMORY_USED_COLUMN:
      type = REFRESH_TYPE_SQLITE_MEMORY;
      break;

    case IDS_TASK_MANAGER_JAVASCRIPT_MEMORY_ALLOCATED_COLUMN:
      type = REFRESH_TYPE_V8_MEMORY;
      break;

    case IDS_TASK_MANAGER_NACL_DEBUG_STUB_PORT_COLUMN:
      type = REFRESH_TYPE_NACL;
      break;

    default:
      NOTREACHED();
      return;
  }

  if (new_visibility)
    AddRefreshType(type);
  else
    RemoveRefreshType(type);
}

bool NewTaskManagerView::TableModel::IsBrowserProcess(int row_index) const {
  return observed_task_manager()->GetProcessId(tasks_[row_index]) ==
      base::GetCurrentProcId();
}

void NewTaskManagerView::TableModel::OnRefresh() {
  if (table_model_observer_)
    table_model_observer_->OnItemsChanged(0, RowCount());
}

bool NewTaskManagerView::TableModel::IsTaskFirstInGroup(int row_index) const {
  if (row_index == 0)
    return true;

  return observed_task_manager()->GetProcessId(tasks_[row_index - 1]) !=
      observed_task_manager()->GetProcessId(tasks_[row_index]);
}

////////////////////////////////////////////////////////////////////////////////
// NewTaskManagerView Implementation:
////////////////////////////////////////////////////////////////////////////////

NewTaskManagerView::~NewTaskManagerView() {
  // Delete child views now, while our table model still exists.
  RemoveAllChildViews(true);
}

// static
void NewTaskManagerView::Show(Browser* browser) {
  if (g_task_manager_view) {
    // If there's a Task manager window open already, just activate it.
    g_task_manager_view->GetWidget()->Activate();
    return;
  }

  // In ash we can come here through the ChromeShellDelegate. If there is no
  // browser window at that time of the call, browser could be passed as NULL.
  const chrome::HostDesktopType desktop_type =
      browser ? browser->host_desktop_type() : chrome::HOST_DESKTOP_TYPE_ASH;

  g_task_manager_view = new NewTaskManagerView(desktop_type);

  gfx::NativeWindow window = browser ? browser->window()->GetNativeWindow()
                                     : nullptr;
#if defined(USE_ASH)
  if (!window)
    window = ash::wm::GetActiveWindow();
#endif

  DialogDelegate::CreateDialogWidget(g_task_manager_view,
                                     window,
                                     nullptr);
  g_task_manager_view->InitAlwaysOnTopState();
  g_task_manager_view->table_model_->StartUpdating();

#if defined(OS_WIN)
  // Set the app id for the task manager to the app id of its parent browser. If
  // no parent is specified, the app id will default to that of the initial
  // process.
  if (browser) {
    ui::win::SetAppIdForWindow(
        ShellIntegration::GetChromiumModelIdForProfile(
            browser->profile()->GetPath()),
        views::HWNDForWidget(g_task_manager_view->GetWidget()));
  }
#endif

  g_task_manager_view->GetWidget()->Show();

  // Set the initial focus to the list of tasks.
  views::FocusManager* focus_manager =
      g_task_manager_view->GetFocusManager();
  if (focus_manager)
    focus_manager->SetFocusedView(g_task_manager_view->tab_table_);

#if defined(USE_ASH)
  gfx::NativeWindow native_window =
      g_task_manager_view->GetWidget()->GetNativeWindow();
  ash::SetShelfItemDetailsForDialogWindow(native_window,
                                          IDR_ASH_SHELF_ICON_TASK_MANAGER,
                                          native_window->title());
#endif
}

// static
void NewTaskManagerView::Hide() {
  if (g_task_manager_view)
    g_task_manager_view->GetWidget()->Close();
}

void NewTaskManagerView::Layout() {
  gfx::Size size = kill_button_->GetPreferredSize();
  gfx::Rect parent_bounds = parent()->GetContentsBounds();
  const int horizontal_margin = views::kPanelHorizMargin;
  const int vertical_margin = views::kButtonVEdgeMargin;
  int x = width() - size.width() - horizontal_margin;
  int y_buttons = parent_bounds.bottom() - size.height() - vertical_margin;
  kill_button_->SetBounds(x, y_buttons, size.width(), size.height());

  size = about_memory_link_->GetPreferredSize();
  about_memory_link_->SetBounds(
      horizontal_margin,
      y_buttons + (kill_button_->height() - size.height()) / 2,
      size.width(), size.height());

  gfx::Rect rect = GetLocalBounds();
  rect.Inset(horizontal_margin, views::kPanelVertMargin);
  rect.Inset(0, 0, 0,
             kill_button_->height() + views::kUnrelatedControlVerticalSpacing);
  tab_table_parent_->SetBoundsRect(rect);
}

gfx::Size NewTaskManagerView::GetPreferredSize() const {
  return gfx::Size(460, 270);
}

bool NewTaskManagerView::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  DCHECK_EQ(ui::VKEY_W, accelerator.key_code());
  DCHECK_EQ(ui::EF_CONTROL_DOWN, accelerator.modifiers());
  GetWidget()->Close();
  return true;
}

void NewTaskManagerView::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  views::DialogDelegateView::ViewHierarchyChanged(details);
  // Since we want the Kill button and the Memory Details link to show up in
  // the same visual row as the close button, which is provided by the
  // framework, we must add the buttons to the non-client view, which is the
  // parent of this view. Similarly, when we're removed from the view
  // hierarchy, we must take care to clean up those items as well.
  if (details.child == this) {
    if (details.is_add) {
      details.parent->AddChildView(about_memory_link_);
      details.parent->AddChildView(kill_button_);
      tab_table_parent_ = tab_table_->CreateParentIfNecessary();
      AddChildView(tab_table_parent_);
    } else {
      details.parent->RemoveChildView(kill_button_);
      details.parent->RemoveChildView(about_memory_link_);
    }
  }
}

void NewTaskManagerView::ButtonPressed(views::Button* sender,
                                       const ui::Event& event) {
  DCHECK_EQ(kill_button_, sender);

  using SelectedIndices =  ui::ListSelectionModel::SelectedIndices;
  SelectedIndices selection(tab_table_->selection_model().selected_indices());
  for (SelectedIndices::const_reverse_iterator i = selection.rbegin();
       i != selection.rend();
       ++i) {
    table_model_->KillTask(*i);
  }
}

bool NewTaskManagerView::CanResize() const {
  return true;
}

bool NewTaskManagerView::CanMaximize() const {
  return true;
}

bool NewTaskManagerView::CanMinimize() const {
  return true;
}

bool NewTaskManagerView::ExecuteWindowsCommand(int command_id) {
  return false;
}

base::string16 NewTaskManagerView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_TASK_MANAGER_TITLE);
}

std::string NewTaskManagerView::GetWindowName() const {
  return prefs::kTaskManagerWindowPlacement;
}

int NewTaskManagerView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

void NewTaskManagerView::WindowClosing() {
  // Now that the window is closed, we can allow a new one to be opened.
  // (WindowClosing comes in asynchronously from the call to Close() and we
  // may have already opened a new instance).
  if (g_task_manager_view == this) {
    // We don't have to delete |g_task_manager_view| as we don't own it. It's
    // owned by the Views hierarchy.
    g_task_manager_view = nullptr;
  }
  table_model_->StopUpdating();
}

bool NewTaskManagerView::UseNewStyleForThisDialog() const {
  return false;
}

void NewTaskManagerView::OnSelectionChanged() {
  const ui::ListSelectionModel::SelectedIndices& selections(
      tab_table_->selection_model().selected_indices());
  bool selection_contains_browser_process = false;
  for (const auto& selection : selections) {
    if (table_model_->IsBrowserProcess(selection)) {
      selection_contains_browser_process = true;
      break;
    }
  }

  kill_button_->SetEnabled(!selection_contains_browser_process &&
                           !selections.empty());
}

void NewTaskManagerView::OnDoubleClick() {
  ActivateFocusedTab();
}

void NewTaskManagerView::OnKeyDown(ui::KeyboardCode keycode) {
  if (keycode == ui::VKEY_RETURN)
    ActivateFocusedTab();
}

void NewTaskManagerView::LinkClicked(views::Link* source, int event_flags) {
  DCHECK_EQ(about_memory_link_, source);
  OpenAboutMemory(desktop_type_);
}

void NewTaskManagerView::ShowContextMenuForView(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  ui::SimpleMenuModel menu_model(this);

  for (const auto& table_column : columns_) {
    menu_model.AddCheckItem(table_column.id,
                            l10n_util::GetStringUTF16(table_column.id));
  }

  menu_runner_.reset(
      new views::MenuRunner(&menu_model, views::MenuRunner::CONTEXT_MENU));

  if (menu_runner_->RunMenuAt(GetWidget(),
                              nullptr,
                              gfx::Rect(point, gfx::Size()),
                              views::MENU_ANCHOR_TOPLEFT,
                              source_type) == views::MenuRunner::MENU_DELETED) {
    return;
  }
}

bool NewTaskManagerView::IsCommandIdChecked(int id) const {
  return tab_table_->IsColumnVisible(id);
}

bool NewTaskManagerView::IsCommandIdEnabled(int id) const {
  return true;
}

bool NewTaskManagerView::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) {
  return false;
}

void NewTaskManagerView::ExecuteCommand(int id, int event_flags) {
  table_model_->ToggleColumnVisibility(tab_table_, id);
}

NewTaskManagerView::NewTaskManagerView(chrome::HostDesktopType desktop_type)
    : table_model_(
        new NewTaskManagerView::TableModel(REFRESH_TYPE_CPU |
                                           REFRESH_TYPE_MEMORY |
                                           REFRESH_TYPE_NETWORK_USAGE)),
      menu_runner_(nullptr),
      always_on_top_menu_text_(),
      kill_button_(nullptr),
      about_memory_link_(nullptr),
      tab_table_(nullptr),
      tab_table_parent_(nullptr),
      columns_(),
      desktop_type_(desktop_type),
      is_always_on_top_(false) {
  Init();
}

void NewTaskManagerView::Init() {
  columns_.push_back(ui::TableColumn(IDS_TASK_MANAGER_TASK_COLUMN,
                                     ui::TableColumn::LEFT, -1, 1));
  columns_.back().sortable = true;

  columns_.push_back(ui::TableColumn(IDS_TASK_MANAGER_PROFILE_NAME_COLUMN,
                                     ui::TableColumn::LEFT, -1, 0));
  columns_.back().sortable = true;

  columns_.push_back(ui::TableColumn(IDS_TASK_MANAGER_PHYSICAL_MEM_COLUMN,
                                     ui::TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;
  columns_.back().initial_sort_is_ascending = false;

  columns_.push_back(ui::TableColumn(IDS_TASK_MANAGER_SHARED_MEM_COLUMN,
                                     ui::TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;
  columns_.back().initial_sort_is_ascending = false;

  columns_.push_back(ui::TableColumn(IDS_TASK_MANAGER_PRIVATE_MEM_COLUMN,
                                     ui::TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;
  columns_.back().initial_sort_is_ascending = false;

  columns_.push_back(ui::TableColumn(IDS_TASK_MANAGER_CPU_COLUMN,
                                     ui::TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;
  columns_.back().initial_sort_is_ascending = false;

  columns_.push_back(ui::TableColumn(IDS_TASK_MANAGER_NET_COLUMN,
                                     ui::TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;
  columns_.back().initial_sort_is_ascending = false;

  columns_.push_back(ui::TableColumn(IDS_TASK_MANAGER_PROCESS_ID_COLUMN,
                                     ui::TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;

#if defined(OS_WIN)
  columns_.push_back(ui::TableColumn(IDS_TASK_MANAGER_GDI_HANDLES_COLUMN,
                                     ui::TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;
  columns_.back().initial_sort_is_ascending = false;
  columns_.push_back(ui::TableColumn(IDS_TASK_MANAGER_USER_HANDLES_COLUMN,
                                     ui::TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;
  columns_.back().initial_sort_is_ascending = false;
#endif

  columns_.push_back(ui::TableColumn(
      IDS_TASK_MANAGER_WEBCORE_IMAGE_CACHE_COLUMN,
      ui::TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;
  columns_.back().initial_sort_is_ascending = false;

  columns_.push_back(ui::TableColumn(
      IDS_TASK_MANAGER_WEBCORE_SCRIPTS_CACHE_COLUMN,
      ui::TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;
  columns_.back().initial_sort_is_ascending = false;

  columns_.push_back(ui::TableColumn(IDS_TASK_MANAGER_WEBCORE_CSS_CACHE_COLUMN,
                                     ui::TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;
  columns_.back().initial_sort_is_ascending = false;

  columns_.push_back(ui::TableColumn(IDS_TASK_MANAGER_VIDEO_MEMORY_COLUMN,
                                     ui::TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;
  columns_.back().initial_sort_is_ascending = false;

  columns_.push_back(ui::TableColumn(IDS_TASK_MANAGER_SQLITE_MEMORY_USED_COLUMN,
                                     ui::TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;
  columns_.back().initial_sort_is_ascending = false;

#if !defined(DISABLE_NACL)
  columns_.push_back(ui::TableColumn(
      IDS_TASK_MANAGER_NACL_DEBUG_STUB_PORT_COLUMN,
      ui::TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;
#endif  // !defined(DISABLE_NACL)

  columns_.push_back(
      ui::TableColumn(IDS_TASK_MANAGER_JAVASCRIPT_MEMORY_ALLOCATED_COLUMN,
                      ui::TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;
  columns_.back().initial_sort_is_ascending = false;

#if defined(OS_MACOSX) || defined(OS_LINUX)
  // TODO(port): Port the idle wakeups per second to platforms other than Linux
  // and MacOS (http://crbug.com/120488).
  columns_.push_back(ui::TableColumn(IDS_TASK_MANAGER_IDLE_WAKEUPS_COLUMN,
                                     ui::TableColumn::RIGHT, -1, 0));
  columns_.back().sortable = true;
  columns_.back().initial_sort_is_ascending = false;
#endif  // defined(OS_MACOSX) || defined(OS_LINUX)

  tab_table_ = new views::TableView(
      table_model_.get(), columns_, views::ICON_AND_TEXT, false);
  tab_table_->SetGrouper(table_model_.get());

  // Hide some columns by default
  tab_table_->SetColumnVisibility(IDS_TASK_MANAGER_PROFILE_NAME_COLUMN, false);
  tab_table_->SetColumnVisibility(IDS_TASK_MANAGER_SHARED_MEM_COLUMN, false);
  tab_table_->SetColumnVisibility(IDS_TASK_MANAGER_PRIVATE_MEM_COLUMN, false);
  tab_table_->SetColumnVisibility(IDS_TASK_MANAGER_WEBCORE_IMAGE_CACHE_COLUMN,
                                  false);
  tab_table_->SetColumnVisibility(IDS_TASK_MANAGER_WEBCORE_SCRIPTS_CACHE_COLUMN,
                                  false);
  tab_table_->SetColumnVisibility(IDS_TASK_MANAGER_WEBCORE_CSS_CACHE_COLUMN,
                                  false);
  tab_table_->SetColumnVisibility(IDS_TASK_MANAGER_VIDEO_MEMORY_COLUMN,
                                  false);
  tab_table_->SetColumnVisibility(IDS_TASK_MANAGER_SQLITE_MEMORY_USED_COLUMN,
                                  false);
  tab_table_->SetColumnVisibility(IDS_TASK_MANAGER_NACL_DEBUG_STUB_PORT_COLUMN,
                                  false);
  tab_table_->SetColumnVisibility(
      IDS_TASK_MANAGER_JAVASCRIPT_MEMORY_ALLOCATED_COLUMN,
      false);
  tab_table_->SetColumnVisibility(IDS_TASK_MANAGER_GDI_HANDLES_COLUMN, false);
  tab_table_->SetColumnVisibility(IDS_TASK_MANAGER_USER_HANDLES_COLUMN, false);
  tab_table_->SetColumnVisibility(IDS_TASK_MANAGER_IDLE_WAKEUPS_COLUMN, false);

  tab_table_->SetObserver(this);
  tab_table_->set_context_menu_controller(this);
  set_context_menu_controller(this);

  kill_button_ = new views::LabelButton(this,
      l10n_util::GetStringUTF16(IDS_TASK_MANAGER_KILL));
  kill_button_->SetStyle(views::Button::STYLE_BUTTON);

  about_memory_link_ = new views::Link(
      l10n_util::GetStringUTF16(IDS_TASK_MANAGER_ABOUT_MEMORY_LINK));
  about_memory_link_->set_listener(this);

  // Makes sure our state is consistent.
  OnSelectionChanged();

  AddAccelerator(ui::Accelerator(ui::VKEY_W, ui::EF_CONTROL_DOWN));
}

void NewTaskManagerView::InitAlwaysOnTopState() {
  RetriveSavedAlwaysOnTopState();
  GetWidget()->SetAlwaysOnTop(is_always_on_top_);
}

void NewTaskManagerView::ActivateFocusedTab() {
  const int active_row = tab_table_->selection_model().active();
  if (active_row != ui::ListSelectionModel::kUnselectedIndex)
    table_model_->ActivateTask(active_row);
}

void NewTaskManagerView::RetriveSavedAlwaysOnTopState() {
  is_always_on_top_ = false;

  if (!g_browser_process->local_state())
    return;

  const base::DictionaryValue* dictionary =
    g_browser_process->local_state()->GetDictionary(GetWindowName());
  if (dictionary)
    dictionary->GetBoolean("always_on_top", &is_always_on_top_);
}

}  // namespace task_management

