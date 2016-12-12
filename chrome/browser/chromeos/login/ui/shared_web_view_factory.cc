// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/shared_web_view_factory.h"

#include "chrome/browser/chromeos/login/ui/shared_web_view.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace chromeos {

// static
SharedWebView* SharedWebViewFactory::GetForProfile(Profile* profile) {
  auto result = static_cast<SharedWebView*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
  return result;
}

// static
SharedWebViewFactory* SharedWebViewFactory::GetInstance() {
  return base::Singleton<SharedWebViewFactory>::get();
}

SharedWebViewFactory::SharedWebViewFactory()
    : BrowserContextKeyedServiceFactory(
          "SharedWebViewFactory",
          BrowserContextDependencyManager::GetInstance()) {}

SharedWebViewFactory::~SharedWebViewFactory() {}

content::BrowserContext* SharedWebViewFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Make sure that only the SigninProfile is using a shared webview.
  if (Profile::FromBrowserContext(context) != ProfileHelper::GetSigninProfile())
    return nullptr;

  return context;
}

KeyedService* SharedWebViewFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new SharedWebView(Profile::FromBrowserContext(context));
}

}  // namespace chromeos
