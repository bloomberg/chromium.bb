// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_DEFAULT_APPS_H_
#define CHROME_BROWSER_EXTENSIONS_DEFAULT_APPS_H_
#pragma once

#include "base/basictypes.h"
#include "chrome/browser/extensions/external_extension_provider_impl.h"

class PrefService;
class Profile;

// Functions and types related to installing default apps.
namespace default_apps {

// These enum values are persisted in the user preferences, so they should not
// be changed.
enum InstallState {
  kUnknown,
  kAlwaysProvideDefaultApps,
  kNeverProvideDefaultApps
};

// Register preference properties used by default apps to maintain
// install state.
void RegisterUserPrefs(PrefService* prefs);


// A specialization of the ExternalExtensionProviderImpl that conditionally
// installs apps from the chrome::DIR_DEFAULT_APPS location based on a
// preference in the profile.
class Provider : public ExternalExtensionProviderImpl {
 public:
  Provider(Profile* profile,
           VisitorInterface* service,
           ExternalExtensionLoader* loader,
           Extension::Location crx_location,
           Extension::Location download_location,
           int creation_flags);

  // ExternalExtensionProviderImpl overrides:
  virtual void VisitRegisteredExtension() OVERRIDE;

 private:
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(Provider);
};


}  // namespace default_apps

#endif  // CHROME_BROWSER_EXTENSIONS_DEFAULT_APPS_H_
