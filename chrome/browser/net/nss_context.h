// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_NSS_CONTEXT_H_
#define CHROME_BROWSER_NET_NSS_CONTEXT_H_

#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "crypto/scoped_nss_types.h"

namespace content {
class ResourceContext;
}  // namespace content

// Returns a reference to the public slot for the user associated with
// |context|.  Should be called only on the IO thread.
crypto::ScopedPK11Slot GetPublicNSSKeySlotForResourceContext(
    content::ResourceContext* context);

// Returns a reference to the private slot for the user associated with
// |context|, if it is loaded. If it is not loaded and |callback| is non-null,
// the |callback| will be run once the slot is loaded.
// Should be called only on the IO thread.
crypto::ScopedPK11Slot GetPrivateNSSKeySlotForResourceContext(
    content::ResourceContext* context,
    const base::Callback<void(crypto::ScopedPK11Slot)>& callback)
    WARN_UNUSED_RESULT;

#endif  // CHROME_BROWSER_NET_NSS_CONTEXT_H_
