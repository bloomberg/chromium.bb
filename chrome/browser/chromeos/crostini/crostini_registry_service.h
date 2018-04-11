// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_REGISTRY_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_REGISTRY_SERVICE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;
class PrefRegistrySimple;
class PrefService;

namespace vm_tools {
namespace apps {
class ApplicationList;
}  // namespace apps
}  // namespace vm_tools

namespace chromeos {

// The CrostiniRegistryService stores information about Desktop Entries (apps)
// in Crostini. We store this in prefs so that it is readily available even when
// the VM isn't running. The registrations here correspond to .desktop files,
// which are detailed in the spec:
// https://www.freedesktop.org/wiki/Specifications/desktop-entry-spec/
class CrostiniRegistryService : public KeyedService {
 public:
  struct Registration {
    // Maps from locale to localized string, where the default string is always
    // present with an empty string key. Locales strings are formatted with
    // underscores and not hyphens (e.g. 'fr', 'en_US').
    using LocaleString = std::map<std::string, std::string>;

    Registration(const std::string& desktop_file_id,
                 const std::string& vm_name,
                 const std::string& container_name,
                 const LocaleString& name,
                 const LocaleString& comment,
                 const std::vector<std::string>& mime_types,
                 bool no_display);
    ~Registration();

    static const std::string& Localize(const LocaleString& locale_string);

    std::string desktop_file_id;
    std::string vm_name;
    std::string container_name;

    // TODO(timloh): Support icons.
    LocaleString name;
    LocaleString comment;
    std::vector<std::string> mime_types;
    bool no_display;

    DISALLOW_COPY_AND_ASSIGN(Registration);
  };

  class Observer {
   public:
    // Called at the end of UpdateApplicationList() with lists of app_ids for
    // apps which have been updated, removed, and inserted.
    virtual void OnRegistryUpdated(
        CrostiniRegistryService* registry_service,
        const std::vector<std::string>& updated_apps,
        const std::vector<std::string>& removed_apps,
        const std::vector<std::string>& inserted_apps) = 0;

   protected:
    virtual ~Observer() = default;
  };

  explicit CrostiniRegistryService(Profile* profile);
  ~CrostiniRegistryService() override;

  std::vector<std::string> GetRegisteredAppIds() const;

  // Return null if |app_id| is not found in the registry.
  std::unique_ptr<CrostiniRegistryService::Registration> GetRegistration(
      const std::string& app_id) const;

  // The existing list of apps is replaced by |application_list|.
  void UpdateApplicationList(const vm_tools::apps::ApplicationList& app_list);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

 private:
  // Owned by the BrowserContext.
  PrefService* const prefs_;

  base::ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(CrostiniRegistryService);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_REGISTRY_SERVICE_H_
