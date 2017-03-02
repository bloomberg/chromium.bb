// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/system_display/system_display_api.h"

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/api/system_display/display_info_provider.h"
#include "extensions/common/api/system_display.h"

#if defined(OS_CHROMEOS)
#include "extensions/common/manifest_handlers/kiosk_mode_info.h"
#endif

namespace extensions {

namespace display = api::system_display;

const char SystemDisplayFunction::kCrosOnlyError[] =
    "Function available only on ChromeOS.";
const char SystemDisplayFunction::kKioskOnlyError[] =
    "Only kiosk enabled extensions are allowed to use this function.";

const char
    SystemDisplayShowNativeTouchCalibrationFunction::kTouchCalibrationError[] =
        "Touch calibration failed";

namespace {

class OverscanTracker;

// Singleton class to track overscan calibration overlays. An observer is
// created per WebContents which tracks any calbiration overlays by id.
// If the render frame is deleted (e.g. the tab is closed) before the overlay
// calibraiton is completed, the observer will call the overscan complete
// method to remove the overlay. When all observers are removed, the singleton
// tracker will delete itself.
class OverscanTracker {
 public:
  static void AddDisplay(content::WebContents* web_contents,
                         const std::string& id);
  static void RemoveDisplay(content::WebContents* web_contents,
                            const std::string& id);
  static void RemoveObserver(content::WebContents* web_contents);

  OverscanTracker() {}
  ~OverscanTracker() {}

 private:
  class OverscanWebObserver;

  OverscanWebObserver* GetObserver(content::WebContents* web_contents,
                                   bool create);
  bool RemoveObserverImpl(content::WebContents* web_contents);

  using ObserverMap =
      std::map<content::WebContents*, std::unique_ptr<OverscanWebObserver>>;
  ObserverMap observers_;

  DISALLOW_COPY_AND_ASSIGN(OverscanTracker);
};

class OverscanTracker::OverscanWebObserver
    : public content::WebContentsObserver {
 public:
  explicit OverscanWebObserver(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}
  ~OverscanWebObserver() override {}

  // WebContentsObserver
  void RenderFrameDeleted(
      content::RenderFrameHost* render_frame_host) override {
    for (const std::string& id : display_ids_) {
      // Reset any uncomitted calibraiton changes and complete calibration to
      // hide the overlay.
      DisplayInfoProvider::Get()->OverscanCalibrationReset(id);
      DisplayInfoProvider::Get()->OverscanCalibrationComplete(id);
    }
    OverscanTracker::RemoveObserver(web_contents());  // Deletes this.
  }

  void AddDisplay(const std::string& id) { display_ids_.insert(id); }

  void RemoveDisplay(const std::string& id) {
    display_ids_.erase(id);
    if (display_ids_.empty())
      OverscanTracker::RemoveObserver(web_contents());  // Deletes this.
  }

 private:
  std::set<std::string> display_ids_;

  DISALLOW_COPY_AND_ASSIGN(OverscanWebObserver);
};

static OverscanTracker* g_overscan_tracker = nullptr;

// static
void OverscanTracker::AddDisplay(content::WebContents* web_contents,
                                 const std::string& id) {
  if (!g_overscan_tracker)
    g_overscan_tracker = new OverscanTracker;
  g_overscan_tracker->GetObserver(web_contents, true)->AddDisplay(id);
}

// static
void OverscanTracker::RemoveDisplay(content::WebContents* web_contents,
                                    const std::string& id) {
  if (!g_overscan_tracker)
    return;
  OverscanWebObserver* observer =
      g_overscan_tracker->GetObserver(web_contents, false);
  if (observer)
    observer->RemoveDisplay(id);
}

// static
void OverscanTracker::RemoveObserver(content::WebContents* web_contents) {
  if (!g_overscan_tracker)
    return;
  if (g_overscan_tracker->RemoveObserverImpl(web_contents)) {
    delete g_overscan_tracker;
    g_overscan_tracker = nullptr;
  }
}

OverscanTracker::OverscanWebObserver* OverscanTracker::GetObserver(
    content::WebContents* web_contents,
    bool create) {
  ObserverMap::iterator iter = observers_.find(web_contents);
  if (iter != observers_.end())
    return iter->second.get();
  if (!create)
    return nullptr;
  auto owned_observer = base::MakeUnique<OverscanWebObserver>(web_contents);
  auto* observer_ptr = owned_observer.get();
  observers_[web_contents] = std::move(owned_observer);
  return observer_ptr;
}

bool OverscanTracker::RemoveObserverImpl(content::WebContents* web_contents) {
  observers_.erase(web_contents);
  return observers_.empty();
}

}  // namespace

bool SystemDisplayFunction::PreRunValidation(std::string* error) {
  if (!UIThreadExtensionFunction::PreRunValidation(error))
    return false;

#if !defined(OS_CHROMEOS)
  *error = kCrosOnlyError;
  return false;
#else
  if (!ShouldRestrictToKioskAndWebUI())
    return true;

  if (source_context_type() == Feature::WEBUI_CONTEXT)
    return true;
  if (KioskModeInfo::IsKioskEnabled(extension()))
    return true;
  *error = kKioskOnlyError;
  return false;
#endif
}

bool SystemDisplayFunction::ShouldRestrictToKioskAndWebUI() {
  return true;
}

ExtensionFunction::ResponseAction SystemDisplayGetInfoFunction::Run() {
  DisplayInfoProvider::DisplayUnitInfoList all_displays_info =
      DisplayInfoProvider::Get()->GetAllDisplaysInfo();
  return RespondNow(
      ArgumentList(display::GetInfo::Results::Create(all_displays_info)));
}

ExtensionFunction::ResponseAction SystemDisplayGetDisplayLayoutFunction::Run() {
  DisplayInfoProvider::DisplayLayoutList display_layout =
      DisplayInfoProvider::Get()->GetDisplayLayout();
  return RespondNow(
      ArgumentList(display::GetDisplayLayout::Results::Create(display_layout)));
}

bool SystemDisplayGetDisplayLayoutFunction::ShouldRestrictToKioskAndWebUI() {
  return false;
}

ExtensionFunction::ResponseAction
SystemDisplaySetDisplayPropertiesFunction::Run() {
  std::string error;
  std::unique_ptr<display::SetDisplayProperties::Params> params(
      display::SetDisplayProperties::Params::Create(*args_));
  bool result =
      DisplayInfoProvider::Get()->SetInfo(params->id, params->info, &error);
  if (!result)
    return RespondNow(Error(error));
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction SystemDisplaySetDisplayLayoutFunction::Run() {
  std::unique_ptr<display::SetDisplayLayout::Params> params(
      display::SetDisplayLayout::Params::Create(*args_));
  if (!DisplayInfoProvider::Get()->SetDisplayLayout(params->layouts))
    return RespondNow(Error("Unable to set display layout"));
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
SystemDisplayEnableUnifiedDesktopFunction::Run() {
  std::unique_ptr<display::EnableUnifiedDesktop::Params> params(
      display::EnableUnifiedDesktop::Params::Create(*args_));
  DisplayInfoProvider::Get()->EnableUnifiedDesktop(params->enabled);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
SystemDisplayOverscanCalibrationStartFunction::Run() {
  std::unique_ptr<display::OverscanCalibrationStart::Params> params(
      display::OverscanCalibrationStart::Params::Create(*args_));
  if (!DisplayInfoProvider::Get()->OverscanCalibrationStart(params->id))
    return RespondNow(Error("Invalid display ID: " + params->id));
  OverscanTracker::AddDisplay(GetSenderWebContents(), params->id);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
SystemDisplayOverscanCalibrationAdjustFunction::Run() {
  std::unique_ptr<display::OverscanCalibrationAdjust::Params> params(
      display::OverscanCalibrationAdjust::Params::Create(*args_));
  if (!params)
    return RespondNow(Error("Invalid parameters"));
  if (!DisplayInfoProvider::Get()->OverscanCalibrationAdjust(params->id,
                                                             params->delta)) {
    return RespondNow(
        Error("Calibration not started for display ID: " + params->id));
  }
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
SystemDisplayOverscanCalibrationResetFunction::Run() {
  std::unique_ptr<display::OverscanCalibrationReset::Params> params(
      display::OverscanCalibrationReset::Params::Create(*args_));
  if (!DisplayInfoProvider::Get()->OverscanCalibrationReset(params->id))
    return RespondNow(
        Error("Calibration not started for display ID: " + params->id));
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
SystemDisplayOverscanCalibrationCompleteFunction::Run() {
  std::unique_ptr<display::OverscanCalibrationComplete::Params> params(
      display::OverscanCalibrationComplete::Params::Create(*args_));
  if (!DisplayInfoProvider::Get()->OverscanCalibrationComplete(params->id)) {
    return RespondNow(
        Error("Calibration not started for display ID: " + params->id));
  }
  OverscanTracker::RemoveDisplay(GetSenderWebContents(), params->id);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
SystemDisplayShowNativeTouchCalibrationFunction::Run() {
  std::unique_ptr<display::ShowNativeTouchCalibration::Params> params(
      display::ShowNativeTouchCalibration::Params::Create(*args_));

  std::string error;
  if (DisplayInfoProvider::Get()->IsNativeTouchCalibrationActive(&error))
    return RespondNow(Error(error));

  if (!DisplayInfoProvider::Get()->ShowNativeTouchCalibration(
          params->id, &error,
          base::Bind(&SystemDisplayShowNativeTouchCalibrationFunction::
                         OnCalibrationComplete,
                     this))) {
    return RespondNow(Error(error));
  }
  return RespondLater();
}

void SystemDisplayShowNativeTouchCalibrationFunction::OnCalibrationComplete(
    bool success) {
  if (success)
    Respond(OneArgument(base::MakeUnique<base::Value>(true)));
  else
    Respond(Error(kTouchCalibrationError));
}

ExtensionFunction::ResponseAction
SystemDisplayStartCustomTouchCalibrationFunction::Run() {
  std::unique_ptr<display::StartCustomTouchCalibration::Params> params(
      display::StartCustomTouchCalibration::Params::Create(*args_));

  std::string error;
  if (DisplayInfoProvider::Get()->IsNativeTouchCalibrationActive(&error))
    return RespondNow(Error(error));

  if (!DisplayInfoProvider::Get()->StartCustomTouchCalibration(params->id,
                                                               &error))
    return RespondNow(Error(error));
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
SystemDisplayCompleteCustomTouchCalibrationFunction::Run() {
  std::unique_ptr<display::CompleteCustomTouchCalibration::Params> params(
      display::CompleteCustomTouchCalibration::Params::Create(*args_));

  std::string error;
  if (DisplayInfoProvider::Get()->IsNativeTouchCalibrationActive(&error))
    return RespondNow(Error(error));

  if (!DisplayInfoProvider::Get()->CompleteCustomTouchCalibration(
          params->pairs, params->bounds, &error)) {
    return RespondNow(Error(error));
  }
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
SystemDisplayClearTouchCalibrationFunction::Run() {
  std::unique_ptr<display::ClearTouchCalibration::Params> params(
      display::ClearTouchCalibration::Params::Create(*args_));

  std::string error;
  if (DisplayInfoProvider::Get()->IsNativeTouchCalibrationActive(&error))
    return RespondNow(Error(error));

  if (!DisplayInfoProvider::Get()->ClearTouchCalibration(params->id, &error))
    return RespondNow(Error(error));
  return RespondNow(NoArguments());
}

}  // namespace extensions
