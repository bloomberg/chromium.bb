// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_FAVORITE_STATE_H_
#define CHROMEOS_NETWORK_FAVORITE_STATE_H_

#include <string>

#include "base/values.h"
#include "chromeos/network/managed_state.h"
#include "chromeos/network/network_ui_data.h"
#include "components/onc/onc_constants.h"

namespace chromeos {

// A simple class to provide essential information for Services created
// by Shill corresponding to Profile Entries (i.e. 'preferred' or 'favorite'
// networks).
// Note: NetworkStateHandler will store an entry for each member of
// Manager.ServiceCompleteList, even for visible entries that are not
// saved. This is necessary to avoid unnecessarily re-requesting entries,
// and to limit the code complexity. It is also convenient for tracking the
// complete list of "known" networks. The IsInProfile() accessor is used to
// skip entries that are not actually saved in a profile.
class CHROMEOS_EXPORT FavoriteState : public ManagedState {
 public:
  explicit FavoriteState(const std::string& path);
  virtual ~FavoriteState();

  // ManagedState overrides
  virtual bool PropertyChanged(const std::string& key,
                               const base::Value& value) OVERRIDE;
  virtual void GetStateProperties(
      base::DictionaryValue* dictionary) const OVERRIDE;

  // Accessors
  const std::string& profile_path() const { return profile_path_; }
  const NetworkUIData& ui_data() const { return ui_data_; }
  const base::DictionaryValue& proxy_config() const { return proxy_config_; }
  const std::string& guid() const { return guid_; }

  // Returns true if this is a favorite stored in a profile (see note above).
  bool IsInProfile() const;

  // Returns true if the network properties are stored in a user profile.
  bool IsPrivate() const;

  // Returns a specifier for identifying this network in the absence of a GUID.
  // This should only be used by NetworkStateHandler for keeping track of
  // GUIDs assigned to unsaved networks.
  std::string GetSpecifier() const;

  // Set the GUID. Called exclusively by NetworkStateHandler.
  void SetGuid(const std::string& guid);

 private:
  std::string profile_path_;
  NetworkUIData ui_data_;
  std::string guid_;

  // Keep track of Service.Security. Only used for specifying wifi networks.
  std::string security_;

  // TODO(pneubeck): Remove this once (Managed)NetworkConfigurationHandler
  // provides proxy configuration. crbug.com/241775
  base::DictionaryValue proxy_config_;

  DISALLOW_COPY_AND_ASSIGN(FavoriteState);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_FAVORITE_STATE_H_
