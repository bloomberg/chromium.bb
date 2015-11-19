// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_COMMON_MESSAGE_TYPES
#define COMPONENTS_ARC_COMMON_MESSAGE_TYPES

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
}

#endif  // COMPONENTS_ARC_COMMON_MESSAGE_TYPES
