// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"

#include "chrome/browser/autocomplete/autocomplete_classifier.h"
#include "chrome/browser/autocomplete/autocomplete_controller.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/autocomplete/shortcuts_backend_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

#if defined(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"
#endif

// static
AutocompleteClassifier* AutocompleteClassifierFactory::GetForProfile(
    Profile* profile) {
  return static_cast<AutocompleteClassifier*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
AutocompleteClassifierFactory* AutocompleteClassifierFactory::GetInstance() {
  return Singleton<AutocompleteClassifierFactory>::get();
}

// static
KeyedService* AutocompleteClassifierFactory::BuildInstanceFor(
    content::BrowserContext* context) {
  Profile* profile = static_cast<Profile*>(context);
  return new AutocompleteClassifier(
      make_scoped_ptr(new AutocompleteController(
          profile, TemplateURLServiceFactory::GetForProfile(profile), NULL,
          AutocompleteClassifier::kDefaultOmniboxProviders)),
      scoped_ptr<AutocompleteSchemeClassifier>(
          new ChromeAutocompleteSchemeClassifier(profile)));
}

AutocompleteClassifierFactory::AutocompleteClassifierFactory()
    : BrowserContextKeyedServiceFactory(
        "AutocompleteClassifier",
        BrowserContextDependencyManager::GetInstance()) {
#if defined(ENABLE_EXTENSIONS)
  DependsOn(
      extensions::ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
#endif
  DependsOn(TemplateURLServiceFactory::GetInstance());
  // TODO(pkasting): Uncomment these once they exist.
  //   DependsOn(PrefServiceFactory::GetInstance());
  DependsOn(ShortcutsBackendFactory::GetInstance());
}

AutocompleteClassifierFactory::~AutocompleteClassifierFactory() {
}

content::BrowserContext* AutocompleteClassifierFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

bool AutocompleteClassifierFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

KeyedService* AutocompleteClassifierFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return BuildInstanceFor(static_cast<Profile*>(profile));
}
