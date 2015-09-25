// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/task_manager/task_manager_columns.h"

#include "base/logging.h"
#include "chrome/grit/generated_resources.h"

namespace task_management {

namespace {

// On Mac: Width of "a" and most other letters/digits in "small" table views.
const int kCharWidth = 6;

}  // namespace

// IMPORTANT: Do NOT change the below list without changing the COLUMN_LIST
// macro below.
const TableColumnData kColumns[] = {
  { IDS_TASK_MANAGER_TASK_COLUMN, ui::TableColumn::LEFT, -1, 1, 120, 600, true,
    true, true },
  { IDS_TASK_MANAGER_PROFILE_NAME_COLUMN, ui::TableColumn::LEFT, -1, 0, 60, 200,
    true, true, false },
  { IDS_TASK_MANAGER_PHYSICAL_MEM_COLUMN, ui::TableColumn::RIGHT, -1, 0,
    arraysize("800 MiB") * kCharWidth, -1, true, false, true },
  { IDS_TASK_MANAGER_SHARED_MEM_COLUMN, ui::TableColumn::RIGHT, -1, 0,
    arraysize("800 MiB") * kCharWidth, -1, true, false, false },
  { IDS_TASK_MANAGER_PRIVATE_MEM_COLUMN, ui::TableColumn::RIGHT, -1, 0,
    arraysize("800 MiB") * kCharWidth, -1, true, false, false },
  { IDS_TASK_MANAGER_CPU_COLUMN, ui::TableColumn::RIGHT, -1, 0,
    arraysize("99.9") * kCharWidth, -1, true, false, true },
  { IDS_TASK_MANAGER_NET_COLUMN, ui::TableColumn::RIGHT, -1, 0,
    arraysize("150 kiB/s") * kCharWidth, -1, true, false, true },
  { IDS_TASK_MANAGER_PROCESS_ID_COLUMN, ui::TableColumn::RIGHT, -1, 0,
    arraysize("73099  ") * kCharWidth, -1, true, true, true },

#if defined(OS_WIN)
  { IDS_TASK_MANAGER_GDI_HANDLES_COLUMN, ui::TableColumn::RIGHT, -1, 0, 0, 0,
    true, false, false },
  { IDS_TASK_MANAGER_USER_HANDLES_COLUMN, ui::TableColumn::RIGHT, -1, 0, 0, 0,
    true, false, false },
#endif

  { IDS_TASK_MANAGER_WEBCORE_IMAGE_CACHE_COLUMN, ui::TableColumn::RIGHT, -1, 0,
    arraysize("2000.0K (2000.0 live)") * kCharWidth, -1, true, false, false },
  { IDS_TASK_MANAGER_WEBCORE_SCRIPTS_CACHE_COLUMN, ui::TableColumn::RIGHT, -1,
    0, arraysize("2000.0K (2000.0 live)") * kCharWidth, -1, true, false,
    false },
  { IDS_TASK_MANAGER_WEBCORE_CSS_CACHE_COLUMN, ui::TableColumn::RIGHT, -1, 0,
    arraysize("2000.0K (2000.0 live)") * kCharWidth, -1, true, false, false },
  { IDS_TASK_MANAGER_VIDEO_MEMORY_COLUMN, ui::TableColumn::RIGHT, -1, 0,
    arraysize("2000.0K") * kCharWidth, -1, true, false, false },
  { IDS_TASK_MANAGER_SQLITE_MEMORY_USED_COLUMN, ui::TableColumn::RIGHT, -1, 0,
    arraysize("800 kB") * kCharWidth, -1, true, false, false },

#if !defined(DISABLE_NACL)
  { IDS_TASK_MANAGER_NACL_DEBUG_STUB_PORT_COLUMN, ui::TableColumn::RIGHT, -1, 0,
    arraysize("32767") * kCharWidth, -1, true, true, false },
#endif  // !defined(DISABLE_NACL)

  { IDS_TASK_MANAGER_JAVASCRIPT_MEMORY_ALLOCATED_COLUMN, ui::TableColumn::RIGHT,
    -1, 0, arraysize("2000.0K (2000.0 live)") * kCharWidth, -1, true, false,
    false },

#if defined(OS_MACOSX) || defined(OS_LINUX)
  // TODO(port): Port the idle wakeups per second to platforms other than Linux
  // and MacOS (http://crbug.com/120488).
  { IDS_TASK_MANAGER_IDLE_WAKEUPS_COLUMN, ui::TableColumn::RIGHT, -1, 0,
    arraysize("idlewakeups") * kCharWidth, -1, true, false, false },
#endif  // defined(OS_MACOSX) || defined(OS_LINUX)
};

const size_t kColumnsSize = arraysize(kColumns);

const char kSortColumnIdKey[] = "sort_column_id";
const char kSortIsAscendingKey[] = "sort_is_ascending";

// We can't use the integer IDs of the columns converted to strings as session
// restore keys. These integer values can change from one build to another as
// they are generated. Instead we use the literal string value of the column
// ID symbol (i.e. for the ID IDS_TASK_MANAGER_TASK_COLUMN, we use the literal
// string "IDS_TASK_MANAGER_TASK_COLUMN". The following macros help us
// efficiently get the literal ID for the integer value.
#define COLUMNS_LITS(def) \
  def(IDS_TASK_MANAGER_TASK_COLUMN) \
  def(IDS_TASK_MANAGER_PROFILE_NAME_COLUMN) \
  def(IDS_TASK_MANAGER_PHYSICAL_MEM_COLUMN) \
  def(IDS_TASK_MANAGER_SHARED_MEM_COLUMN) \
  def(IDS_TASK_MANAGER_PRIVATE_MEM_COLUMN) \
  def(IDS_TASK_MANAGER_CPU_COLUMN) \
  def(IDS_TASK_MANAGER_NET_COLUMN) \
  def(IDS_TASK_MANAGER_PROCESS_ID_COLUMN) \
  def(IDS_TASK_MANAGER_GDI_HANDLES_COLUMN) \
  def(IDS_TASK_MANAGER_USER_HANDLES_COLUMN) \
  def(IDS_TASK_MANAGER_WEBCORE_IMAGE_CACHE_COLUMN) \
  def(IDS_TASK_MANAGER_WEBCORE_SCRIPTS_CACHE_COLUMN) \
  def(IDS_TASK_MANAGER_WEBCORE_CSS_CACHE_COLUMN) \
  def(IDS_TASK_MANAGER_VIDEO_MEMORY_COLUMN) \
  def(IDS_TASK_MANAGER_SQLITE_MEMORY_USED_COLUMN) \
  def(IDS_TASK_MANAGER_NACL_DEBUG_STUB_PORT_COLUMN) \
  def(IDS_TASK_MANAGER_JAVASCRIPT_MEMORY_ALLOCATED_COLUMN) \
  def(IDS_TASK_MANAGER_IDLE_WAKEUPS_COLUMN)
// Add to the above list in the macro any new IDs added in the future. Also
// remove the removed ones.

#define COLUMN_ID_AS_STRING(col_id) case col_id: return std::string(#col_id);

std::string GetColumnIdAsString(int column_id) {
  switch (column_id) {
    COLUMNS_LITS(COLUMN_ID_AS_STRING)
    default:
      NOTREACHED();
      return std::string();
  }
}


}  // namespace task_management
