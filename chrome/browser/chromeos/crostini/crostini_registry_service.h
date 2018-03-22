// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_REGISTRY_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_REGISTRY_SERVICE_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;
class PrefRegistrySimple;
class PrefService;

namespace chromeos {

// The CrostiniRegistryService stores information about Desktop Entries (apps)
// in Crostini. We store this in prefs so that it is readily available even when
// the VM isn't running. The registrations here correspond to .desktop files,
// which are detailed in the spec:
// https://www.freedesktop.org/wiki/Specifications/desktop-entry-spec/
class CrostiniRegistryService : public KeyedService {
 public:
  struct Registration {
    Registration(const std::string& desktop_file_id, const std::string& name);
    ~Registration() = default;

    std::string desktop_file_id;

    // TODO(timloh): Add other relevant fields from the Desktop Entry Spec, in
    // particular: Icon, Comment, MimeType, NoDisplay
    // TODO(timloh): .desktop files allow localization of this string. We need
    // to expand this to support those too.
    std::string name;

    DISALLOW_COPY_AND_ASSIGN(Registration);
  };

  explicit CrostiniRegistryService(Profile* profile);
  ~CrostiniRegistryService() override;

  // Return null if |app_id| is not found in the registry.
  std::unique_ptr<CrostiniRegistryService::Registration> GetRegistration(
      const std::string& app_id) const;

  // If an app has already been registered for the given desktop file id, it
  // will be overriden.
  void SetRegistration(const Registration& registration);

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

 private:
  // Owned by the BrowserContext.
  PrefService* const prefs_;

  DISALLOW_COPY_AND_ASSIGN(CrostiniRegistryService);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_REGISTRY_SERVICE_H_
