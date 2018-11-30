// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/mash_service_factory.h"

#include <memory>

#include "ash/ash_service.h"
#include "ash/components/quick_launch/public/mojom/constants.mojom.h"
#include "ash/components/quick_launch/quick_launch_application.h"
#include "ash/components/shortcut_viewer/public/mojom/shortcut_viewer.mojom.h"
#include "ash/components/shortcut_viewer/shortcut_viewer_application.h"
#include "ash/components/tap_visualizer/public/mojom/constants.mojom.h"
#include "ash/components/tap_visualizer/tap_visualizer_app.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class MashService {
  kAsh = 0,
  kAutoclickDeprecated = 1,  // Deleted Aug 2018, https://crbug.com/876115
  kQuickLaunch = 2,
  kShortcutViewer = 3,
  kTapVisualizer = 4,
  kFontDeprecated = 5,  // Font Service is not in use for mash, but run
                        // in-process in the browser
                        // process. https://crbug.com/862553
  kMaxValue = kFontDeprecated,
};

// Wrapper function so we only have one copy of histogram macro generated code.
void RecordMashServiceLaunch(MashService service) {
  UMA_HISTOGRAM_ENUMERATION("Launch.MashService", service);
}

std::unique_ptr<service_manager::Service> CreateAshService(
    service_manager::mojom::ServiceRequest request) {
  RecordMashServiceLaunch(MashService::kAsh);
  logging::SetLogPrefix("ash");
  return std::make_unique<ash::AshService>(std::move(request));
}

std::unique_ptr<service_manager::Service> CreateQuickLaunchService(
    service_manager::mojom::ServiceRequest request) {
  RecordMashServiceLaunch(MashService::kQuickLaunch);
  logging::SetLogPrefix("quick");
  return std::make_unique<quick_launch::QuickLaunchApplication>(
      std::move(request));
}

std::unique_ptr<service_manager::Service> CreateShortcutViewerApp(
    service_manager::mojom::ServiceRequest request) {
  RecordMashServiceLaunch(MashService::kShortcutViewer);
  logging::SetLogPrefix("shortcut");
  return std::make_unique<keyboard_shortcut_viewer::ShortcutViewerApplication>(
      std::move(request));
}

std::unique_ptr<service_manager::Service> CreateTapVisualizerApp(
    service_manager::mojom::ServiceRequest request) {
  RecordMashServiceLaunch(MashService::kTapVisualizer);
  logging::SetLogPrefix("tap");
  return std::make_unique<tap_visualizer::TapVisualizerApp>(std::move(request));
}

}  // namespace

MashServiceFactory::MashServiceFactory() = default;

MashServiceFactory::~MashServiceFactory() = default;

std::unique_ptr<service_manager::Service>
MashServiceFactory::HandleServiceRequest(
    const std::string& service_name,
    service_manager::mojom::ServiceRequest request) {
  if (service_name == ash::mojom::kServiceName)
    return CreateAshService(std::move(request));
  if (service_name == quick_launch::mojom::kServiceName)
    return CreateQuickLaunchService(std::move(request));
  if (service_name == shortcut_viewer::mojom::kServiceName) {
    keyboard_shortcut_viewer::ShortcutViewerApplication ::
        RegisterForTraceEvents();
    return CreateShortcutViewerApp(std::move(request));
  }
  if (service_name == tap_visualizer::mojom::kServiceName)
    return CreateTapVisualizerApp(std::move(request));

  return nullptr;
}
