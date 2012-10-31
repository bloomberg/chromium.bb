// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/monitor/tray_monitor.h"

#include "ash/system/tray/tray_item_view.h"
#include "ash/system/tray/tray_views.h"
#include "base/process_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/views/controls/label.h"
#include "ui/views/border.h"

namespace {
const int kRefreshTimeoutMs = 1000;
}

namespace ash {
namespace internal {

TrayMonitor::TrayMonitor() : label_(NULL) {
  refresh_timer_.Start(FROM_HERE,
      base::TimeDelta::FromMilliseconds(kRefreshTimeoutMs),
      this, &TrayMonitor::RefreshStats);
}

TrayMonitor::~TrayMonitor() {
  label_ = NULL;
}

views::View* TrayMonitor::CreateTrayView(user::LoginStatus status) {
  TrayItemView* view = new TrayItemView;
  view->CreateLabel();
  label_ = view->label();
  SetupLabelForTray(label_);
  return view;
}

void TrayMonitor::DestroyTrayView() {
  label_ = NULL;
}

void TrayMonitor::RefreshStats() {
  base::SystemMemoryInfoKB mem_info;
  base::GetSystemMemoryInfo(&mem_info);
  std::string output;
  string16 free_bytes =
      ui::FormatBytes(static_cast<int64>(mem_info.free) * 1024);
  output = StringPrintf("%s free", UTF16ToUTF8(free_bytes).c_str());
  if (mem_info.gem_objects != -1 && mem_info.gem_size != -1) {
    string16 gem_size = ui::FormatBytes(mem_info.gem_size);
    output += StringPrintf(", %d gobjects alloced (%s)",
                           mem_info.gem_objects,
                           UTF16ToUTF8(gem_size).c_str());
  }
  label_->SetText(UTF8ToUTF16(output));
}

}  // namespace internal
}  // namespace ash
