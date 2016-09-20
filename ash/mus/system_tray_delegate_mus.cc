// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/system_tray_delegate_mus.h"

#include "ash/common/system/tray/system_tray_notifier.h"
#include "ash/common/wm_shell.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"

namespace ash {
namespace {

SystemTrayDelegateMus* g_instance = nullptr;

}  // namespace

SystemTrayDelegateMus::SystemTrayDelegateMus()
    : hour_clock_type_(base::GetHourClockType()) {
  DCHECK(!g_instance);
  g_instance = this;
}

SystemTrayDelegateMus::~SystemTrayDelegateMus() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
SystemTrayDelegateMus* SystemTrayDelegateMus::Get() {
  return g_instance;
}

base::HourClockType SystemTrayDelegateMus::GetHourClockType() const {
  return hour_clock_type_;
}

void SystemTrayDelegateMus::SetUse24HourClock(bool use_24_hour) {
  hour_clock_type_ = use_24_hour ? base::k24HourClock : base::k12HourClock;
  WmShell::Get()->system_tray_notifier()->NotifyDateFormatChanged();
}

}  // namespace ash
