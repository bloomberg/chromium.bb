// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/crypto/cssm_init.h"

#include <Security/SecBase.h>

#include "base/logging.h"
#include "base/singleton.h"
#include "base/synchronization/lock.h"
#include "base/sys_string_conversions.h"

// When writing crypto code for Mac OS X, you may find the following
// documentation useful:
// - Common Security: CDSA and CSSM, Version 2 (with corrigenda)
//   http://www.opengroup.org/security/cdsa.htm
// - Apple Cryptographic Service Provider Functional Specification
// - CryptoSample: http://developer.apple.com/SampleCode/CryptoSample/

namespace {

class CSSMInitSingleton {
 public:
  static CSSMInitSingleton* GetInstance() {
    return Singleton<CSSMInitSingleton,
                     LeakySingletonTraits<CSSMInitSingleton> >::get();
  }

  CSSM_CSP_HANDLE csp_handle() const  { return csp_handle_; }

 private:
  CSSMInitSingleton() : inited_(false), loaded_(false), csp_handle_(NULL) {
    static CSSM_VERSION version = {2, 0};
    // TODO(wtc): what should our caller GUID be?
    static const CSSM_GUID test_guid = {
      0xFADE, 0, 0, { 1, 2, 3, 4, 5, 6, 7, 0 }
    };
    CSSM_RETURN crtn;
    CSSM_PVC_MODE pvc_policy = CSSM_PVC_NONE;
    crtn = CSSM_Init(&version, CSSM_PRIVILEGE_SCOPE_NONE, &test_guid,
                     CSSM_KEY_HIERARCHY_NONE, &pvc_policy, NULL);
    if (crtn) {
      NOTREACHED();
      return;
    }
    inited_ = true;

    crtn = CSSM_ModuleLoad(&gGuidAppleCSP, CSSM_KEY_HIERARCHY_NONE, NULL, NULL);
    if (crtn) {
      NOTREACHED();
      return;
    }
    loaded_ = true;

    crtn = CSSM_ModuleAttach(&gGuidAppleCSP, &version,
                             &base::kCssmMemoryFunctions, 0,
                             CSSM_SERVICE_CSP, 0, CSSM_KEY_HIERARCHY_NONE,
                             NULL, 0, NULL, &csp_handle_);
    DCHECK(crtn == CSSM_OK);
  }

  ~CSSMInitSingleton() {
    CSSM_RETURN crtn;
    if (csp_handle_) {
      CSSM_RETURN crtn = CSSM_ModuleDetach(csp_handle_);
      DCHECK(crtn == CSSM_OK);
    }
    if (loaded_) {
      crtn = CSSM_ModuleUnload(&gGuidAppleCSP, NULL, NULL);
      DCHECK(crtn == CSSM_OK);
    }
    if (inited_) {
      crtn = CSSM_Terminate();
      DCHECK(crtn == CSSM_OK);
    }
  }

  bool inited_;  // True if CSSM_Init has been called successfully.
  bool loaded_;  // True if CSSM_ModuleLoad has been called successfully.
  CSSM_CSP_HANDLE csp_handle_;

  friend struct DefaultSingletonTraits<CSSMInitSingleton>;
};

// This singleton is separate as it pertains to Apple's wrappers over
// their own CSSM handles, as opposed to our own CSSM_CSP_HANDLE.
class SecurityServicesSingleton {
 public:
  static SecurityServicesSingleton* GetInstance() {
    return Singleton<SecurityServicesSingleton,
                     LeakySingletonTraits<SecurityServicesSingleton> >::get();
  }

  base::Lock& lock() { return lock_; }

 private:
  friend struct DefaultSingletonTraits<SecurityServicesSingleton>;

  SecurityServicesSingleton() {}
  ~SecurityServicesSingleton() {}

  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(SecurityServicesSingleton);
};

}  // namespace

namespace base {

void EnsureCSSMInit() {
  CSSMInitSingleton::GetInstance();
}

CSSM_CSP_HANDLE GetSharedCSPHandle() {
  return CSSMInitSingleton::GetInstance()->csp_handle();
}

void* CSSMMalloc(CSSM_SIZE size, void *alloc_ref) {
  return malloc(size);
}

void CSSMFree(void* mem_ptr, void* alloc_ref) {
  free(mem_ptr);
}

void* CSSMRealloc(void* ptr, CSSM_SIZE size, void* alloc_ref) {
  return realloc(ptr, size);
}

void* CSSMCalloc(uint32 num, CSSM_SIZE size, void* alloc_ref) {
  return calloc(num, size);
}

const CSSM_API_MEMORY_FUNCS kCssmMemoryFunctions = {
  CSSMMalloc,
  CSSMFree,
  CSSMRealloc,
  CSSMCalloc,
  NULL
};

void LogCSSMError(const char *fn_name, CSSM_RETURN err) {
  if (!err)
    return;
  CFStringRef cfstr = SecCopyErrorMessageString(err, NULL);
  if (cfstr) {
    std::string err_name = SysCFStringRefToUTF8(cfstr);
    CFRelease(cfstr);
    LOG(ERROR) << fn_name << " returned " << err << " (" << err_name << ")";
  } else {
    LOG(ERROR) << fn_name << " returned " << err;
  }
}

base::Lock& GetMacSecurityServicesLock() {
  return SecurityServicesSingleton::GetInstance()->lock();
}

}  // namespace base
