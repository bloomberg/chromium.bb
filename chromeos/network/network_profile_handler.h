// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_NETWORK_PROFILE_HANDLER_H_
#define CHROMEOS_NETWORK_NETWORK_PROFILE_HANDLER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/shill_property_changed_observer.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_profile.h"

namespace base {
class DictionaryValue;
}

namespace chromeos {

class NetworkProfileObserver;
class NetworkStateHandler;

class CHROMEOS_EXPORT NetworkProfileHandler
    : public ShillPropertyChangedObserver {
 public:
  typedef std::vector<NetworkProfile> ProfileList;

  virtual ~NetworkProfileHandler();

  void AddObserver(NetworkProfileObserver* observer);
  void RemoveObserver(NetworkProfileObserver* observer);

  void GetManagerPropertiesCallback(DBusMethodCallStatus call_status,
                                    const base::DictionaryValue& properties);

  // ShillPropertyChangedObserver overrides
  virtual void OnPropertyChanged(const std::string& name,
                                 const base::Value& value) OVERRIDE;

  void GetProfilePropertiesCallback(const std::string& profile_path,
                                    const base::DictionaryValue& properties);

  const NetworkProfile* GetProfileForPath(
      const std::string& profile_path) const;
  const NetworkProfile* GetProfileForUserhash(
      const std::string& userhash) const;

  // Returns the first profile entry with a non-empty userhash.
  // TODO(stevenjb): Replace with GetProfileForUserhash() with the correct
  // userhash.
  const NetworkProfile* GetDefaultUserProfile() const;

  static std::string GetSharedProfilePath();

 protected:
  friend class ClientCertResolverTest;
  friend class NetworkConnectionHandlerTest;
  friend class NetworkHandler;
  NetworkProfileHandler();

  // Add ShillManagerClient property observer and request initial list.
  void Init();

  void AddProfile(const NetworkProfile& profile);
  void RemoveProfile(const std::string& profile_path);

 private:
  ProfileList profiles_;
  ObserverList<NetworkProfileObserver> observers_;

  // For Shill client callbacks
  base::WeakPtrFactory<NetworkProfileHandler> weak_ptr_factory_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkProfileHandler);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_NETWORK_PROFILE_HANDLER_H_
