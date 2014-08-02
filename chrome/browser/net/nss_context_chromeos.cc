// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/nss_context.h"

#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/nss_util_internal.h"
#include "net/cert/nss_cert_database_chromeos.h"

namespace {

void* kDatabaseManagerKey = &kDatabaseManagerKey;

class NSSCertDatabaseChromeOSManager : public base::SupportsUserData::Data {
 public:
  typedef base::Callback<void(net::NSSCertDatabaseChromeOS*)>
      GetNSSCertDatabaseCallback;
  explicit NSSCertDatabaseChromeOSManager(const std::string& username_hash)
      : username_hash_(username_hash), weak_ptr_factory_(this) {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
    crypto::ScopedPK11Slot private_slot(crypto::GetPrivateSlotForChromeOSUser(
        username_hash,
        base::Bind(&NSSCertDatabaseChromeOSManager::DidGetPrivateSlot,
                   weak_ptr_factory_.GetWeakPtr())));
    if (private_slot)
      DidGetPrivateSlot(private_slot.Pass());
  }

  virtual ~NSSCertDatabaseChromeOSManager() {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  }

  net::NSSCertDatabaseChromeOS* GetNSSCertDatabase(
      const GetNSSCertDatabaseCallback& callback) {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

    if (nss_cert_database_)
      return nss_cert_database_.get();

    ready_callback_list_.push_back(callback);
    return NULL;
  }

 private:
  typedef std::vector<GetNSSCertDatabaseCallback> ReadyCallbackList;

  void DidGetPrivateSlot(crypto::ScopedPK11Slot private_slot) {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
    nss_cert_database_.reset(new net::NSSCertDatabaseChromeOS(
        crypto::GetPublicSlotForChromeOSUser(username_hash_),
        private_slot.Pass()));

    ReadyCallbackList callback_list;
    callback_list.swap(ready_callback_list_);
    for (ReadyCallbackList::iterator i = callback_list.begin();
         i != callback_list.end();
         ++i) {
      (*i).Run(nss_cert_database_.get());
    }
  }

  std::string username_hash_;
  scoped_ptr<net::NSSCertDatabaseChromeOS> nss_cert_database_;
  ReadyCallbackList ready_callback_list_;
  base::WeakPtrFactory<NSSCertDatabaseChromeOSManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NSSCertDatabaseChromeOSManager);
};

std::string GetUsername(content::ResourceContext* context) {
  return ProfileIOData::FromResourceContext(context)->username_hash();
}

net::NSSCertDatabaseChromeOS* GetNSSCertDatabaseChromeOS(
    content::ResourceContext* context,
    const NSSCertDatabaseChromeOSManager::GetNSSCertDatabaseCallback&
        callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  NSSCertDatabaseChromeOSManager* manager =
      static_cast<NSSCertDatabaseChromeOSManager*>(
          context->GetUserData(kDatabaseManagerKey));
  if (!manager) {
    manager = new NSSCertDatabaseChromeOSManager(GetUsername(context));
    context->SetUserData(kDatabaseManagerKey, manager);
  }
  return manager->GetNSSCertDatabase(callback);
}

void CallWithNSSCertDatabase(
    const base::Callback<void(net::NSSCertDatabase*)>& callback,
    net::NSSCertDatabaseChromeOS* db) {
  callback.Run(db);
}

void SetSystemSlot(crypto::ScopedPK11Slot system_slot,
                   net::NSSCertDatabaseChromeOS* db) {
  db->SetSystemSlot(system_slot.Pass());
}

void SetSystemSlotOfDBForResourceContext(content::ResourceContext* context,
                                         crypto::ScopedPK11Slot system_slot) {
  base::Callback<void(net::NSSCertDatabaseChromeOS*)> callback =
      base::Bind(&SetSystemSlot, base::Passed(&system_slot));

  net::NSSCertDatabaseChromeOS* db =
      GetNSSCertDatabaseChromeOS(context, callback);
  if (db)
    callback.Run(db);
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

net::NSSCertDatabase* GetNSSCertDatabaseForResourceContext(
    content::ResourceContext* context,
    const base::Callback<void(net::NSSCertDatabase*)>& callback) {
  return GetNSSCertDatabaseChromeOS(
      context, base::Bind(&CallWithNSSCertDatabase, callback));
}

void EnableNSSSystemKeySlotForResourceContext(
    content::ResourceContext* context) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  base::Callback<void(crypto::ScopedPK11Slot)> callback =
      base::Bind(&SetSystemSlotOfDBForResourceContext, context);
  crypto::ScopedPK11Slot system_slot = crypto::GetSystemNSSKeySlot(callback);
  if (system_slot)
    callback.Run(system_slot.Pass());
}
