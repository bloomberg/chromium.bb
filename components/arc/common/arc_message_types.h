// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_COMMON_MESSAGE_TYPES
#define COMPONENTS_ARC_COMMON_MESSAGE_TYPES

#include <string>

namespace arc {

// Describing the boot phase of the ARC instance, as defined by AOSP in
// com.android.server.SystemService
enum class InstanceBootPhase {
  // Boot phase indicating that the instance is not running
  NOT_RUNNING = 0,

  // After receiving this boot phase the ARC bridge is ready to receive
  // IPC messages. This phase is ARC-specific.
  BRIDGE_READY,

  // After receiving this boot phase, services can safely call into core
  // system services such as the PowerManager or PackageManager.
  SYSTEM_SERVICES_READY,

  // After receiving this boot phase, services can broadcast Intents.
  ACTIVITY_MANAGER_READY,

  // After receiving this boot phase, services can start/bind to third party
  // apps. Apps will be able to make Binder calls into services at this point.
  THIRD_PARTY_APPS_CAN_START,

  // After receiving this boot phase, services can allow user interaction
  // with the device. This phase occurs when boot has completed and the home
  // application has started.
  BOOT_COMPLETED,

  // Last enum entry for IPC_ENUM_TRAITS
  LAST = BOOT_COMPLETED
};

// Enumerates the types of wake lock the ARC instance can request from the
// host.
enum class DisplayWakeLockType {
  // Does not allow the screen to dim, turn off, or lock; prevents
  // idle suspend.
  BRIGHT = 0,

  // Allows dimming the screen, but prevents it from turning off or locking.
  // Also prevents idle suspend.
  DIM = 1,

  // Last enum entry for IPC_ENUM_TRAITS
  LAST = DIM
};

// Duplicates ui::ScaleFactor enum in order to be accessible on Android side.
enum ScaleFactor : int {
  SCALE_FACTOR_NONE = 0,
  SCALE_FACTOR_100P,
  SCALE_FACTOR_125P,
  SCALE_FACTOR_133P,
  SCALE_FACTOR_140P,
  SCALE_FACTOR_150P,
  SCALE_FACTOR_180P,
  SCALE_FACTOR_200P,
  SCALE_FACTOR_250P,
  SCALE_FACTOR_300P,

  NUM_SCALE_FACTORS
};

// Describes ARC app.
struct AppInfo {
  std::string name;
  std::string package;
  std::string activity;
};

}  // namespace arc

#endif  // COMPONENTS_ARC_COMMON_MESSAGE_TYPES
