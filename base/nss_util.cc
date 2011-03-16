// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/nss_util.h"
#include "base/nss_util_internal.h"

#include <nss.h>
#include <plarena.h>
#include <prerror.h>
#include <prinit.h>
#include <prtime.h>
#include <pk11pub.h>
#include <secmod.h>

#if defined(OS_LINUX)
#include <linux/nfs_fs.h>
#include <sys/vfs.h>
#endif

#include "base/environment.h"
#include "base/file_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/stringprintf.h"
#include "base/threading/thread_restrictions.h"

// USE_NSS means we use NSS for everything crypto-related.  If USE_NSS is not
// defined, such as on Mac and Windows, we use NSS for SSL only -- we don't
// use NSS for crypto or certificate verification, and we don't use the NSS
// certificate and key databases.
#if defined(USE_NSS)
#include "base/crypto/crypto_module_blocking_password_delegate.h"
#include "base/synchronization/lock.h"
#endif  // defined(USE_NSS)

namespace base {

namespace {

#if defined(USE_NSS)
FilePath GetDefaultConfigDirectory() {
  FilePath dir = file_util::GetHomeDir();
  if (dir.empty()) {
    LOG(ERROR) << "Failed to get home directory.";
    return dir;
  }
  dir = dir.AppendASCII(".pki").AppendASCII("nssdb");
  if (!file_util::CreateDirectory(dir)) {
    LOG(ERROR) << "Failed to create ~/.pki/nssdb directory.";
    dir.clear();
  }
  return dir;
}

// On non-chromeos platforms, return the default config directory.
// On chromeos, return a read-only directory with fake root CA certs for testing
// (which will not exist on non-testing images).  These root CA certs are used
// by the local Google Accounts server mock we use when testing our login code.
// If this directory is not present, NSS_Init() will fail.  It is up to the
// caller to failover to NSS_NoDB_Init() at that point.
FilePath GetInitialConfigDirectory() {
#if defined(OS_CHROMEOS)
  static const FilePath::CharType kReadOnlyCertDB[] =
      FILE_PATH_LITERAL("/etc/fake_root_ca/nssdb");
  return FilePath(kReadOnlyCertDB);
#else
  return GetDefaultConfigDirectory();
#endif  // defined(OS_CHROMEOS)
}

// This callback for NSS forwards all requests to a caller-specified
// CryptoModuleBlockingPasswordDelegate object.
char* PKCS11PasswordFunc(PK11SlotInfo* slot, PRBool retry, void* arg) {
  base::CryptoModuleBlockingPasswordDelegate* delegate =
      reinterpret_cast<base::CryptoModuleBlockingPasswordDelegate*>(arg);
  if (delegate) {
    bool cancelled = false;
    std::string password = delegate->RequestPassword(PK11_GetTokenName(slot),
                                                     retry != PR_FALSE,
                                                     &cancelled);
    if (cancelled)
      return NULL;
    char* result = PORT_Strdup(password.c_str());
    password.replace(0, password.size(), password.size(), 0);
    return result;
  }
  DLOG(ERROR) << "PK11 password requested with NULL arg";
  return NULL;
}

// NSS creates a local cache of the sqlite database if it detects that the
// filesystem the database is on is much slower than the local disk.  The
// detection doesn't work with the latest versions of sqlite, such as 3.6.22
// (NSS bug https://bugzilla.mozilla.org/show_bug.cgi?id=578561).  So we set
// the NSS environment variable NSS_SDB_USE_CACHE to "yes" to override NSS's
// detection when database_dir is on NFS.  See http://crbug.com/48585.
//
// TODO(wtc): port this function to other USE_NSS platforms.  It is defined
// only for OS_LINUX simply because the statfs structure is OS-specific.
//
// Because this function sets an environment variable it must be run before we
// go multi-threaded.
void UseLocalCacheOfNSSDatabaseIfNFS(const FilePath& database_dir) {
#if defined(OS_LINUX)
  struct statfs buf;
  if (statfs(database_dir.value().c_str(), &buf) == 0) {
    if (buf.f_type == NFS_SUPER_MAGIC) {
      scoped_ptr<Environment> env(Environment::Create());
      const char* use_cache_env_var = "NSS_SDB_USE_CACHE";
      if (!env->HasVar(use_cache_env_var))
        env->SetVar(use_cache_env_var, "yes");
    }
  }
#endif  // defined(OS_LINUX)
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
#endif  // defined(USE_NSS)

// A singleton to initialize/deinitialize NSPR.
// Separate from the NSS singleton because we initialize NSPR on the UI thread.
// Now that we're leaking the singleton, we could merge back with the NSS
// singleton.
class NSPRInitSingleton {
 private:
  friend struct DefaultLazyInstanceTraits<NSPRInitSingleton>;

  NSPRInitSingleton() {
    PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);
  }

  // NOTE(willchan): We don't actually execute this code since we leak NSS to
  // prevent non-joinable threads from using NSS after it's already been shut
  // down.
  ~NSPRInitSingleton() {
    PL_ArenaFinish();
    PRStatus prstatus = PR_Cleanup();
    if (prstatus != PR_SUCCESS) {
      LOG(ERROR) << "PR_Cleanup failed; was NSPR initialized on wrong thread?";
    }
  }
};

LazyInstance<NSPRInitSingleton, LeakyLazyInstanceTraits<NSPRInitSingleton> >
    g_nspr_singleton(LINKER_INITIALIZED);

class NSSInitSingleton {
 public:
#if defined(OS_CHROMEOS)
  void OpenPersistentNSSDB() {
    if (!chromeos_user_logged_in_) {
      // GetDefaultConfigDirectory causes us to do blocking IO on UI thread.
      // Temporarily allow it until we fix http://crbug.com.70119
      ThreadRestrictions::ScopedAllowIO allow_io;
      chromeos_user_logged_in_ = true;
      real_db_slot_ = OpenUserDB(GetDefaultConfigDirectory(),
                                 "Real NSS database");
    }
  }
#endif  // defined(OS_CHROMEOS)

  bool OpenTestNSSDB(const FilePath& path, const char* description) {
    test_db_slot_ = OpenUserDB(path, description);
    return !!test_db_slot_;
  }

  void CloseTestNSSDB() {
    if (test_db_slot_) {
      SECStatus status = SECMOD_CloseUserDB(test_db_slot_);
      if (status != SECSuccess)
        LOG(ERROR) << "SECMOD_CloseUserDB failed: " << PORT_GetError();
      PK11_FreeSlot(test_db_slot_);
      test_db_slot_ = NULL;
    }
  }

  PK11SlotInfo* GetDefaultKeySlot() {
    if (test_db_slot_)
      return PK11_ReferenceSlot(test_db_slot_);
    if (real_db_slot_)
      return PK11_ReferenceSlot(real_db_slot_);
    return PK11_GetInternalKeySlot();
  }

#if defined(USE_NSS)
  Lock* write_lock() {
    return &write_lock_;
  }
#endif  // defined(USE_NSS)

  // This method is used to force NSS to be initialized without a DB.
  // Call this method before NSSInitSingleton() is constructed.
  static void ForceNoDBInit() {
    force_nodb_init_ = true;
  }

 private:
  friend struct DefaultLazyInstanceTraits<NSSInitSingleton>;

  NSSInitSingleton()
      : real_db_slot_(NULL),
        test_db_slot_(NULL),
        root_(NULL),
        chromeos_user_logged_in_(false) {
    EnsureNSPRInit();

    // We *must* have NSS >= 3.12.3.  See bug 26448.
    COMPILE_ASSERT(
        (NSS_VMAJOR == 3 && NSS_VMINOR == 12 && NSS_VPATCH >= 3) ||
        (NSS_VMAJOR == 3 && NSS_VMINOR > 12) ||
        (NSS_VMAJOR > 3),
        nss_version_check_failed);
    // Also check the run-time NSS version.
    // NSS_VersionCheck is a >= check, not strict equality.
    if (!NSS_VersionCheck("3.12.3")) {
      // It turns out many people have misconfigured NSS setups, where
      // their run-time NSPR doesn't match the one their NSS was compiled
      // against.  So rather than aborting, complain loudly.
      LOG(ERROR) << "NSS_VersionCheck(\"3.12.3\") failed.  "
                    "We depend on NSS >= 3.12.3, and this error is not fatal "
                    "only because many people have busted NSS setups (for "
                    "example, using the wrong version of NSPR). "
                    "Please upgrade to the latest NSS and NSPR, and if you "
                    "still get this error, contact your distribution "
                    "maintainer.";
    }

    SECStatus status = SECFailure;
    bool nodb_init = force_nodb_init_;

#if !defined(USE_NSS)
    // Use the system certificate store, so initialize NSS without database.
    nodb_init = true;
#endif

    if (nodb_init) {
      status = NSS_NoDB_Init(NULL);
      if (status != SECSuccess) {
        LOG(ERROR) << "Error initializing NSS without a persistent "
            "database: NSS error code " << PR_GetError();
      }
    } else {
#if defined(USE_NSS)
      FilePath database_dir = GetInitialConfigDirectory();
      if (!database_dir.empty()) {
        // This duplicates the work which should have been done in
        // EarlySetupForNSSInit. However, this function is idempotent so
        // there's no harm done.
        UseLocalCacheOfNSSDatabaseIfNFS(database_dir);

        // Initialize with a persistent database (likely, ~/.pki/nssdb).
        // Use "sql:" which can be shared by multiple processes safely.
        std::string nss_config_dir =
            StringPrintf("sql:%s", database_dir.value().c_str());
#if defined(OS_CHROMEOS)
        status = NSS_Init(nss_config_dir.c_str());
#else
        status = NSS_InitReadWrite(nss_config_dir.c_str());
#endif
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
          return;
        }
      }

      PK11_SetPasswordFunc(PKCS11PasswordFunc);

      // If we haven't initialized the password for the NSS databases,
      // initialize an empty-string password so that we don't need to
      // log in.
      PK11SlotInfo* slot = PK11_GetInternalKeySlot();
      if (slot) {
        // PK11_InitPin may write to the keyDB, but no other thread can use NSS
        // yet, so we don't need to lock.
        if (PK11_NeedUserInit(slot))
          PK11_InitPin(slot, NULL, NULL);
        PK11_FreeSlot(slot);
      }

      root_ = InitDefaultRootCerts();
#endif  // defined(USE_NSS)
    }
  }

  // NOTE(willchan): We don't actually execute this code since we leak NSS to
  // prevent non-joinable threads from using NSS after it's already been shut
  // down.
  ~NSSInitSingleton() {
    if (real_db_slot_) {
      SECMOD_CloseUserDB(real_db_slot_);
      PK11_FreeSlot(real_db_slot_);
      real_db_slot_ = NULL;
    }
    CloseTestNSSDB();
    if (root_) {
      SECMOD_UnloadUserModule(root_);
      SECMOD_DestroyModule(root_);
      root_ = NULL;
    }

    SECStatus status = NSS_Shutdown();
    if (status != SECSuccess) {
      // We VLOG(1) because this failure is relatively harmless (leaking, but
      // we're shutting down anyway).
      VLOG(1) << "NSS_Shutdown failed; see "
                 "http://code.google.com/p/chromium/issues/detail?id=4609";
    }
  }

  static PK11SlotInfo* OpenUserDB(const FilePath& path,
                                  const char* description) {
    const std::string modspec =
        StringPrintf("configDir='sql:%s' tokenDescription='%s'",
                     path.value().c_str(), description);
    PK11SlotInfo* db_slot = SECMOD_OpenUserDB(modspec.c_str());
    if (db_slot) {
      if (PK11_NeedUserInit(db_slot))
        PK11_InitPin(db_slot, NULL, NULL);
    }
    else {
      LOG(ERROR) << "Error opening persistent database (" << modspec
                 << "): NSS error code " << PR_GetError();
    }
    return db_slot;
  }

  // If this is set to true NSS is forced to be initialized without a DB.
  static bool force_nodb_init_;

  PK11SlotInfo* real_db_slot_;  // Overrides internal key slot if non-NULL.
  PK11SlotInfo* test_db_slot_;  // Overrides internal key slot and real_db_slot_
  SECMODModule *root_;
  bool chromeos_user_logged_in_;
#if defined(USE_NSS)
  // TODO(davidben): When https://bugzilla.mozilla.org/show_bug.cgi?id=564011
  // is fixed, we will no longer need the lock.
  Lock write_lock_;
#endif  // defined(USE_NSS)
};

// static
bool NSSInitSingleton::force_nodb_init_ = false;

LazyInstance<NSSInitSingleton, LeakyLazyInstanceTraits<NSSInitSingleton> >
    g_nss_singleton(LINKER_INITIALIZED);

}  // namespace

#if defined(USE_NSS)
void EarlySetupForNSSInit() {
  FilePath database_dir = GetInitialConfigDirectory();
  if (!database_dir.empty())
    UseLocalCacheOfNSSDatabaseIfNFS(database_dir);
}
#endif

void EnsureNSPRInit() {
  g_nspr_singleton.Get();
}

void EnsureNSSInit() {
  // Initializing SSL causes us to do blocking IO.
  // Temporarily allow it until we fix
  //   http://code.google.com/p/chromium/issues/detail?id=59847
  ThreadRestrictions::ScopedAllowIO allow_io;
  g_nss_singleton.Get();
}

void ForceNSSNoDBInit() {
  NSSInitSingleton::ForceNoDBInit();
}

void DisableNSSForkCheck() {
  scoped_ptr<Environment> env(Environment::Create());
  env->SetVar("NSS_STRICT_NOFORK", "DISABLED");
}

bool CheckNSSVersion(const char* version) {
  return !!NSS_VersionCheck(version);
}

#if defined(USE_NSS)
bool OpenTestNSSDB(const FilePath& path, const char* description) {
  return g_nss_singleton.Get().OpenTestNSSDB(path, description);
}

void CloseTestNSSDB() {
  g_nss_singleton.Get().CloseTestNSSDB();
}

Lock* GetNSSWriteLock() {
  return g_nss_singleton.Get().write_lock();
}

AutoNSSWriteLock::AutoNSSWriteLock() : lock_(GetNSSWriteLock()) {
  // May be NULL if the lock is not needed in our version of NSS.
  if (lock_)
    lock_->Acquire();
}

AutoNSSWriteLock::~AutoNSSWriteLock() {
  if (lock_) {
    lock_->AssertAcquired();
    lock_->Release();
  }
}
#endif  // defined(USE_NSS)

#if defined(OS_CHROMEOS)
void OpenPersistentNSSDB() {
  g_nss_singleton.Get().OpenPersistentNSSDB();
}
#endif

// TODO(port): Implement this more simply.  We can convert by subtracting an
// offset (the difference between NSPR's and base::Time's epochs).
Time PRTimeToBaseTime(PRTime prtime) {
  PRExplodedTime prxtime;
  PR_ExplodeTime(prtime, PR_GMTParameters, &prxtime);

  Time::Exploded exploded;
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

PK11SlotInfo* GetDefaultNSSKeySlot() {
  return g_nss_singleton.Get().GetDefaultKeySlot();
}

}  // namespace base
