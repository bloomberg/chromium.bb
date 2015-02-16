// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_NETWORKING_CONFIG_DELEGATE_H_
#define ASH_NETWORKING_CONFIG_DELEGATE_H_

#include <string>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"

namespace ash {

// This delegate allows the UI code in ash, e.g. |NetworkStateListDetailedView|,
// to access the |NetworkingConfigService| in order to determine whether the
// configuration of a network identified by its |service_path| is controlled by
// an extension.
class NetworkingConfigDelegate {
 public:
  // A struct that provides information about the extension controlling the
  // configuration of a network.
  struct ASH_EXPORT ExtensionInfo {
    ExtensionInfo(const std::string& id, const std::string& name);
    ~ExtensionInfo();
    std::string extension_id;
    std::string extension_name;
  };

  virtual ~NetworkingConfigDelegate() {}

  // Returns information about the extension registered to control configuration
  // of the network |service_path|. If no extension is registered, the
  // scoped_ptr is empty.
  virtual scoped_ptr<const ExtensionInfo> LookUpExtensionForNetwork(
      const std::string& service_path) = 0;

 private:
  DISALLOW_ASSIGN(NetworkingConfigDelegate);
};

}  // namespace ash

#endif  // ASH_NETWORKING_CONFIG_DELEGATE_H_
