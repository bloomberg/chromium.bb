// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_SHILL_SHILL_PROFILE_CLIENT_H_
#define CHROMEOS_DBUS_SHILL_SHILL_PROFILE_CLIENT_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "chromeos/dbus/shill/fake_shill_simulated_result.h"
#include "chromeos/dbus/shill/shill_client_helper.h"

namespace base {
class DictionaryValue;
}  // namespace base

namespace dbus {
class Bus;
class ObjectPath;
}  // namespace dbus

namespace chromeos {

class ShillPropertyChangedObserver;

// ShillProfileClient is used to communicate with the Shill Profile
// service.  All methods should be called from the origin thread which
// initializes the DBusThreadManager instance.
class COMPONENT_EXPORT(SHILL_CLIENT) ShillProfileClient {
 public:
  typedef ShillClientHelper::DictionaryValueCallbackWithoutStatus
      DictionaryValueCallbackWithoutStatus;
  typedef ShillClientHelper::ErrorCallback ErrorCallback;

  // Interface for setting up services for testing. Accessed through
  // GetTestInterface(), only implemented in the stub implementation.
  // TODO(stevenjb): remove dependencies on entry_path -> service_path
  // mappings in some of the TestInterface implementations.
  class TestInterface {
   public:
    virtual void AddProfile(const std::string& profile_path,
                            const std::string& userhash) = 0;

    // Adds an entry to the profile only. |entry_path| corresponds to a
    // 'service_path' and a corresponding entry will be added to
    // ShillManagerClient ServiceCompleteList. No checking or updating of
    // ShillServiceClient is performed.
    virtual void AddEntry(const std::string& profile_path,
                          const std::string& entry_path,
                          const base::DictionaryValue& properties) = 0;

    // Adds a service to the profile, copying properties from the
    // ShillServiceClient entry matching |service_path|. Returns false if no
    // Service entry exists or if a Profile entry already exists. Also sets
    // the Profile property of the service in ShillServiceClient.
    virtual bool AddService(const std::string& profile_path,
                            const std::string& service_path) = 0;

    // Copies properties from the ShillServiceClient entry matching
    // |service_path| to the profile entry matching |profile_path|. Returns
    // false if no Service entry exits or if no Profile entry exists.
    virtual bool UpdateService(const std::string& profile_path,
                               const std::string& service_path) = 0;

    // Sets |profiles| to the current list of profile paths.
    virtual void GetProfilePaths(std::vector<std::string>* profiles) = 0;

    // Sets |profiles| to the current list of profile paths that contain an
    // entry for |service_path|.
    virtual void GetProfilePathsContainingService(
        const std::string& service_path,
        std::vector<std::string>* profiles) = 0;

    // Sets |properties| to the entry for |service_path|, sets |profile_path|
    // to the path of the profile with the entry, and returns true if the
    // service exists in any profile.
    virtual bool GetService(const std::string& service_path,
                            std::string* profile_path,
                            base::DictionaryValue* properties) = 0;

    // Returns true iff an entry sepcified via |service_path| exists in
    // any profile.
    virtual bool HasService(const std::string& service_path) = 0;

    // Remove all profile entries.
    virtual void ClearProfiles() = 0;

    // Makes DeleteEntry succeed, fail, or timeout.
    virtual void SetSimulateDeleteResult(
        FakeShillSimulatedResult delete_result) = 0;

   protected:
    virtual ~TestInterface() {}
  };

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates the global instance with a fake implementation.
  static void InitializeFake();

  // Destroys the global instance which must have been initialized.
  static void Shutdown();

  // Returns the global instance if initialized. May return null.
  static ShillProfileClient* Get();

  // Returns the shared profile path.
  static std::string GetSharedProfilePath();

  // Adds a property changed |observer| for the profile at |profile_path|.
  virtual void AddPropertyChangedObserver(
      const dbus::ObjectPath& profile_path,
      ShillPropertyChangedObserver* observer) = 0;

  // Removes a property changed |observer| for the profile at |profile_path|.
  virtual void RemovePropertyChangedObserver(
      const dbus::ObjectPath& profile_path,
      ShillPropertyChangedObserver* observer) = 0;

  // Calls GetProperties method.
  // |callback| is called after the method call succeeds.
  virtual void GetProperties(const dbus::ObjectPath& profile_path,
                             DictionaryValueCallbackWithoutStatus callback,
                             ErrorCallback error_callback) = 0;

  // Calls GetEntry method.
  // |callback| is called after the method call succeeds.
  virtual void GetEntry(const dbus::ObjectPath& profile_path,
                        const std::string& entry_path,
                        DictionaryValueCallbackWithoutStatus callback,
                        ErrorCallback error_callback) = 0;

  // Calls DeleteEntry method.
  // |callback| is called after the method call succeeds.
  virtual void DeleteEntry(const dbus::ObjectPath& profile_path,
                           const std::string& entry_path,
                           base::OnceClosure callback,
                           ErrorCallback error_callback) = 0;

  // Returns an interface for testing (stub only), or returns null.
  virtual TestInterface* GetTestInterface() = 0;

 protected:
  friend class ShillProfileClientTest;

  // Initialize/Shutdown should be used instead.
  ShillProfileClient();
  virtual ~ShillProfileClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(ShillProfileClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_SHILL_SHILL_PROFILE_CLIENT_H_
