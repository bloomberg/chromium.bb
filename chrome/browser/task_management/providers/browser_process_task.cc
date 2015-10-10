// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_management/providers/browser_process_task.h"

#include "base/command_line.h"
#include "chrome/browser/task_management/task_manager_observer.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/common/content_switches.h"
#include "grit/theme_resources.h"
#include "net/proxy/proxy_resolver_v8.h"
#include "third_party/sqlite/sqlite3.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace task_management {

namespace {

gfx::ImageSkia* g_default_icon = nullptr;

gfx::ImageSkia* GetDefaultIcon() {
  if (!g_default_icon && ResourceBundle::HasSharedInstance()) {
      g_default_icon = ResourceBundle::GetSharedInstance().
          GetImageSkiaNamed(IDR_PRODUCT_LOGO_16);
    if (g_default_icon)
      g_default_icon->MakeThreadSafe();
  }

  return g_default_icon;
}

bool ReportsV8Stats() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  return !command_line->HasSwitch(switches::kWinHttpProxyResolver) &&
      !command_line->HasSwitch(switches::kSingleProcess);
}

}  // namespace

BrowserProcessTask::BrowserProcessTask()
    : Task(l10n_util::GetStringUTF16(IDS_TASK_MANAGER_WEB_BROWSER_CELL_TEXT),
           GetDefaultIcon(),
           base::GetCurrentProcessHandle()),
       allocated_v8_memory_(-1),
       used_v8_memory_(-1),
       used_sqlite_memory_(-1),
       reports_v8_stats_(ReportsV8Stats()){
}

BrowserProcessTask::~BrowserProcessTask() {
}

void BrowserProcessTask::Refresh(const base::TimeDelta& update_interval,
                                 int64 refresh_flags) {
  Task::Refresh(update_interval, refresh_flags);

  if (reports_v8_stats_ && (refresh_flags & REFRESH_TYPE_V8_MEMORY) != 0) {
    allocated_v8_memory_ =
        static_cast<int64>(net::ProxyResolverV8::GetTotalHeapSize());
    used_v8_memory_ =
        static_cast<int64>(net::ProxyResolverV8::GetUsedHeapSize());
  }

  if ((refresh_flags & REFRESH_TYPE_SQLITE_MEMORY) != 0)
    used_sqlite_memory_ = static_cast<int64>(sqlite3_memory_used());
}

Task::Type BrowserProcessTask::GetType() const {
  return Task::BROWSER;
}

int BrowserProcessTask::GetChildProcessUniqueID() const {
  return 0;
}

int64 BrowserProcessTask::GetSqliteMemoryUsed() const {
  return used_sqlite_memory_;
}

int64 BrowserProcessTask::GetV8MemoryAllocated() const {
  return allocated_v8_memory_;
}

int64 BrowserProcessTask::GetV8MemoryUsed() const {
  return used_v8_memory_;
}

}  // namespace task_management
