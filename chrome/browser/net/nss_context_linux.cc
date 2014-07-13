// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/nss_context.h"

#include "content/public/browser/browser_thread.h"
#include "crypto/nss_util_internal.h"
#include "net/cert/nss_cert_database.h"

crypto::ScopedPK11Slot GetPublicNSSKeySlotForResourceContext(
    content::ResourceContext* context) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  return crypto::ScopedPK11Slot(crypto::GetPersistentNSSKeySlot());
}

crypto::ScopedPK11Slot GetPrivateNSSKeySlotForResourceContext(
    content::ResourceContext* context,
    const base::Callback<void(crypto::ScopedPK11Slot)>& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  return crypto::ScopedPK11Slot(crypto::GetPersistentNSSKeySlot());
}

net::NSSCertDatabase* GetNSSCertDatabaseForResourceContext(
    content::ResourceContext* context,
    const base::Callback<void(net::NSSCertDatabase*)>& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  return net::NSSCertDatabase::GetInstance();
}
