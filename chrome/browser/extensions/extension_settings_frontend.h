// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_FRONTEND_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_FRONTEND_H_
#pragma once

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list_threadsafe.h"
#include "chrome/browser/extensions/extension_settings_observer.h"
#include "chrome/browser/sync/api/syncable_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class FilePath;
class Profile;
class ExtensionSettingsBackend;
class ExtensionSettingsStorage;

// The component of extension settings which runs on the UI thread, as opposed
// to ExtensionSettingsBackend which lives on the FILE thread.
class ExtensionSettingsFrontend : public content::NotificationObserver {
 public:
  explicit ExtensionSettingsFrontend(Profile* profile);
  virtual ~ExtensionSettingsFrontend();

  typedef base::Callback<void(ExtensionSettingsBackend*)> BackendCallback;

  // Runs |callback| on the FILE thread with the extension settings.
  void RunWithBackend(const BackendCallback& callback);

  // Adds an observer to settings changes.
  void AddObserver(ExtensionSettingsObserver* observer);

  // Removes an observer to settings changes.
  void RemoveObserver(ExtensionSettingsObserver* observer);

  // NotificationObserver implementation.
  virtual void Observe(
      int type,
      const content::NotificationSource& source,
      const content::NotificationDetails& details) OVERRIDE;

 private:
  // Observer which sends events to a target profile iff the profile isn't the
  // originating profile for the event.
  class DefaultObserver;

  // Called when a profile is created.
  void OnProfileCreated(Profile* profile);

  // Called when a profile is destroyed.
  void OnProfileDestroyed(Profile* profile);

  // Creates a scoped DefaultObserver and adds it as an Observer.
  void SetDefaultObserver(
      Profile* profile, scoped_ptr<DefaultObserver>* observer);

  // If a scoped DefaultObserver exists, clears it and removes it as an
  // Observer.
  void ClearDefaultObserver(scoped_ptr<DefaultObserver>* observer);

  // The (original) Profile this Frontend belongs to.  Note that we don't store
  // the incognito version of the profile because it will change as it's
  // created and destroyed during the lifetime of Chrome.
  Profile* const profile_;

  // List of observers to settings changes.
  scoped_refptr<ObserverListThreadSafe<ExtensionSettingsObserver> > observers_;

  // The default original and incognito mode profile observers.
  scoped_ptr<DefaultObserver> original_profile_observer;
  scoped_ptr<DefaultObserver> incognito_profile_observer_;

  // Ref-counted container for the ExtensionSettingsBackend object.
  class Core;
  scoped_refptr<Core> core_;

  // For profile created/destroyed notifications.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionSettingsFrontend);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_FRONTEND_H_
