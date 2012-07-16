// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_DEFAULT_APPS_H_
#define CHROME_BROWSER_EXTENSIONS_DEFAULT_APPS_H_

#include "base/basictypes.h"
#include "chrome/browser/extensions/external_provider_impl.h"

class PrefService;
class Profile;

namespace extensions {
class Extension;
}

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


// A specialization of the ExternalProviderImpl that conditionally installs apps
// from the chrome::DIR_DEFAULT_APPS location based on a preference in the
// profile.
class Provider : public extensions::ExternalProviderImpl {
 public:
  Provider(Profile* profile,
           VisitorInterface* service,
           extensions::ExternalLoader* loader,
           extensions::Extension::Location crx_location,
           extensions::Extension::Location download_location,
           int creation_flags);

  // ExternalProviderImpl overrides:
  virtual void VisitRegisteredExtension() OVERRIDE;

 private:
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(Provider);
};

}  // namespace default_apps

#endif  // CHROME_BROWSER_EXTENSIONS_DEFAULT_APPS_H_
