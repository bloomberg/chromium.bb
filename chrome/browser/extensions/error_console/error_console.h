// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_ERROR_CONSOLE_ERROR_CONSOLE_H_
#define CHROME_BROWSER_EXTENSIONS_ERROR_CONSOLE_ERROR_CONSOLE_H_

#include <deque>
#include <map>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/strings/string16.h"
#include "base/threading/thread_checker.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/extension_error.h"

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
// runtime Javascript errors.
// This class is owned by ExtensionSystem, making it, in effect, a
// BrowserContext-keyed service.
class ErrorConsole : public content::NotificationObserver {
 public:
  typedef std::deque<const ExtensionError*> ErrorList;
  typedef std::map<std::string, ErrorList> ErrorMap;

  class Observer {
   public:
    // Sent when a new error is reported to the error console.
    virtual void OnErrorAdded(const ExtensionError* error) = 0;

    // Sent upon destruction to allow any observers to invalidate any references
    // they have to the error console.
    virtual void OnErrorConsoleDestroyed();
  };

  explicit ErrorConsole(Profile* profile, ExtensionService* extension_service);
  virtual ~ErrorConsole();

  // Convenience method to return the ErrorConsole for a given profile.
  static ErrorConsole* Get(Profile* profile);

  // Report an extension error, and add it to the list.
  void ReportError(scoped_ptr<ExtensionError> error);

  // Get a collection of weak pointers to all errors relating to the extension
  // with the given |extension_id|.
  const ErrorList& GetErrorsForExtension(const std::string& extension_id) const;

  // Add or remove observers of the ErrorConsole to be notified of any errors
  // added.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  const ErrorMap& errors() const { return errors_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(ErrorConsoleUnitTest, AddAndRemoveErrors);

  // Enable the error console for error collection and retention. This involves
  // subscribing to the appropriate notifications and fetching manifest errors.
  void Enable(ExtensionService* extension_service);
  // Disable the error console, removing the subscriptions to notifications and
  // removing all current errors.
  void Disable();

  // Called when the Developer Mode preference is changed; this is important
  // since we use this as a heuristic to determine if the console is enabled or
  // not.
  void OnPrefChanged();

  // Add manifest errors from an extension's install warnings.
  void AddManifestErrorsForExtension(const Extension* extension);

  // Remove all errors which happened while incognito; we have to do this once
  // the incognito profile is destroyed.
  void RemoveIncognitoErrors();

  // Remove all errors relating to a particular |extension_id|.
  void RemoveErrorsForExtension(const std::string& extension_id);

  // Remove all errors for all extensions.
  void RemoveAllErrors();

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Whether or not the error console is enabled; it is enabled if the
  // FeatureSwitch (FeatureSwitch::error_console) is enabled and the user is
  // in Developer Mode.
  bool enabled_;

  // Needed because ObserverList is not thread-safe.
  base::ThreadChecker thread_checker_;

  // The list of all observers for the ErrorConsole.
  ObserverList<Observer> observers_;

  // The errors which we have received so far.
  ErrorMap errors_;

  // The profile with which the ErrorConsole is associated. Only collect errors
  // from extensions and RenderViews associated with this Profile (and it's
  // incognito fellow).
  Profile* profile_;

  content::NotificationRegistrar notification_registrar_;
  PrefChangeRegistrar pref_registrar_;

  DISALLOW_COPY_AND_ASSIGN(ErrorConsole);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_ERROR_CONSOLE_ERROR_CONSOLE_H_
