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

namespace crostini {

// The CrostiniRegistryService stores information about Desktop Entries (apps)
// in Crostini. We store this in prefs so that it is readily available even when
// the VM isn't running. The registrations here correspond to .desktop files,
// which are detailed in the spec:
// https://www.freedesktop.org/wiki/Specifications/desktop-entry-spec/

// This class deals with several types of IDs, including:
// 1) Desktop File IDs (desktop_file_id):
//    - As per the desktop entry spec.
// 2) Crostini App List Ids (app_id):
//    - Valid extensions ids for apps stored in the registry, derived from the
//    desktop file id, vm name, and container name.
//    - The default Terminal app is a special case app list item, without an
//    entry in the registry.
// 3) Exo Window App Ids (window_app_id):
//    - Retrieved from exo::ShellSurface::GetApplicationId()
//    - For Wayland apps, this is the surface class of the app
//    - For X apps, this is of the form org.chromium.termina.wmclass.foo when
//    WM_CLASS is set to foo, or otherwise some string prefixed by
//    "org.chromium.termina." when WM_CLASS is not set.
// 4) Shelf App Ids (shelf_app_id):
//    - Used in ash::ShelfID::app_id
//    - Either a Window App Id prefixed by "crostini:" or a Crostini App Id.
//    - For pinned apps, this is a Crostini App Id.
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

  // Returns a shelf app id for an exo window id.
  //
  // If the given window app id is not for Crostini (i.e. Arc++), returns an
  // empty string. If we can uniquely identify a registry entry, returns the
  // crostini app id for that. Otherwise, returns the |window_app_id|, possibly
  // prefixed "org.chromium.termina.wayland." if it was not already prefixed.
  //
  // As the window app id is derived from fields set by the app itself, it is
  // possible for an app to masquerade as a different app.
  std::string GetCrostiniShelfAppId(const std::string& window_app_id,
                                    const std::string* window_startup_id);
  // Returns whether the app_id is a Crostini app id.
  bool IsCrostiniShelfAppId(const std::string& shelf_app_id);

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

}  // namespace crostini

#endif  // CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_REGISTRY_SERVICE_H_
