// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/spellcheck/spellcheck_api.h"

#include "base/lazy_instance.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "chrome/common/extensions/api/spellcheck/spellcheck_handler.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
namespace extensions {

namespace keys = extension_manifest_keys;
namespace errors = extension_manifest_errors;

namespace {

SpellcheckDictionaryInfo* GetSpellcheckDictionaryInfo(
    const Extension* extension) {
  SpellcheckDictionaryInfo *spellcheck_info =
      static_cast<SpellcheckDictionaryInfo*>(
          extension->GetManifestData(keys::kSpellcheck));
  return spellcheck_info;
}

SpellcheckService::DictionaryFormat GetDictionaryFormat(std::string format) {
  if (format == "hunspell") {
    return SpellcheckService::DICT_HUNSPELL;
  } else if (format == "text") {
    return SpellcheckService::DICT_TEXT;
  } else {
    return SpellcheckService::DICT_UNKNOWN;
  }
}

}  // namespace


SpellcheckAPI::SpellcheckAPI(Profile* profile) {
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                 content::Source<Profile>(profile));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile));
}

SpellcheckAPI::~SpellcheckAPI() {
}

static base::LazyInstance<ProfileKeyedAPIFactory<SpellcheckAPI> >
g_factory = LAZY_INSTANCE_INITIALIZER;

// static
ProfileKeyedAPIFactory<SpellcheckAPI>* SpellcheckAPI::GetFactoryInstance() {
  return &g_factory.Get();
}

void SpellcheckAPI::Observe(int type,
                            const content::NotificationSource& source,
                            const content::NotificationDetails& details) {
  Profile* profile = content::Source<Profile>(source).ptr();
  SpellcheckService* spellcheck = NULL;
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_LOADED: {
      const Extension* extension = content::Details<Extension>(details).ptr();
      SpellcheckDictionaryInfo* spellcheck_info =
          GetSpellcheckDictionaryInfo(extension);
      if (spellcheck_info) {
        // TODO(rlp): Handle load failure. =
        spellcheck = SpellcheckServiceFactory::GetForProfile(profile);
        spellcheck->LoadExternalDictionary(
            spellcheck_info->language,
            spellcheck_info->locale,
            spellcheck_info->path,
            GetDictionaryFormat(spellcheck_info->format));
      }
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_UNLOADED: {
      const Extension* extension =
          content::Details<UnloadedExtensionInfo>(details)->extension;
      SpellcheckDictionaryInfo* spellcheck_info =
          GetSpellcheckDictionaryInfo(extension);
      if (spellcheck_info) {
        // TODO(rlp): Handle unload failure.
        spellcheck = SpellcheckServiceFactory::GetForProfile(profile);
        spellcheck->UnloadExternalDictionary(spellcheck_info->path);
      }
      break;
    }
    default:
      NOTREACHED();
  }
}

template <>
void ProfileKeyedAPIFactory<SpellcheckAPI>::DeclareFactoryDependencies() {
  DependsOn(SpellcheckServiceFactory::GetInstance());
}

}  // namespace extensions
