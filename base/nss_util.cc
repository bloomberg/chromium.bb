// Copyright (c) 2008-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/nss_util.h"

#include <dlfcn.h>
#include <nss.h>
#include <plarena.h>
#include <prerror.h>
#include <prinit.h>
#include <prtime.h>
#include <pk11pub.h>
#include <secmod.h>
#include <ssl.h>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/singleton.h"
#include "base/string_util.h"

namespace {

std::string GetDefaultConfigDirectory() {
  const char* home = getenv("HOME");
  if (home == NULL) {
    LOG(ERROR) << "$HOME is not set.";
    return "";
  }
  FilePath dir(home);
  dir = dir.AppendASCII(".pki").AppendASCII("nssdb");
  if (!file_util::CreateDirectory(dir)) {
    LOG(ERROR) << "Failed to create ~/.pki/nssdb directory.";
    return "";
  }
  return dir.value();
}

// Load nss's built-in root certs.
SECMODModule *InitDefaultRootCerts() {
  const char* kModulePath = "libnssckbi.so";
  char modparams[1024];
  snprintf(modparams, sizeof(modparams),
          "name=\"Root Certs\" library=\"%s\"", kModulePath);
  SECMODModule *root = SECMOD_LoadUserModule(modparams, NULL, PR_FALSE);
  if (root)
    return root;

  // Aw, snap.  Can't find/load root cert shared library.
  // This will make it hard to talk to anybody via https.
  NOTREACHED();
  return NULL;
}

// A singleton to initialize/deinitialize NSPR.
// Separate from the NSS singleton because we initialize NSPR on the UI thread.
class NSPRInitSingleton {
 public:
  NSPRInitSingleton() {
    PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);
  }

  ~NSPRInitSingleton() {
    PRStatus prstatus = PR_Cleanup();
    if (prstatus != PR_SUCCESS) {
      LOG(ERROR) << "PR_Cleanup failed; was NSPR initialized on wrong thread?";
    }
  }
};

class NSSInitSingleton {
 public:
  NSSInitSingleton() {
    base::EnsureNSPRInit();

    // We *must* have NSS >= 3.12.3.  See bug 26448.
    COMPILE_ASSERT(
        (NSS_VMAJOR == 3 && NSS_VMINOR == 12 && NSS_VPATCH >= 3) ||
        (NSS_VMAJOR == 3 && NSS_VMINOR > 12) ||
        (NSS_VMAJOR > 3),
        nss_version_check_failed);
    // Also check the run-time NSS version.
    // NSS_VersionCheck is a >= check, not strict equality.
    CHECK(NSS_VersionCheck("3.12.3")) << "We depend on NSS >= 3.12.3";

    SECStatus status = SECFailure;
    std::string database_dir = GetDefaultConfigDirectory();
    if (!database_dir.empty()) {
      // Initialize with a persistant database (~/.pki/nssdb).
      // Use "sql:" which can be shared by multiple processes safely.
      std::string nss_config_dir =
          StringPrintf("sql:%s", database_dir.c_str());
      status = NSS_InitReadWrite(nss_config_dir.c_str());
      if (status != SECSuccess) {
        LOG(ERROR) << "Error initializing NSS with a persistent "
                      "database (" << nss_config_dir
                   << "): NSS error code " << PR_GetError();
      }
    }
    if (status != SECSuccess) {
      LOG(WARNING) << "Initialize NSS without a persistent database "
                      "(~/.pki/nssdb).";
      status = NSS_NoDB_Init(NULL);
      if (status != SECSuccess) {
        LOG(ERROR) << "Error initializing NSS without a persistent "
                      "database: NSS error code " << PR_GetError();
      }
    }

    // If we haven't initialized the password for the NSS databases,
    // initialize an empty-string password so that we don't need to
    // log in.
    PK11SlotInfo* slot = PK11_GetInternalKeySlot();
    if (slot) {
      if (PK11_NeedUserInit(slot))
        PK11_InitPin(slot, NULL, NULL);
      PK11_FreeSlot(slot);
    }

    root_ = InitDefaultRootCerts();

    NSS_SetDomesticPolicy();

#if defined(USE_SYSTEM_SSL)
    // Use late binding to avoid scary but benign warning
    // "Symbol `SSL_ImplementedCiphers' has different size in shared object,
    //  consider re-linking"
    const PRUint16* pSSL_ImplementedCiphers = static_cast<const PRUint16*>(
        dlsym(RTLD_DEFAULT, "SSL_ImplementedCiphers"));
    if (pSSL_ImplementedCiphers == NULL) {
      NOTREACHED() << "Can't get list of supported ciphers";
      return;
    }
#else
#define pSSL_ImplementedCiphers SSL_ImplementedCiphers
#endif

    // Explicitly enable exactly those ciphers with keys of at least 80 bits
    for (int i = 0; i < SSL_NumImplementedCiphers; i++) {
      SSLCipherSuiteInfo info;
      if (SSL_GetCipherSuiteInfo(pSSL_ImplementedCiphers[i], &info,
                                 sizeof(info)) == SECSuccess) {
        SSL_CipherPrefSetDefault(pSSL_ImplementedCiphers[i],
                                 (info.effectiveKeyBits >= 80));
      }
    }

    // Enable SSL
    SSL_OptionSetDefault(SSL_SECURITY, PR_TRUE);

    // All other SSL options are set per-session by SSLClientSocket.
  }

  ~NSSInitSingleton() {
    if (root_) {
      SECMOD_UnloadUserModule(root_);
      SECMOD_DestroyModule(root_);
      root_ = NULL;
    }

    // Have to clear the cache, or NSS_Shutdown fails with SEC_ERROR_BUSY
    SSL_ClearSessionCache();

    SECStatus status = NSS_Shutdown();
    if (status != SECSuccess) {
      // We LOG(INFO) because this failure is relatively harmless
      // (leaking, but we're shutting down anyway).
      LOG(INFO) << "NSS_Shutdown failed; see "
                   "http://code.google.com/p/chromium/issues/detail?id=4609";
    }

    PL_ArenaFinish();
  }

 private:
  SECMODModule *root_;
};

}  // namespace

namespace base {

void EnsureNSPRInit() {
  Singleton<NSPRInitSingleton>::get();
}

void EnsureNSSInit() {
  Singleton<NSSInitSingleton>::get();
}

// TODO(port): Implement this more simply.  We can convert by subtracting an
// offset (the difference between NSPR's and base::Time's epochs).
Time PRTimeToBaseTime(PRTime prtime) {
  PRExplodedTime prxtime;
  PR_ExplodeTime(prtime, PR_GMTParameters, &prxtime);

  base::Time::Exploded exploded;
  exploded.year         = prxtime.tm_year;
  exploded.month        = prxtime.tm_month + 1;
  exploded.day_of_week  = prxtime.tm_wday;
  exploded.day_of_month = prxtime.tm_mday;
  exploded.hour         = prxtime.tm_hour;
  exploded.minute       = prxtime.tm_min;
  exploded.second       = prxtime.tm_sec;
  exploded.millisecond  = prxtime.tm_usec / 1000;

  return Time::FromUTCExploded(exploded);
}

}  // namespace base
