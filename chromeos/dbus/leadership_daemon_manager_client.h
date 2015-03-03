// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROMEOS_DBUS_LEADERSHIP_DAEMON_MANAGER_CLIENT_H_
#define CHROMEOS_DBUS_LEADERSHIP_DAEMON_MANAGER_CLIENT_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/values.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "dbus/object_path.h"
#include "dbus/property.h"

namespace chromeos {

// LeadershipDaemonManagerClient is used to communicate with the
// leaderd's Manager service.  All methods should be called from
// the origin thread which initializes the DBusThreadManager instance.
class CHROMEOS_EXPORT LeadershipDaemonManagerClient : public DBusClient {
 public:
  // Structure of properties associated with advertised groups.
  class GroupProperties : public dbus::PropertySet {
   public:
    GroupProperties(dbus::ObjectProxy* object_proxy,
                    const std::string& interface_name,
                    const PropertyChangedCallback& callback);
    ~GroupProperties() override;

    const std::string& leader_uuid() const { return leader_uuid_.value(); }
    const std::vector<std::string>& group_members() const {
      return group_members_.value();
    }

   private:
    dbus::Property<std::string> leader_uuid_;
    dbus::Property<std::vector<std::string>> group_members_;
  };

  // Interface for observing changes from a leadership daemon.
  class Observer {
   public:
    virtual ~Observer();

    // Called when the manager has been added.
    virtual void ManagerAdded();

    // Called when the manager has been removed.
    virtual void ManagerRemoved();

    // Called when the group with object path |object_path| is added to the
    // system.
    virtual void GroupAdded(const dbus::ObjectPath& object_path);

    // Called when the group with object path |object_path| is removed from
    // the system.
    virtual void GroupRemoved(const dbus::ObjectPath& object_path);

    // Called when the adapter with object path |object_path| has a
    // change in value of the property named |property_name|.
    virtual void GroupPropertyChanged(const dbus::ObjectPath& object_path,
                                      const std::string& property_name);
  };

  ~LeadershipDaemonManagerClient() override;

  // Factory function, creates a new instance which is owned by the caller.
  // For normal usage, access the singleton via DBusThreadManager::Get().
  static LeadershipDaemonManagerClient* Create();

  // Adds and removes observers for events on all leadership group
  // events. Check the |object_path| parameter of observer methods to
  // determine which group is issuing the event.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Calls JoinGroup method.
  // |callback| is called with its |call_status| argument set to
  // DBUS_METHOD_CALL_SUCCESS if the method call succeeds. Otherwise,
  // |callback| is called with |call_status| set to DBUS_METHOD_CALL_FAILURE.
  virtual void JoinGroup(const std::string& group,
                         const base::DictionaryValue& options,
                         const ObjectPathDBusMethodCallback& callback) = 0;

  // Calls LeaveGroup method.
  // |callback| is called with its |call_status| argument set to
  // DBUS_METHOD_CALL_SUCCESS if the method call succeeds. Otherwise,
  // |callback| is called with |call_status| set to DBUS_METHOD_CALL_FAILURE.
  virtual void LeaveGroup(const std::string& object_path,
                          const VoidDBusMethodCallback& callback) = 0;

  // Calls SetScore method.
  // |callback| is called with its |call_status| argument set to
  // DBUS_METHOD_CALL_SUCCESS if the method call succeeds. Otherwise,
  // |callback| is called with |call_status| set to DBUS_METHOD_CALL_FAILURE.
  virtual void SetScore(const std::string& object_path,
                        int score,
                        const VoidDBusMethodCallback& callback) = 0;

  // Calls PokeLeader method.
  // |callback| is called with its |call_status| argument set to
  // DBUS_METHOD_CALL_SUCCESS if the method call succeeds. Otherwise,
  // |callback| is called with |call_status| set to DBUS_METHOD_CALL_FAILURE.
  virtual void PokeLeader(const std::string& object_path,
                          const VoidDBusMethodCallback& callback) = 0;

  // Calls Ping method.
  // |callback| is called with its |call_status| argument set to
  // DBUS_METHOD_CALL_SUCCESS if the method call succeeds. Otherwise,
  // |callback| is called with |call_status| set to DBUS_METHOD_CALL_FAILURE.
  virtual void Ping(const StringDBusMethodCallback& callback) = 0;

  // Obtains the properties for the group with object path |object_path|,
  // any values should be copied if needed.
  virtual const GroupProperties* GetGroupProperties(
      const dbus::ObjectPath& object_path) = 0;

 protected:
  // Create() should be used instead.
  LeadershipDaemonManagerClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(LeadershipDaemonManagerClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_LEADERSHIP_DAEMON_MANAGER_CLIENT_H_
