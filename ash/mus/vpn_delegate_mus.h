// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_VPN_DELEGATE_MUS_H_
#define ASH_MUS_VPN_DELEGATE_MUS_H_

#include "ash/common/system/chromeos/network/vpn_delegate.h"
#include "base/macros.h"

namespace ash {

// VPN delegate for mustash. Fetches VPN data from browser process over mojo.
class VPNDelegateMus : public VPNDelegate {
 public:
  VPNDelegateMus();
  ~VPNDelegateMus() override;

 private:
  // VPNDelegate:
  void ShowAddPage(const std::string& extension_id) override;

  DISALLOW_COPY_AND_ASSIGN(VPNDelegateMus);
};

}  // namespace ash

#endif  // ASH_MUS_VPN_DELEGATE_MUS_H_
