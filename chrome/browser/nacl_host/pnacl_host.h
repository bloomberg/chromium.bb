// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NACL_HOST_PNACL_HOST_H_
#define CHROME_BROWSER_NACL_HOST_PNACL_HOST_H_

#include <map>

#include "base/callback_forward.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/nacl_host/nacl_file_host.h"
#include "components/nacl/common/pnacl_types.h"
#include "ipc/ipc_platform_file.h"

namespace pnacl {
class PnaclHostTest;
class PnaclTranslationCache;
}

// Shared state (translation cache) and common utilities (temp file creation)
// for all PNaCl translations. Unless otherwise specified, all methods should be
// called on the IO thread.
class PnaclHost {
 public:
  typedef base::Callback<void(IPC::PlatformFileForTransit)> TempFileCallback;
  typedef base::Callback<void(IPC::PlatformFileForTransit, bool is_hit)>
      NexeFdCallback;

  static PnaclHost* GetInstance();

  PnaclHost();
  ~PnaclHost();
  void Init();

  // Creates a temporary file that will be deleted when the last handle
  // is closed, or earlier. Returns a PlatformFileForTransit usable by the
  // process identified by |process_handle|.
  void CreateTemporaryFile(base::ProcessHandle process_handle,
                           TempFileCallback cb);

  // Create a temporary file, which will be deleted by the time the last
  // handle is closed (or earlier on POSIX systems), to use for the nexe
  // with the cache information given in |cache_info|. The specific instance
  // is identified by the combination of |render_process_id| and |pp_instance|.
  // Returns by calling |cb| with a PlatformFileForTransit usable by the process
  // identified by |process_handle|. If the nexe is already present
  // in the cache, |is_hit| is set to true and the contents of the nexe
  // have been copied into the temporary file. Otherwise |is_hit| is set to
  // false and the temporary file will be writeable.
  // Currently the implementation is a stub, which always sets is_hit to false
  // and calls the implementation of CreateTemporaryFile.
  // If the cache request was a miss, the caller is expected to call
  // TranslationFinished after it finishes translation to allow the nexe to be
  // stored in the cache.
  void GetNexeFd(int render_process_id,
                 base::ProcessHandle process_handle,
                 int render_view_id,
                 int pp_instance,
                 const nacl::PnaclCacheInfo& cache_info,
                 const NexeFdCallback& cb);

  // Called after the translation of a pexe instance identified by
  // |render_process_id| and |pp_instance| finishes. If |success| is true,
  // store the nexe translated for the instance in the cache.
  void TranslationFinished(int render_process_id,
                           int pp_instance,
                           bool success);

  // Called when the renderer identified by |render_process_id| is closing.
  // Clean up any outstanding translations for that renderer.
  void RendererClosing(int render_process_id);

 private:
  // PnaclHost is a singleton because there is only one translation cache, and
  // so that the BrowsingDataRemover can clear it even if no translation has
  // ever been started.
  friend struct DefaultSingletonTraits<PnaclHost>;
  friend class pnacl::PnaclHostTest;
  enum CacheState {
    CacheUninitialized,
    CacheInitializing,
    CacheReady
  };
  struct PendingTranslation {
   public:
    PendingTranslation();
    ~PendingTranslation();
    int render_view_id;
    IPC::PlatformFileForTransit nexe_fd;
    NexeFdCallback callback;
    nacl::PnaclCacheInfo cache_info;
  };

  typedef std::pair<int, int> TranslationID;
  typedef std::map<TranslationID, PendingTranslation> PendingTranslationMap;

  void InitForTest(base::FilePath temp_dir);
  void OnCacheInitialized(int error);
  static IPC::PlatformFileForTransit DoCreateTemporaryFile(
      base::ProcessHandle process_handle,
      base::FilePath temp_dir_);
  void ReturnMiss(TranslationID id, IPC::PlatformFileForTransit fd);

  CacheState cache_state_;
  base::FilePath temp_dir_;
  scoped_ptr<pnacl::PnaclTranslationCache> disk_cache_;
  PendingTranslationMap pending_translations_;
  base::ThreadChecker thread_checker_;
  base::WeakPtrFactory<PnaclHost> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(PnaclHost);
};

#endif  // CHROME_BROWSER_NACL_HOST_PNACL_HOST_H_
