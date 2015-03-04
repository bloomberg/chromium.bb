// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/app_list/start_page_handler.h"

#include <string>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/hotword_service.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/browser/ui/app_list/start_page_service.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/common/pref_names.h"
#include "components/update_client/update_query_params.h"
#include "content/public/browser/web_ui.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_icon_set.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/speech_ui_model_observer.h"
#include "ui/events/event_constants.h"

namespace app_list {

namespace {

const char kAppListDoodleActionHistogram[] = "Apps.AppListDoodleAction";

// Interactions a user has with the app list doodle. This enum must not have its
// order altered as it is used in histograms.
enum DoodleAction {
  DOODLE_SHOWN = 0,
  DOODLE_CLICKED,
  // Add values here.

  DOODLE_ACTION_LAST,
};

#if defined(OS_CHROMEOS)
const char kOldHotwordExtensionVersionString[] = "0.1.1.5023";
#endif

}  // namespace

StartPageHandler::StartPageHandler() : extension_registry_observer_(this) {
}

StartPageHandler::~StartPageHandler() {
}

void StartPageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "appListShown", base::Bind(&StartPageHandler::HandleAppListShown,
                                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "doodleClicked", base::Bind(&StartPageHandler::HandleDoodleClicked,
                                  base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "initialize",
      base::Bind(&StartPageHandler::HandleInitialize, base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "launchApp",
      base::Bind(&StartPageHandler::HandleLaunchApp, base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "speechResult",
      base::Bind(&StartPageHandler::HandleSpeechResult,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "speechSoundLevel",
      base::Bind(&StartPageHandler::HandleSpeechSoundLevel,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setSpeechRecognitionState",
      base::Bind(&StartPageHandler::HandleSpeechRecognition,
                 base::Unretained(this)));
}

void StartPageHandler::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension) {
#if defined(OS_CHROMEOS)
  DCHECK_EQ(Profile::FromWebUI(web_ui()),
            Profile::FromBrowserContext(browser_context));
  if (extension->id() == extension_misc::kHotwordExtensionId)
    OnHotwordEnabledChanged();
#endif
}

void StartPageHandler::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionInfo::Reason reason) {
#if defined(OS_CHROMEOS)
  DCHECK_EQ(Profile::FromWebUI(web_ui()),
            Profile::FromBrowserContext(browser_context));
  if (extension->id() == extension_misc::kHotwordExtensionId)
    OnHotwordEnabledChanged();
#endif
}

#if defined(OS_CHROMEOS)
void StartPageHandler::OnHotwordEnabledChanged() {
  // If the hotword extension is new enough, we should use the new
  // hotwordPrivate API to provide the feature.
  // TODO(mukai): remove this after everything gets stable.
  Profile* profile = Profile::FromWebUI(web_ui());

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile);
  const extensions::Extension* hotword_extension =
      registry->GetExtensionById(extension_misc::kHotwordExtensionId,
                                 extensions::ExtensionRegistry::ENABLED);
  if (hotword_extension &&
      hotword_extension->version()->CompareTo(
          base::Version(kOldHotwordExtensionVersionString)) <= 0 &&
      !HotwordService::IsExperimentalHotwordingEnabled()) {
    StartPageService* service = StartPageService::Get(profile);
    web_ui()->CallJavascriptFunction(
        "appList.startPage.setHotwordEnabled",
        base::FundamentalValue(service && service->HotwordEnabled()));
  }
}
#endif

void StartPageHandler::HandleAppListShown(const base::ListValue* args) {
  bool doodle_shown = false;
  if (args->GetBoolean(0, &doodle_shown) && doodle_shown) {
    UMA_HISTOGRAM_ENUMERATION(kAppListDoodleActionHistogram, DOODLE_SHOWN,
                              DOODLE_ACTION_LAST);
  }
}

void StartPageHandler::HandleDoodleClicked(const base::ListValue* args) {
  UMA_HISTOGRAM_ENUMERATION(kAppListDoodleActionHistogram, DOODLE_CLICKED,
                            DOODLE_ACTION_LAST);
}

void StartPageHandler::HandleInitialize(const base::ListValue* args) {
  Profile* profile = Profile::FromWebUI(web_ui());
  StartPageService* service = StartPageService::Get(profile);
  if (!service)
    return;

  service->WebUILoaded();

#if defined(OS_CHROMEOS)
  if (app_list::switches::IsVoiceSearchEnabled() &&
      HotwordService::DoesHotwordSupportLanguage(profile)) {
    OnHotwordEnabledChanged();
    pref_change_registrar_.Init(profile->GetPrefs());
    pref_change_registrar_.RemoveAll();
    pref_change_registrar_.Add(
        prefs::kHotwordSearchEnabled,
        base::Bind(&StartPageHandler::OnHotwordEnabledChanged,
                   base::Unretained(this)));

    extension_registry_observer_.RemoveAll();
    extension_registry_observer_.Add(
        extensions::ExtensionRegistry::Get(profile));
  }

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile);
  const extensions::Extension* hotword_extension =
      registry->GetExtensionById(extension_misc::kHotwordExtensionId,
                                 extensions::ExtensionRegistry::ENABLED);
  if (hotword_extension &&
      hotword_extension->version()->CompareTo(
          base::Version(kOldHotwordExtensionVersionString)) <= 0) {
    web_ui()->CallJavascriptFunction(
        "appList.startPage.setNaclArch",
        base::StringValue(update_client::UpdateQueryParams::GetNaclArch()));
  }
#endif

  // If v2 hotwording is enabled, don't tell the start page that the app list is
  // being shown. V2 hotwording doesn't use the start page for anything.
  if (!app_list::switches::IsExperimentalAppListEnabled() &&
      !HotwordService::IsExperimentalHotwordingEnabled()) {
    web_ui()->CallJavascriptFunction(
        "appList.startPage.onAppListShown",
        base::FundamentalValue(service->HotwordEnabled()));
  }
}

void StartPageHandler::HandleLaunchApp(const base::ListValue* args) {
  std::string app_id;
  CHECK(args->GetString(0, &app_id));

  Profile* profile = Profile::FromWebUI(web_ui());
  const extensions::Extension* app =
      extensions::ExtensionRegistry::Get(profile)
          ->GetExtensionById(app_id, extensions::ExtensionRegistry::EVERYTHING);
  if (!app) {
    NOTREACHED();
    return;
  }

  AppListControllerDelegate* controller = AppListService::Get(
      chrome::GetHostDesktopTypeForNativeView(
          web_ui()->GetWebContents()->GetNativeView()))->
              GetControllerDelegate();
  controller->ActivateApp(profile,
                          app,
                          AppListControllerDelegate::LAUNCH_FROM_APP_LIST,
                          ui::EF_NONE);
}

void StartPageHandler::HandleSpeechResult(const base::ListValue* args) {
  base::string16 query;
  bool is_final = false;
  CHECK(args->GetString(0, &query));
  CHECK(args->GetBoolean(1, &is_final));

  StartPageService::Get(Profile::FromWebUI(web_ui()))->OnSpeechResult(
      query, is_final);
}

void StartPageHandler::HandleSpeechSoundLevel(const base::ListValue* args) {
  double level;
  CHECK(args->GetDouble(0, &level));

  StartPageService* service =
      StartPageService::Get(Profile::FromWebUI(web_ui()));
  if (service)
    service->OnSpeechSoundLevelChanged(static_cast<int16>(level));
}

void StartPageHandler::HandleSpeechRecognition(const base::ListValue* args) {
  std::string state_string;
  CHECK(args->GetString(0, &state_string));

  SpeechRecognitionState new_state = SPEECH_RECOGNITION_OFF;
  if (state_string == "READY")
    new_state = SPEECH_RECOGNITION_READY;
  else if (state_string == "HOTWORD_RECOGNIZING")
    new_state = SPEECH_RECOGNITION_HOTWORD_LISTENING;
  else if (state_string == "RECOGNIZING")
    new_state = SPEECH_RECOGNITION_RECOGNIZING;
  else if (state_string == "IN_SPEECH")
    new_state = SPEECH_RECOGNITION_IN_SPEECH;
  else if (state_string == "STOPPING")
    new_state = SPEECH_RECOGNITION_STOPPING;
  else if (state_string == "NETWORK_ERROR")
    new_state = SPEECH_RECOGNITION_NETWORK_ERROR;

  StartPageService* service =
      StartPageService::Get(Profile::FromWebUI(web_ui()));
  if (service)
    service->OnSpeechRecognitionStateChanged(new_state);
}

}  // namespace app_list
