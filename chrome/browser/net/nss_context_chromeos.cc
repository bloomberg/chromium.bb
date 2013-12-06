// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/nss_context.h"

#include "chrome/browser/profiles/profile_io_data.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/nss_util_internal.h"

namespace {
std::string GetUsername(content::ResourceContext* context) {
  return ProfileIOData::FromResourceContext(context)->username_hash();
}
}  // namespace

crypto::ScopedPK11Slot GetPublicNSSKeySlotForResourceContext(
    content::ResourceContext* context) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  return crypto::GetPublicSlotForChromeOSUser(GetUsername(context));
}

crypto::ScopedPK11Slot GetPrivateNSSKeySlotForResourceContext(
    content::ResourceContext* context,
    const base::Callback<void(crypto::ScopedPK11Slot)>& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  return crypto::GetPrivateSlotForChromeOSUser(GetUsername(context), callback);
}

