// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_component_loader.h"

#include "base/command_line.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/prefs/pref_service.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/enhanced_bookmarks_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {

#if defined(OS_CHROMEOS)
namespace {
// Table mapping language codes to the extension ids of high-quality
// speech synthesis extensions. See the comment in StartLoading() for more.
struct LangToExtensionId {
  const char* lang;
  const char* extension_id;
};
LangToExtensionId kLangToExtensionIdTable[] = {
  { "en-US", extension_misc::kHighQuality_en_US_ExtensionId }
};
}  // anonymous namespace
#endif  // defined(OS_CHROMEOS)

ExternalComponentLoader::ExternalComponentLoader(Profile* profile)
    : profile_(profile) {
#if defined(OS_CHROMEOS)
  pref_change_registrar_.reset(new PrefChangeRegistrar());
  pref_change_registrar_->Init(profile->GetPrefs());
  pref_change_registrar_->Add(
      prefs::kHighQualitySpeechSynthesisLanguages,
      base::Bind(&ExternalComponentLoader::StartLoading, AsWeakPtr()));

  for (size_t i = 0; i < arraysize(kLangToExtensionIdTable); ++i) {
    lang_to_extension_id_map_[kLangToExtensionIdTable[i].lang] =
        kLangToExtensionIdTable[i].extension_id;
  }
#endif
}

ExternalComponentLoader::~ExternalComponentLoader() {}

void ExternalComponentLoader::StartLoading() {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  prefs_.reset(new base::DictionaryValue());
  std::string appId = extension_misc::kInAppPaymentsSupportAppId;
  prefs_->SetString(appId + ".external_update_url",
                    extension_urls::GetWebstoreUpdateUrl().spec());

  if (CommandLine::ForCurrentProcess()->
          GetSwitchValueASCII(switches::kEnableEnhancedBookmarks) != "0") {
    std::string ext_id = GetEnhancedBookmarksExtensionId();
    if (!ext_id.empty()) {
      prefs_->SetString(ext_id + ".external_update_url",
                        extension_urls::GetWebstoreUpdateUrl().spec());
    }
  }

#if defined(OS_CHROMEOS)
  // Chrome OS comes with medium-quality speech synthesis extensions built-in.
  // When the user speaks a certain threshold of utterances in the same
  // session, we set a preference indicating that high quality speech is
  // enabled for that language. Here, we check the preference and prepare
  // the list of external extensions to be installed based on that.
  PrefService* pref_service = profile_->GetPrefs();
  const base::ListValue* languages =
      pref_service->GetList(prefs::kHighQualitySpeechSynthesisLanguages);
  for (size_t i = 0; i < languages->GetSize(); ++i) {
    std::string lang;
    if (!languages->GetString(i, &lang))
      continue;

    base::hash_map<std::string, std::string>::iterator iter =
        lang_to_extension_id_map_.find(lang);
    if (iter == lang_to_extension_id_map_.end())
      continue;

    std::string extension_id = iter->second;
    base::DictionaryValue* extension = new base::DictionaryValue();
    prefs_->Set(extension_id, extension);
    extension->SetString(ExternalProviderImpl::kExternalUpdateUrl,
                         extension_urls::GetWebstoreUpdateUrl().spec());
    base::ListValue* supported_locales = new base::ListValue();
    supported_locales->AppendString(g_browser_process->GetApplicationLocale());
    extension->Set(ExternalProviderImpl::kSupportedLocales, supported_locales);
    extension->SetBoolean(ExternalProviderImpl::kIsFromWebstore, true);
  }
#endif  // defined(OS_CHROMEOS)

  LoadFinished();
}

// static
void ExternalComponentLoader::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
#if defined(OS_CHROMEOS)
  registry->RegisterListPref(prefs::kHighQualitySpeechSynthesisLanguages,
                             user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
#endif
}

}  // namespace extensions
