// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_CHROMEOS_NETWORK_NETWORK_LIST_VIEW_BASE_H_
#define ASH_COMMON_SYSTEM_CHROMEOS_NETWORK_NETWORK_LIST_VIEW_BASE_H_

#include <string>

#include "base/macros.h"

namespace views {
class View;
}

namespace ash {

// Base class for a list of available networks (and, in the case of VPNs, the
// list of available VPN providers).
class NetworkListViewBase {
 public:
  NetworkListViewBase();
  virtual ~NetworkListViewBase();

  void set_container(views::View* container) { container_ = container; }

  // Refreshes the network list.
  virtual void Update() = 0;

  // Checks whether |view| represents a network in the list. If yes, sets
  // |guid| to the network's guid and returns |true|. Otherwise,
  // leaves |guid| unchanged and returns |false|.
  virtual bool IsNetworkEntry(views::View* view, std::string* guid) const = 0;

 protected:
  views::View* container() { return container_; }

 private:
  // The container that holds the actual list entries.
  views::View* container_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(NetworkListViewBase);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_CHROMEOS_NETWORK_NETWORK_LIST_VIEW_BASE_H_
