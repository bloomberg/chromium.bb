// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ntp_snippets/content_suggestions_service_factory.h"

#include "base/feature_list.h"
#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/ntp_snippets/content_suggestions_service.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/chrome_feature_list.h"
#endif  // OS_ANDROID

// static
ContentSuggestionsServiceFactory*
ContentSuggestionsServiceFactory::GetInstance() {
  return base::Singleton<ContentSuggestionsServiceFactory>::get();
}

// static
ntp_snippets::ContentSuggestionsService*
ContentSuggestionsServiceFactory::GetForProfile(Profile* profile) {
  DCHECK(!profile->IsOffTheRecord());
  return static_cast<ntp_snippets::ContentSuggestionsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

ContentSuggestionsServiceFactory::ContentSuggestionsServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "ContentSuggestionsService",
          BrowserContextDependencyManager::GetInstance()) {}

ContentSuggestionsServiceFactory::~ContentSuggestionsServiceFactory() {}

KeyedService* ContentSuggestionsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  using State = ntp_snippets::ContentSuggestionsService::State;

  // TODO(mvanouwerkerk): Move the enable logic into the service once we start
  // observing pref changes.
  State enabled = State::DISABLED;
#if defined(OS_ANDROID)
  // TODO(pke): Split that feature into suggestions overall and article
  // suggestions in particular.
  if (base::FeatureList::IsEnabled(chrome::android::kNTPSnippetsFeature))
    enabled = State::ENABLED;
#endif  // OS_ANDROID

  return new ntp_snippets::ContentSuggestionsService(enabled);
}
