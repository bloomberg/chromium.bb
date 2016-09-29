// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/vpn_delegate_mus.h"

namespace ash {

VPNDelegateMus::VPNDelegateMus() {}

VPNDelegateMus::~VPNDelegateMus() {}

bool VPNDelegateMus::HaveThirdPartyVPNProviders() const {
  // TODO(mash): http://crbug.com/651148
  return false;
}

const std::vector<VPNProvider>& VPNDelegateMus::GetVPNProviders() const {
  // TODO(mash): http://crbug.com/651148
  return providers_;
}

void VPNDelegateMus::ShowAddPage(const VPNProvider::Key& key) {
  // TODO(mash): http://crbug.com/651148
}

}  // namespace ash
