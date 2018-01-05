// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/media_perception_private/media_perception_api_delegate_chromeos.h"

#include "chrome/browser/component_updater/cros_component_installer.h"

namespace media_perception = extensions::api::media_perception_private;

namespace extensions {

namespace {

constexpr char kLightComponentName[] = "rtanalytics-light";
constexpr char kFullComponentName[] = "rtanalytics-full";

std::string GetComponentNameForComponentType(
    const media_perception::ComponentType& type) {
  switch (type) {
    case media_perception::COMPONENT_TYPE_LIGHT:
      return kLightComponentName;
    case media_perception::COMPONENT_TYPE_FULL:
      return kFullComponentName;
    case media_perception::COMPONENT_TYPE_NONE:
      LOG(ERROR) << "No component type requested.";
      return "";
  }
  NOTREACHED() << "Reached component type not in switch.";
  return "";
}

}  // namespace

MediaPerceptionAPIDelegateChromeOS::MediaPerceptionAPIDelegateChromeOS() =
    default;

MediaPerceptionAPIDelegateChromeOS::~MediaPerceptionAPIDelegateChromeOS() {}

void MediaPerceptionAPIDelegateChromeOS::LoadCrOSComponent(
    const media_perception::ComponentType& type,
    LoadCrOSComponentCallback load_callback) {
  g_browser_process->platform_part()->cros_component_manager()->Load(
      GetComponentNameForComponentType(type), std::move(load_callback));
}

}  // namespace extensions
