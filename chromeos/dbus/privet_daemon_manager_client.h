// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_PRIVET_DAEMON_MANAGER_CLIENT_H_
#define CHROMEOS_DBUS_PRIVET_DAEMON_MANAGER_CLIENT_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "dbus/object_path.h"
#include "dbus/property.h"

namespace chromeos {

// PrivetDaemonManagerClient is used to communicate with the
// privetd service.  All methods should be called from
// the origin thread which initializes the DBusThreadManager instance.
class CHROMEOS_EXPORT PrivetDaemonManagerClient : public DBusClient {
 public:
  class PairingInfo {
   public:
    PairingInfo();
    ~PairingInfo();

    // Returns the value of the pairing code; not necessarily a printable
    // string.
    const std::vector<uint8_t>& code() const { return code_; }
    void set_code(const uint8_t* data, size_t length) {
      code_.assign(data, data + length);
    }

    // Returns the selected type of pairing (e.g. "pinCode", "embeddedCode").
    const std::string& mode() const { return mode_; }
    void set_mode(const std::string& mode) { mode_ = mode; }

    // Returns a unique identifier representing the pairing session.
    const std::string& session_id() const { return session_id_; }
    void set_session_id(const std::string& id) { session_id_ = id; }

    // Resets the values to empty values.
    void Clear();

   private:
    std::vector<uint8_t> code_;
    std::string mode_;
    std::string session_id_;
  };

  class PairingInfoProperty : public dbus::PropertyBase {
   public:
    bool PopValueFromReader(dbus::MessageReader* reader) override;
    void AppendSetValueToWriter(dbus::MessageWriter* writer) override;
    void ReplaceValueWithSetValue() override;

    const PairingInfo& value() const { return value_; }

   private:
    PairingInfo value_;
  };

  // Structure of properties associated with a privet Manager.
  class Properties : public dbus::PropertySet {
   public:
    Properties(dbus::ObjectProxy* object_proxy,
               const std::string& interface_name,
               const PropertyChangedCallback& callback);
    ~Properties() override;

    // State of WiFi bootstrapping.
    // Values are "disabled", "waiting", "connecting", "monitoring".
    const std::string& wifi_bootstrap_state() const {
      return wifi_bootstrap_state_.value();
    }

    // State of GCD bootstrapping.
    // Values are "disabled", "offline", "connecting", "waiting", "registering",
    // "online".
    const std::string& gcd_boostrap_state() const {
      return gcd_bootstrap_state_.value();
    }

    // State of device pairing.
    const PairingInfo& pairing_info() const { return pairing_info_.value(); }

    //  Concise note describing a peer.  Suitable for display to the user.
    const std::string& description() const { return description_.value(); }

    // Concise name describing a peer.  Suitable for display to the user.
    const std::string& name() const { return name_.value(); }

   private:
    dbus::Property<std::string> wifi_bootstrap_state_;
    dbus::Property<std::string> gcd_bootstrap_state_;
    PairingInfoProperty pairing_info_;
    dbus::Property<std::string> description_;
    dbus::Property<std::string> name_;

    DISALLOW_COPY_AND_ASSIGN(Properties);
  };

  // Interface for observing changes from a privet daemon.
  class Observer {
   public:
    virtual ~Observer();

    // Called when the manager has been added.
    virtual void ManagerAdded() = 0;

    // Called when the manager has been removed.
    virtual void ManagerRemoved() = 0;

    // Called when the manager has a change in value of the property named
    // |property_name|. Valid values are "Description", "GCDBootstrapState",
    // "Name", "PairingInfo", "WiFiBootstrapState".
    virtual void ManagerPropertyChanged(const std::string& property_name) = 0;
  };

  ~PrivetDaemonManagerClient() override;

  // Factory function, creates a new instance which is owned by the caller.
  // For normal usage, access the singleton via DBusThreadManager::Get().
  static PrivetDaemonManagerClient* Create();

  // Adds and removes observers for events on all privet daemon events.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Calls SetDescription method.
  // |callback| is called with its |call_status| argument set to
  // DBUS_METHOD_CALL_SUCCESS if the method call succeeds. Otherwise,
  // |callback| is called with |call_status| set to DBUS_METHOD_CALL_FAILURE.
  virtual void SetDescription(const std::string& description,
                              const VoidDBusMethodCallback& callback) = 0;

  // Obtains the properties for the manager any values should be
  // copied if needed.
  virtual const Properties* GetProperties() = 0;

 protected:
  // Create() should be used instead.
  PrivetDaemonManagerClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(PrivetDaemonManagerClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_PRIVET_DAEMON_MANAGER_CLIENT_H_
