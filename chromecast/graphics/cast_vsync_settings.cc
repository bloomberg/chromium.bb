// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/graphics/cast_vsync_settings.h"

namespace chromecast {
namespace {
base::LazyInstance<CastVSyncSettings> g_instance = LAZY_INSTANCE_INITIALIZER;
}  // namespace

// static
CastVSyncSettings* CastVSyncSettings::GetInstance() {
  return g_instance.Pointer();
}

base::TimeDelta CastVSyncSettings::GetVSyncInterval() const {
  return interval_;
}

void CastVSyncSettings::SetVSyncInterval(base::TimeDelta interval) {
  if (interval_ == interval)
    return;
  interval_ = interval;
  for (auto& observer : observers_)
    observer.OnVSyncIntervalChanged(interval);
}

void CastVSyncSettings::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void CastVSyncSettings::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

// Default to 60fps until set otherwise
CastVSyncSettings::CastVSyncSettings()
    : interval_(base::TimeDelta::FromMicroseconds(16666)) {}

CastVSyncSettings::~CastVSyncSettings() = default;

}  // namespace chromecast
