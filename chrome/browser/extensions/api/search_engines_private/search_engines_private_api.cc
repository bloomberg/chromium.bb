// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/search_engines_private/search_engines_private_api.h"

#include <stddef.h>

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/hotword_audio_history_handler.h"
#include "chrome/browser/search/hotword_service.h"
#include "chrome/browser/search/hotword_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "extensions/browser/extension_function_registry.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace search_engines_private = api::search_engines_private;

const char kHotwordServiceMissing[] = "Cannot retrieve hotword service";
const char kRetrainWithoutAlwaysOnEnabledError[] =
    "Cannot retrain without always search and audio logging enabled";
const char kOptInWithAudioLoggingError[] =
    "Cannot opt in with audio logging but also with always on enabled";
const char kAlreadyOptedInError[] = "Cannot opt in if already opted in";

////////////////////////////////////////////////////////////////////////////////
// SearchEnginesPrivateGetSearchEnginesFunction

SearchEnginesPrivateGetSearchEnginesFunction::
    SearchEnginesPrivateGetSearchEnginesFunction()
    : chrome_details_(this) {
}

SearchEnginesPrivateGetSearchEnginesFunction::
    ~SearchEnginesPrivateGetSearchEnginesFunction() {
}

ExtensionFunction::ResponseAction
SearchEnginesPrivateGetSearchEnginesFunction::Run() {
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(chrome_details_.GetProfile());
  base::ListValue* engines = new base::ListValue();

  const TemplateURL* default_url =
      template_url_service->GetDefaultSearchProvider();
  std::vector<TemplateURL*> urls = template_url_service->GetTemplateURLs();
  for (size_t i = 0; i < urls.size(); i++) {
    search_engines_private::SearchEngine engine;
    engine.guid = urls[i]->sync_guid();
    engine.name = base::UTF16ToUTF8(urls[i]->short_name());
    engine.keyword = base::UTF16ToUTF8(urls[i]->keyword());
    engine.url = urls[i]->url();
    engine.type = urls[i]->show_in_default_list()
        ? search_engines_private::SearchEngineType::SEARCH_ENGINE_TYPE_DEFAULT
        : search_engines_private::SearchEngineType::SEARCH_ENGINE_TYPE_OTHER;

    if (urls[i] == default_url)
      engine.is_selected = scoped_ptr<bool>(new bool(true));

    engines->Append(engine.ToValue().release());
  }

  return RespondNow(OneArgument(engines));
}

////////////////////////////////////////////////////////////////////////////////
// SearchEnginesPrivateSetSelectedSearchEngineFunction

SearchEnginesPrivateSetSelectedSearchEngineFunction::
    SearchEnginesPrivateSetSelectedSearchEngineFunction()
    : chrome_details_(this) {
}

SearchEnginesPrivateSetSelectedSearchEngineFunction::
    ~SearchEnginesPrivateSetSelectedSearchEngineFunction() {
}

ExtensionFunction::ResponseAction
SearchEnginesPrivateSetSelectedSearchEngineFunction::Run() {
  scoped_ptr<search_engines_private::SetSelectedSearchEngine::Params>
      parameters =
          search_engines_private::SetSelectedSearchEngine::Params::Create(
              *args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(chrome_details_.GetProfile());
  template_url_service->SetUserSelectedDefaultSearchProvider(
      template_url_service->GetTemplateURLForGUID(parameters->guid));
  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// SearchEnginesPrivateAddOtherSearchEngineFunction

SearchEnginesPrivateAddOtherSearchEngineFunction::
    SearchEnginesPrivateAddOtherSearchEngineFunction()
    : chrome_details_(this) {
}

SearchEnginesPrivateAddOtherSearchEngineFunction::
    ~SearchEnginesPrivateAddOtherSearchEngineFunction() {
}

ExtensionFunction::ResponseAction
SearchEnginesPrivateAddOtherSearchEngineFunction::Run() {
  scoped_ptr<search_engines_private::AddOtherSearchEngine::Params> parameters =
      search_engines_private::AddOtherSearchEngine::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  TemplateURLData data;
  data.SetShortName(base::UTF8ToUTF16(parameters->name));
  data.SetKeyword(base::UTF8ToUTF16(parameters->keyword));
  data.SetURL(parameters->url);
  TemplateURL* turl = new TemplateURL(data);

  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(chrome_details_.GetProfile());
  template_url_service->Add(turl);
  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// SearchEnginesPrivateUpdateSearchEngineFunction

SearchEnginesPrivateUpdateSearchEngineFunction::
    SearchEnginesPrivateUpdateSearchEngineFunction()
    : chrome_details_(this) {
}

SearchEnginesPrivateUpdateSearchEngineFunction::
    ~SearchEnginesPrivateUpdateSearchEngineFunction() {
}

ExtensionFunction::ResponseAction
SearchEnginesPrivateUpdateSearchEngineFunction::Run() {
  scoped_ptr<search_engines_private::UpdateSearchEngine::Params> parameters =
      search_engines_private::UpdateSearchEngine::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(chrome_details_.GetProfile());
  TemplateURL* turl =
      template_url_service->GetTemplateURLForGUID(parameters->guid);

  template_url_service->ResetTemplateURL(
      turl, base::UTF8ToUTF16(parameters->name),
      base::UTF8ToUTF16(parameters->keyword), parameters->url);

  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// SearchEnginesPrivateRemoveSearchEngineFunction

SearchEnginesPrivateRemoveSearchEngineFunction::
    SearchEnginesPrivateRemoveSearchEngineFunction()
    : chrome_details_(this) {
}

SearchEnginesPrivateRemoveSearchEngineFunction::
    ~SearchEnginesPrivateRemoveSearchEngineFunction() {
}

ExtensionFunction::ResponseAction
SearchEnginesPrivateRemoveSearchEngineFunction::Run() {
  scoped_ptr<search_engines_private::RemoveSearchEngine::Params> parameters =
      search_engines_private::RemoveSearchEngine::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(chrome_details_.GetProfile());
  TemplateURL* turl =
      template_url_service->GetTemplateURLForGUID(parameters->guid);
  template_url_service->Remove(turl);
  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// SearchEnginesPrivateGetHotwordStateFunction

SearchEnginesPrivateGetHotwordStateFunction::
    SearchEnginesPrivateGetHotwordStateFunction()
    : chrome_details_(this), weak_ptr_factory_(this) {
}

SearchEnginesPrivateGetHotwordStateFunction::
    ~SearchEnginesPrivateGetHotwordStateFunction() {
}

ExtensionFunction::ResponseAction
SearchEnginesPrivateGetHotwordStateFunction::Run() {
  Profile* profile = chrome_details_.GetProfile();
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);

  scoped_ptr<search_engines_private::HotwordState> state(
      new search_engines_private::HotwordState());

  if (template_url_service && template_url_service->loaded()) {
    const TemplateURL* default_url =
        template_url_service->GetDefaultSearchProvider();
    if (!default_url ||
        !default_url->HasGoogleBaseURLs(
              template_url_service->search_terms_data()) ||
        !HotwordServiceFactory::IsHotwordAllowed(profile)) {
      return RespondNow(OneArgument(state->ToValue().release()));
    }
  }

  state->availability.push_back(
      search_engines_private::HotwordFeature::HOTWORD_FEATURE_SEARCH);

  SigninManagerBase* signin = SigninManagerFactory::GetForProfile(profile);
  bool authenticated = signin && signin->IsAuthenticated();
  bool always_on =
      HotwordServiceFactory::IsAlwaysOnAvailable() && authenticated;

  if (always_on) {
    state->availability.push_back(
        search_engines_private::HotwordFeature::HOTWORD_FEATURE_ALWAYS_ON);
    if (profile->GetPrefs()->GetBoolean(prefs::kHotwordAlwaysOnSearchEnabled)) {
      state->availability.push_back(
          search_engines_private::HotwordFeature::HOTWORD_FEATURE_RETRAIN_LINK);
    }
  }

  int error = HotwordServiceFactory::GetCurrentError(profile);
  if (error) {
    std::string error_message(l10n_util::GetStringUTF8(error));
    if (error == IDS_HOTWORD_GENERIC_ERROR_MESSAGE) {
      error_message = l10n_util::GetStringFUTF8(
          error, base::UTF8ToUTF16(chrome::kHotwordLearnMoreURL));
    }
    state->error_msg.reset(new std::string(error_message));
  }

  // Audio history should be displayed if it's enabled regardless of the
  // hotword error state if the user is signed in. If the user is not signed
  // in, audio history is meaningless. This is only displayed if always-on
  // hotwording is available.
  if (authenticated && always_on) {
    std::string user_display_name = signin->GetAuthenticatedAccountInfo().email;
    HotwordService* hotword_service =
        HotwordServiceFactory::GetForProfile(profile);
    if (hotword_service) {
      hotword_service->GetAudioHistoryHandler()->GetAudioHistoryEnabled(
          base::Bind(&SearchEnginesPrivateGetHotwordStateFunction::
                         OnAudioHistoryChecked,
                     weak_ptr_factory_.GetWeakPtr(), base::Passed(&state),
                     l10n_util::GetStringFUTF16(
                         IDS_HOTWORD_AUDIO_HISTORY_ENABLED,
                         base::UTF8ToUTF16(user_display_name))));
      return RespondLater();
    }
  }

  return RespondNow(OneArgument(state->ToValue().release()));
}

void SearchEnginesPrivateGetHotwordStateFunction::OnAudioHistoryChecked(
    scoped_ptr<search_engines_private::HotwordState> state,
    const base::string16& audio_history_state,
    bool success,
    bool logging_enabled) {
  if (success && logging_enabled) {
    state->availability.push_back(
        search_engines_private::HotwordFeature::HOTWORD_FEATURE_AUDIO_HISTORY);
    state->audio_history_state.reset(new std::string(
        base::UTF16ToUTF8(audio_history_state)));
  }
  Respond(OneArgument(state->ToValue().release()));
}

////////////////////////////////////////////////////////////////////////////////
// SearchEnginesPrivateOptIntoHotwordingFunction

SearchEnginesPrivateOptIntoHotwordingFunction::
    SearchEnginesPrivateOptIntoHotwordingFunction()
    : chrome_details_(this) {
}

SearchEnginesPrivateOptIntoHotwordingFunction::
    ~SearchEnginesPrivateOptIntoHotwordingFunction() {
}

ExtensionFunction::ResponseAction
SearchEnginesPrivateOptIntoHotwordingFunction::Run() {
  scoped_ptr<search_engines_private::OptIntoHotwording::Params> parameters =
      search_engines_private::OptIntoHotwording::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  Profile* profile = chrome_details_.GetProfile();

  HotwordService::LaunchMode launch_mode =
      HotwordService::HOTWORD_AND_AUDIO_HISTORY;

  PrefService* prefs = profile->GetPrefs();

  HotwordService* hotword_service =
      HotwordServiceFactory::GetForProfile(profile);
  if (!hotword_service)
    return RespondNow(Error(kHotwordServiceMissing));

  if (parameters->retrain) {
    if (!prefs->GetBoolean(prefs::kHotwordAlwaysOnSearchEnabled) ||
        !prefs->GetBoolean(prefs::kHotwordAudioLoggingEnabled)) {
      return RespondNow(Error(kRetrainWithoutAlwaysOnEnabledError));
    }

    launch_mode = HotwordService::RETRAIN;
  } else if (prefs->GetBoolean(prefs::kHotwordAudioLoggingEnabled)) {
    if (prefs->GetBoolean(prefs::kHotwordAlwaysOnSearchEnabled)) {
      return RespondNow(Error(kOptInWithAudioLoggingError));

    }
    launch_mode = HotwordService::HOTWORD_ONLY;
  } else {
    if (prefs->GetBoolean(prefs::kHotwordAlwaysOnSearchEnabled))
      return RespondNow(Error(kAlreadyOptedInError));
  }

  hotword_service->OptIntoHotwording(launch_mode);
  return RespondNow(NoArguments());
}

}  // namespace extensions
