// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/browser_process_resource_provider.h"

#include "base/command_line.h"
#include "base/strings/string16.h"
#include "chrome/browser/task_manager/resource_provider.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/common/content_switches.h"
#include "grit/theme_resources.h"
#include "net/proxy/proxy_resolver_v8.h"
#include "third_party/sqlite/sqlite3.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

namespace task_manager {

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

}  // namespace

BrowserProcessResource::BrowserProcessResource()
    : title_() {
}

BrowserProcessResource::~BrowserProcessResource() {
}

// Resource methods:
base::string16 BrowserProcessResource::GetTitle() const {
  if (title_.empty()) {
    title_ = l10n_util::GetStringUTF16(IDS_TASK_MANAGER_WEB_BROWSER_CELL_TEXT);
  }
  return title_;
}

base::string16 BrowserProcessResource::GetProfileName() const {
  return base::string16();
}

gfx::ImageSkia BrowserProcessResource::GetIcon() const {
  gfx::ImageSkia* image = GetDefaultIcon();
  return image? *image : gfx::ImageSkia();
}

size_t BrowserProcessResource::SqliteMemoryUsedBytes() const {
  return static_cast<size_t>(sqlite3_memory_used());
}

base::ProcessHandle BrowserProcessResource::GetProcess() const {
  return base::GetCurrentProcessHandle();
}

int BrowserProcessResource::GetUniqueChildProcessId() const {
  return 0;
}

Resource::Type BrowserProcessResource::GetType() const {
  return BROWSER;
}

bool BrowserProcessResource::SupportNetworkUsage() const {
  return true;
}

void BrowserProcessResource::SetSupportNetworkUsage() {
  NOTREACHED();
}

bool BrowserProcessResource::ReportsSqliteMemoryUsed() const {
  return true;
}

// BrowserProcess uses v8 for proxy resolver in certain cases.
bool BrowserProcessResource::ReportsV8MemoryStats() const {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  bool using_v8 = !command_line->HasSwitch(switches::kWinHttpProxyResolver);
  if (using_v8 && command_line->HasSwitch(switches::kSingleProcess)) {
    using_v8 = false;
  }
  return using_v8;
}

size_t BrowserProcessResource::GetV8MemoryAllocated() const {
  return net::ProxyResolverV8::GetTotalHeapSize();
}

size_t BrowserProcessResource::GetV8MemoryUsed() const {
  return net::ProxyResolverV8::GetUsedHeapSize();
}

////////////////////////////////////////////////////////////////////////////////
// BrowserProcessResourceProvider class
////////////////////////////////////////////////////////////////////////////////

BrowserProcessResourceProvider::
    BrowserProcessResourceProvider(TaskManager* task_manager)
    : updating_(false),
      task_manager_(task_manager) {
}

BrowserProcessResourceProvider::~BrowserProcessResourceProvider() {
}

Resource* BrowserProcessResourceProvider::GetResource(
    int origin_pid,
    int child_id,
    int route_id) {
  if (origin_pid || child_id != -1) {
    return NULL;
  }

  return &resource_;
}

void BrowserProcessResourceProvider::StartUpdating() {
  task_manager_->AddResource(&resource_);
}

void BrowserProcessResourceProvider::StopUpdating() {
}

}  // namespace task_manager
