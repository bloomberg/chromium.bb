// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/monitor/tray_monitor.h"

#include "ash/gpu_support.h"
#include "ash/shell.h"
#include "ash/system/tray/tray_item_view.h"
#include "base/process/memory.h"
#include "base/process/process_metrics.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"

namespace {
const int kRefreshTimeoutMs = 1000;
}

namespace ash {
namespace internal {

TrayMonitor::TrayMonitor(SystemTray* system_tray)
    : SystemTrayItem(system_tray),
      label_(NULL) {
  refresh_timer_.Start(FROM_HERE,
      base::TimeDelta::FromMilliseconds(kRefreshTimeoutMs),
      this, &TrayMonitor::OnTimer);
}

TrayMonitor::~TrayMonitor() {
  label_ = NULL;
}

views::View* TrayMonitor::CreateTrayView(user::LoginStatus status) {
  TrayItemView* view = new TrayItemView(this);
  view->CreateLabel();
  label_ = view->label();
  label_->SetAutoColorReadabilityEnabled(false);
  label_->SetEnabledColor(SK_ColorWHITE);
  label_->SetBackgroundColor(SkColorSetARGB(0, 255, 255, 255));
  label_->SetShadowColors(SkColorSetARGB(64, 0, 0, 0),
                          SkColorSetARGB(64, 0, 0, 0));
  label_->SetShadowOffset(0, 1);
  label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label_->SetFontList(label_->font_list().DeriveWithSizeDelta(-2));
  return view;
}

void TrayMonitor::DestroyTrayView() {
  label_ = NULL;
}

void TrayMonitor::OnTimer() {
  GPUSupport::GetGpuProcessHandlesCallback callback =
      base::Bind(&TrayMonitor::OnGotHandles, base::Unretained(this));
  refresh_timer_.Stop();
  Shell::GetInstance()->gpu_support()->GetGpuProcessHandles(callback);
}

void TrayMonitor::OnGotHandles(const std::list<base::ProcessHandle>& handles) {
  base::SystemMemoryInfoKB mem_info;
  base::GetSystemMemoryInfo(&mem_info);
  std::string output;
  base::string16 free_bytes =
      ui::FormatBytes(static_cast<int64>(mem_info.free) * 1024);
  output = base::StringPrintf("free: %s",
                              base::UTF16ToUTF8(free_bytes).c_str());
#if defined(OS_CHROMEOS)
  if (mem_info.gem_size != -1) {
    base::string16 gem_size = ui::FormatBytes(mem_info.gem_size);
    output += base::StringPrintf("  gmem: %s",
                                 base::UTF16ToUTF8(gem_size).c_str());
    if (mem_info.gem_objects != -1)
      output += base::StringPrintf("  gobjects: %d", mem_info.gem_objects);
  }
#endif
  size_t total_private_bytes = 0, total_shared_bytes = 0;
  for (std::list<base::ProcessHandle>::const_iterator i = handles.begin();
       i != handles.end(); ++i) {
    base::ProcessMetrics* pm = base::ProcessMetrics::CreateProcessMetrics(*i);
    size_t private_bytes, shared_bytes;
    pm->GetMemoryBytes(&private_bytes, &shared_bytes);
    total_private_bytes += private_bytes;
    total_shared_bytes += shared_bytes;
    delete pm;
  }
  base::string16 private_size = ui::FormatBytes(total_private_bytes);
  base::string16 shared_size = ui::FormatBytes(total_shared_bytes);

  output += base::StringPrintf("\nGPU private: %s  shared: %s",
                               base::UTF16ToUTF8(private_size).c_str(),
                               base::UTF16ToUTF8(shared_size).c_str());
  label_->SetText(base::UTF8ToUTF16(output));
  refresh_timer_.Start(FROM_HERE,
      base::TimeDelta::FromMilliseconds(kRefreshTimeoutMs),
      this, &TrayMonitor::OnTimer);
}

}  // namespace internal
}  // namespace ash
