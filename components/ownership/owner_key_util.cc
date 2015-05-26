// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ownership/owner_key_util.h"

namespace ownership {

///////////////////////////////////////////////////////////////////////////
// PublicKey

PublicKey::PublicKey() {
}

PublicKey::~PublicKey() {
}

///////////////////////////////////////////////////////////////////////////
// PrivateKey

PrivateKey::PrivateKey(crypto::ScopedSECKEYPrivateKey key) : key_(key.Pass()) {
}

PrivateKey::~PrivateKey() {
}

}  // namespace ownership
