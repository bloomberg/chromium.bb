// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the Chrome Extensions Managed Mode API relevant classes to realize
// the API as specified in the extension API JSON.

#ifndef CHROME_BROWSER_EXTENSIONS_API_MANAGED_MODE_MANAGED_MODE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_MANAGED_MODE_MANAGED_MODE_API_H_

#include "base/prefs/public/pref_change_registrar.h"
#include "chrome/browser/extensions/api/profile_keyed_api_factory.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_function.h"
#include "content/public/browser/notification_observer.h"

class Profile;

namespace extensions {

class ManagedModeEventRouter {
 public:
  explicit ManagedModeEventRouter(Profile* profile);
  virtual ~ManagedModeEventRouter();

 private:
  void OnInManagedModeChanged();

  PrefChangeRegistrar registrar_;
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(ManagedModeEventRouter);
};

class GetManagedModeFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("managedModePrivate.get", MANAGEDMODEPRIVATE_GET)

 protected:
  virtual ~GetManagedModeFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class EnterManagedModeFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("managedModePrivate.enter",
                             MANAGEDMODEPRIVATE_ENTER)

 protected:
  virtual ~EnterManagedModeFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;

 private:
  // Called when we have either successfully entered managed mode or failed.
  void SendResult(bool success);
};


class GetPolicyFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("managedModePrivate.getPolicy",
                             MANAGEDMODEPRIVATE_GETPOLICY)

 protected:
  virtual ~GetPolicyFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class SetPolicyFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("managedModePrivate.setPolicy",
                             MANAGEDMODEPRIVATE_SETPOLICY)

 protected:
  virtual ~SetPolicyFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class ManagedModeAPI : public ProfileKeyedAPI,
                       public extensions::EventRouter::Observer {
 public:
  explicit ManagedModeAPI(Profile* profile);
  virtual ~ManagedModeAPI();

  // ProfileKeyedService implementation.
  virtual void Shutdown() OVERRIDE;

  // ProfileKeyedAPIFactory implementation.
  static ProfileKeyedAPIFactory<ManagedModeAPI>* GetFactoryInstance();

  // EventRouter::Observer implementation.
  virtual void OnListenerAdded(const extensions::EventListenerInfo& details)
      OVERRIDE;

 private:
  friend class ProfileKeyedAPIFactory<ManagedModeAPI>;

  Profile* profile_;

  // ProfileKeyedAPI implementation.
  static const char* service_name() {
    return "ManagedModeAPI";
  }
  static const bool kServiceIsNULLWhileTesting = true;

  // Created lazily upon OnListenerAdded.
  scoped_ptr<ManagedModeEventRouter> managed_mode_event_router_;

  DISALLOW_COPY_AND_ASSIGN(ManagedModeAPI);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_MANAGED_MODE_MANAGED_MODE_API_H_
