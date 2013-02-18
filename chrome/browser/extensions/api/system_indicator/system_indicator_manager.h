// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INDICATOR_SYSTEM_INDICATOR_MANAGER_H_
#define CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INDICATOR_SYSTEM_INDICATOR_MANAGER_H_

#include <map>
#include <string>

#include "base/memory/linked_ptr.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_action_icon_factory.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class ExtensionAction;
class Profile;
class StatusTray;

FORWARD_DECLARE_TEST(SystemIndicatorApiTest, SystemIndicator);

namespace extensions {

class ExtensionIndicatorIcon;

// Keeps track of all the systemIndicator icons created for a given Profile
// that are currently visible in the UI.  Use SystemIndicatorManagerFactory to
// create a SystemIndicatorManager object.
class SystemIndicatorManager : public content::NotificationObserver,
                               public ProfileKeyedService {
 public:
  SystemIndicatorManager(Profile* profile, StatusTray* status_tray);
  virtual ~SystemIndicatorManager();

  // ProfileKeyedService implementation.
  virtual void Shutdown() OVERRIDE;

  // content::NotificationDelegate implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(::SystemIndicatorApiTest, SystemIndicator);

  // Determines whether the indicator should be hidden or shown and calls the
  // appropriate function.
  void OnSystemIndicatorChanged(const ExtensionAction* extension_action);

  // Causes a call to OnStatusIconClicked for the specified extension_id.
  // Returns false if no ExtensionIndicatorIcon is found for the extension.
  bool SendClickEventToExtensionForTest(const std::string extension_id);

  // Causes an indicator to be shown for the given extension_action.  Creates
  // the indicator if necessary.
  void CreateOrUpdateIndicator(
      const Extension* extension,
      const ExtensionAction* extension_action);

  // Causes the indicator for the given extension to be hidden.
  void RemoveIndicator(const std::string &extension_id);

  typedef std::map<const std::string, linked_ptr<ExtensionIndicatorIcon> >
      SystemIndicatorMap;

  Profile* profile_;
  StatusTray* status_tray_;
  SystemIndicatorMap system_indicators_;
  content::NotificationRegistrar registrar_;
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(SystemIndicatorManager);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INDICATOR_SYSTEM_INDICATOR_MANAGER_H_
