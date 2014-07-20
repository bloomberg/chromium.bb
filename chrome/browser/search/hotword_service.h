// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_HOTWORD_SERVICE_H_
#define CHROME_BROWSER_SEARCH_HOTWORD_SERVICE_H_

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/scoped_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"

class ExtensionService;
class HotwordClient;
class Profile;

namespace extensions {
class Extension;
class WebstoreStandaloneInstaller;
}  // namespace extensions

namespace hotword_internal {
// Constants for the hotword field trial.
extern const char kHotwordFieldTrialName[];
extern const char kHotwordFieldTrialDisabledGroupName[];
}  // namespace hotword_internal

// Provides an interface for the Hotword component that does voice triggered
// search.
class HotwordService : public content::NotificationObserver,
                       public extensions::ExtensionRegistryObserver,
                       public KeyedService {
 public:
  // Returns true if the hotword supports the current system language.
  static bool DoesHotwordSupportLanguage(Profile* profile);

  explicit HotwordService(Profile* profile);
  virtual ~HotwordService();

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Overridden from ExtensionRegisterObserver:
  virtual void OnExtensionInstalled(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension,
      bool is_update) OVERRIDE;
  virtual void OnExtensionUninstalled(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension,
      extensions::UninstallReason reason) OVERRIDE;

  // Checks for whether all the necessary files have downloaded to allow for
  // using the extension.
  virtual bool IsServiceAvailable();

  // Determine if hotwording is allowed in this profile based on field trials
  // and language.
  virtual bool IsHotwordAllowed();

  // Checks if the user has opted into audio logging. Returns true if the user
  // is opted in, false otherwise..
  bool IsOptedIntoAudioLogging();

  // Control the state of the hotword extension.
  void EnableHotwordExtension(ExtensionService* extension_service);
  void DisableHotwordExtension(ExtensionService* extension_service);

  // Handles enabling/disabling the hotword extension when the user
  // turns it off via the settings menu.
  void OnHotwordSearchEnabledChanged(const std::string& pref_name);

  // Called to handle the hotword session from |client|.
  void RequestHotwordSession(HotwordClient* client);
  void StopHotwordSession(HotwordClient* client);
  HotwordClient* client() { return client_; }

  // Checks if the current version of the hotword extension should be
  // uninstalled in order to update to a different language version.
  // Returns true if the extension was uninstalled.
  bool MaybeReinstallHotwordExtension();

  // Checks based on locale if the current version should be uninstalled so that
  // a version with a different language can be installed.
  bool ShouldReinstallHotwordExtension();

  // Helper functions pulled out for testing purposes.
  // UninstallHotwordExtension returns true if the extension was uninstalled.
  virtual bool UninstallHotwordExtension(ExtensionService* extension_service);
  virtual void InstallHotwordExtensionFromWebstore();

  // Sets the pref value of the previous language.
  void SetPreviousLanguagePref();

  // Returns the current error message id. A value of 0 indicates
  // no error.
  int error_message() { return error_message_; }

 private:
  Profile* profile_;

  PrefChangeRegistrar pref_registrar_;

  content::NotificationRegistrar registrar_;

  // For observing the ExtensionRegistry.
  ScopedObserver<extensions::ExtensionRegistry,
                 extensions::ExtensionRegistryObserver>
      extension_registry_observer_;

  scoped_refptr<extensions::WebstoreStandaloneInstaller> installer_;

  HotwordClient* client_;
  int error_message_;
  bool reinstall_pending_;

  base::WeakPtrFactory<HotwordService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(HotwordService);
};

#endif  // CHROME_BROWSER_SEARCH_HOTWORD_SERVICE_H_
