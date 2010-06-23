// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/crypto/capi_util.h"

#include "base/basictypes.h"
#include "base/lock.h"
#include "base/singleton.h"

namespace {

class CAPIUtilSingleton {
 public:
  static CAPIUtilSingleton* GetInstance() {
    return Singleton<CAPIUtilSingleton>::get();
  }

  // Returns a lock to guard calls to CryptAcquireContext with
  // CRYPT_DELETEKEYSET or CRYPT_NEWKEYSET.
  Lock& acquire_context_lock() {
    return acquire_context_lock_;
  }

 private:
  friend class Singleton<CAPIUtilSingleton>;
  friend struct DefaultSingletonTraits<CAPIUtilSingleton>;

  CAPIUtilSingleton() {}

  Lock acquire_context_lock_;

  DISALLOW_COPY_AND_ASSIGN(CAPIUtilSingleton);
};

}  // namespace

namespace base {

BOOL CryptAcquireContextLocked(HCRYPTPROV* prov,
                               const TCHAR* container,
                               const TCHAR* provider,
                               DWORD prov_type,
                               DWORD flags)
{
  AutoLock lock(CAPIUtilSingleton::GetInstance()->acquire_context_lock());
  return CryptAcquireContext(prov, container, provider, prov_type, flags);
}

}  // namespace base
