// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_ERROR_CONSOLE_ERROR_CONSOLE_H_
#define CHROME_BROWSER_EXTENSIONS_ERROR_CONSOLE_ERROR_CONSOLE_H_

#include <deque>
#include <map>

#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/scoped_observer.h"
#include "base/threading/thread_checker.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/error_map.h"
#include "extensions/browser/extension_error.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"

namespace content {
class NotificationDetails;
class NotificationSource;
class RenderViewHost;
}

class ExtensionService;
class Profile;

namespace extensions {
class ErrorConsoleUnitTest;
class Extension;

// The ErrorConsole is a central object to which all extension errors are
// reported. This includes errors detected in extensions core, as well as
// runtime Javascript errors. If FeatureSwitch::error_console() is enabled these
// errors can be viewed at chrome://extensions in developer mode.
// This class is owned by ExtensionSystem, making it, in effect, a
// BrowserContext-keyed service.
class ErrorConsole : public content::NotificationObserver,
                     public ExtensionRegistryObserver {
 public:
  class Observer {
   public:
    // Sent when a new error is reported to the error console.
    virtual void OnErrorAdded(const ExtensionError* error) = 0;

    // Sent upon destruction to allow any observers to invalidate any references
    // they have to the error console.
    virtual void OnErrorConsoleDestroyed();
  };

  explicit ErrorConsole(Profile* profile);
  virtual ~ErrorConsole();

  // Convenience method to return the ErrorConsole for a given profile.
  static ErrorConsole* Get(Profile* profile);

  // Set whether or not errors of the specified |type| are stored for the
  // extension with the given |extension_id|. This will be stored in the
  // preferences.
  void SetReportingForExtension(const std::string& extension_id,
                                ExtensionError::Type type,
                                bool enabled);

  // Restore default reporting to the given extension.
  void UseDefaultReportingForExtension(const std::string& extension_id);

  // Report an extension error, and add it to the list.
  void ReportError(scoped_ptr<ExtensionError> error);

  // Get a collection of weak pointers to all errors relating to the extension
  // with the given |extension_id|.
  const ErrorList& GetErrorsForExtension(const std::string& extension_id) const;

  // Add or remove observers of the ErrorConsole to be notified of any errors
  // added.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns whether or not the ErrorConsole is enabled for the
  // chrome:extensions page or the Chrome Apps Developer Tools.
  //
  // TODO(rdevlin.cronin): These have different answers - ErrorConsole is
  // enabled by default in ADT, but only Dev Channel for chrome:extensions (or
  // with the commandline switch). Once we do a full launch, clean all this up.
  bool IsEnabledForChromeExtensionsPage() const;
  bool IsEnabledForAppsDeveloperTools() const;

  // Return whether or not the ErrorConsole is enabled.
  bool enabled() const { return enabled_; }

  // Return the number of entries (extensions) in the error map.
  size_t get_num_entries_for_test() const { return errors_.size(); }

  // Set the default reporting for all extensions.
  void set_default_reporting_for_test(ExtensionError::Type type, bool enabled) {
    default_mask_ =
        enabled ? default_mask_ | (1 << type) : default_mask_ & ~(1 << type);
  }

 private:
  // A map which stores the reporting preferences for each Extension. If there
  // is no entry in the map, it signals that the |default_mask_| should be used.
  typedef std::map<std::string, int32> ErrorPreferenceMap;

  // Checks whether or not the ErrorConsole should be enabled or disabled. If it
  // is in the wrong state, enables or disables it appropriately.
  void CheckEnabled();

  // Enable the error console for error collection and retention. This involves
  // subscribing to the appropriate notifications and fetching manifest errors.
  void Enable();

  // Disable the error console, removing the subscriptions to notifications and
  // removing all current errors.
  void Disable();

  // Called when the Developer Mode preference is changed; this is important
  // since we use this as a heuristic to determine if the console is enabled or
  // not.
  void OnPrefChanged();

  // ExtensionRegistry implementation. If the Apps Developer Tools app is
  // installed or uninstalled, we may need to turn the ErrorConsole on/off.
  virtual void OnExtensionUnloaded(content::BrowserContext* browser_context,
                                   const Extension* extension) OVERRIDE;
  virtual void OnExtensionLoaded(content::BrowserContext* browser_context,
                                 const Extension* extension) OVERRIDE;

  // Add manifest errors from an extension's install warnings.
  void AddManifestErrorsForExtension(const Extension* extension);

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Whether or not the error console should record errors. This is true if
  // the user is in developer mode, and at least one of the following is true:
  // - The Chrome Apps Developer Tools are installed.
  // - FeatureSwitch::error_console() is enabled.
  // - This is a Dev Channel release.
  bool enabled_;

  // Needed because ObserverList is not thread-safe.
  base::ThreadChecker thread_checker_;

  // The list of all observers for the ErrorConsole.
  ObserverList<Observer> observers_;

  // The errors which we have received so far.
  ErrorMap errors_;

  // The mapping of Extension's error-reporting preferences.
  ErrorPreferenceMap pref_map_;

  // The default mask to use if an Extension does not have specific settings.
  int32 default_mask_;

  // The profile with which the ErrorConsole is associated. Only collect errors
  // from extensions and RenderViews associated with this Profile (and it's
  // incognito fellow).
  Profile* profile_;

  content::NotificationRegistrar notification_registrar_;
  PrefChangeRegistrar pref_registrar_;

  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      registry_observer_;

  DISALLOW_COPY_AND_ASSIGN(ErrorConsole);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_ERROR_CONSOLE_ERROR_CONSOLE_H_
