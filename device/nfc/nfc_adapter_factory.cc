// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/nfc/nfc_adapter_factory.h"

#include "base/lazy_instance.h"
#include "base/memory/weak_ptr.h"

namespace device {

// static
bool NfcAdapterFactory::IsNfcAvailable() {
  // TODO(armansito): Return true on supported platforms here.
  return false;
}

// static
void NfcAdapterFactory::GetAdapter(const AdapterCallback& /* callback */) {
  // TODO(armansito): Create platform-specific implementation instances here.
  // No platform currently supports NFC, so we never invoke the callback.
}

}  // namespace device
