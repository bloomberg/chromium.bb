// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/task_manager_browser_process_resource_provider.h"

#include "base/command_line.h"
#include "base/string16.h"
#include "chrome/common/chrome_switches.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "net/proxy/proxy_resolver_v8.h"
#include "third_party/sqlite/sqlite3.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

#if defined(OS_MACOSX)
#include "ui/gfx/image/image_skia_util_mac.h"
#endif  // defined(OS_MACOSX)

#if defined(OS_WIN)
#include "chrome/browser/app_icon_win.h"
#include "ui/gfx/icon_util.h"
#endif  // defined(OS_WIN)

////////////////////////////////////////////////////////////////////////////////
// TaskManagerBrowserProcessResource class
////////////////////////////////////////////////////////////////////////////////

gfx::ImageSkia* TaskManagerBrowserProcessResource::default_icon_ = NULL;

TaskManagerBrowserProcessResource::TaskManagerBrowserProcessResource()
    : title_() {
  int pid = base::GetCurrentProcId();
  bool success = base::OpenPrivilegedProcessHandle(pid, &process_);
  DCHECK(success);
#if defined(OS_WIN)
  if (!default_icon_) {
    HICON icon = GetAppIcon();
    if (icon) {
      scoped_ptr<SkBitmap> bitmap(IconUtil::CreateSkBitmapFromHICON(icon));
      default_icon_ = new gfx::ImageSkia(
          gfx::ImageSkiaRep(*bitmap, ui::SCALE_FACTOR_100P));
    }
  }
#elif defined(OS_POSIX) && !defined(OS_MACOSX)
  if (!default_icon_) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    default_icon_ = rb.GetImageSkiaNamed(IDR_PRODUCT_LOGO_16);
  }
#elif defined(OS_MACOSX)
  if (!default_icon_) {
    // IDR_PRODUCT_LOGO_16 doesn't quite look like chrome/mac's icns icon. Load
    // the real app icon (requires a nsimage->image_skia->nsimage
    // conversion :-().
    default_icon_ = new gfx::ImageSkia(gfx::ApplicationIconAtSize(16));
  }
#else
  // TODO(port): Port icon code.
  NOTIMPLEMENTED();
#endif  // defined(OS_WIN)
  default_icon_->MakeThreadSafe();
}

TaskManagerBrowserProcessResource::~TaskManagerBrowserProcessResource() {
  base::CloseProcessHandle(process_);
}

// TaskManagerResource methods:
string16 TaskManagerBrowserProcessResource::GetTitle() const {
  if (title_.empty()) {
    title_ = l10n_util::GetStringUTF16(IDS_TASK_MANAGER_WEB_BROWSER_CELL_TEXT);
  }
  return title_;
}

string16 TaskManagerBrowserProcessResource::GetProfileName() const {
  return string16();
}

gfx::ImageSkia TaskManagerBrowserProcessResource::GetIcon() const {
  return *default_icon_;
}

size_t TaskManagerBrowserProcessResource::SqliteMemoryUsedBytes() const {
  return static_cast<size_t>(sqlite3_memory_used());
}

base::ProcessHandle TaskManagerBrowserProcessResource::GetProcess() const {
  return base::GetCurrentProcessHandle();  // process_;
}

int TaskManagerBrowserProcessResource::GetUniqueChildProcessId() const {
  return 0;
}

TaskManager::Resource::Type TaskManagerBrowserProcessResource::GetType() const {
  return BROWSER;
}

bool TaskManagerBrowserProcessResource::SupportNetworkUsage() const {
  return true;
}

void TaskManagerBrowserProcessResource::SetSupportNetworkUsage() {
  NOTREACHED();
}

bool TaskManagerBrowserProcessResource::ReportsSqliteMemoryUsed() const {
  return true;
}

// BrowserProcess uses v8 for proxy resolver in certain cases.
bool TaskManagerBrowserProcessResource::ReportsV8MemoryStats() const {
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  bool using_v8 = !command_line->HasSwitch(switches::kWinHttpProxyResolver);
  if (using_v8 && command_line->HasSwitch(switches::kSingleProcess)) {
    using_v8 = false;
  }
  return using_v8;
}

size_t TaskManagerBrowserProcessResource::GetV8MemoryAllocated() const {
  return net::ProxyResolverV8::GetTotalHeapSize();
}

size_t TaskManagerBrowserProcessResource::GetV8MemoryUsed() const {
  return net::ProxyResolverV8::GetUsedHeapSize();
}

////////////////////////////////////////////////////////////////////////////////
// TaskManagerBrowserProcessResourceProvider class
////////////////////////////////////////////////////////////////////////////////

TaskManagerBrowserProcessResourceProvider::
    TaskManagerBrowserProcessResourceProvider(TaskManager* task_manager)
    : updating_(false),
      task_manager_(task_manager) {
}

TaskManagerBrowserProcessResourceProvider::
    ~TaskManagerBrowserProcessResourceProvider() {
}

TaskManager::Resource* TaskManagerBrowserProcessResourceProvider::GetResource(
    int origin_pid,
    int render_process_host_id,
    int routing_id) {
  if (origin_pid || render_process_host_id != -1) {
    return NULL;
  }

  return &resource_;
}

void TaskManagerBrowserProcessResourceProvider::StartUpdating() {
  task_manager_->AddResource(&resource_);
}

void TaskManagerBrowserProcessResourceProvider::StopUpdating() {
}
