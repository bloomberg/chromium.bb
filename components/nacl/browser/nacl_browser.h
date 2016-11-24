// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_BROWSER_NACL_BROWSER_H_
#define COMPONENTS_NACL_BROWSER_NACL_BROWSER_H_

#include <stdint.h>

#include <deque>
#include <memory>

#include "base/bind.h"
#include "base/containers/mru_cache.h"
#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/nacl/browser/nacl_browser_delegate.h"
#include "components/nacl/browser/nacl_validation_cache.h"

namespace base {
class FileProxy;
}

namespace nacl {

static const int kGdbDebugStubPortUnknown = -1;
static const int kGdbDebugStubPortUnused = 0;

// Open an immutable executable file that can be mmapped (or a read-only file).
// This function should only be called on a thread that can perform file IO.
base::File OpenNaClReadExecImpl(const base::FilePath& file_path,
                                bool is_executable);

// Represents shared state for all NaClProcessHost objects in the browser.
class NaClBrowser {
 public:
  static NaClBrowser* GetInstance();

  // Will it be possible to launch a NaCl process, eventually?
  bool IsOk() const;

  // Are we ready to launch a NaCl process now?  Implies IsOk().
  bool IsReady() const;

  // Attempt to asynchronously acquire all resources needed to start a process.
  // This method is idempotent - it is safe to call multiple times.
  void EnsureAllResourcesAvailable();

  // Enqueues reply() in the message loop when all the resources needed to start
  // a process have been acquired.
  void WaitForResources(const base::Closure& reply);

  // Asynchronously attempt to get the IRT open.
  // This is entailed by EnsureInitialized.  This method is exposed as part of
  // the public interface, however, so the IRT can be explicitly opened as
  // early as possible to prevent autoupdate issues.
  void EnsureIrtAvailable();

  // Path to IRT. Available even before IRT is loaded.
  const base::FilePath& GetIrtFilePath();

  // IRT file handle, only available when IsReady().
  const base::File& IrtFile() const;

  // Methods for testing GDB debug stub in browser. If test adds debug stub
  // port listener, Chrome will allocate a currently-unused TCP port number for
  // debug stub server instead of a fixed one.

  // Notify listener that new debug stub TCP port is allocated.
  void SetProcessGdbDebugStubPort(int process_id, int port);
  void SetGdbDebugStubPortListener(base::Callback<void(int)> listener);
  void ClearGdbDebugStubPortListener();

  int GetProcessGdbDebugStubPort(int process_id);

  enum ValidationCacheStatus {
    CACHE_MISS = 0,
    CACHE_HIT,
    CACHE_MAX
  };

  bool ValidationCacheIsEnabled() const {
    return validation_cache_is_enabled_;
  }

  const std::string& GetValidationCacheKey() const {
    return validation_cache_.GetValidationCacheKey();
  }

  // The NaCl singleton keeps information about NaCl executable files opened via
  // PPAPI.  This allows the NaCl process to get trusted information about the
  // file directly from the browser process.  In theory, a compromised renderer
  // could provide a writable file handle or lie about the file's path.  If we
  // trusted the handle was read only but it was not, an mmapped file could be
  // modified after validation, allowing an escape from the NaCl sandbox.
  // Similarly, if we trusted the file path corresponded to the file handle but
  // it did not, the validation cache could be tricked into bypassing validation
  // for bad code.
  // Instead of allowing these attacks, the NaCl process only trusts information
  // it gets directly from the browser process.  Because the information is
  // stored in a cache of bounded size, it is not guaranteed the browser process
  // will be able to provide the requested information.  In these cases, the
  // NaCl process must make conservative assumptions about the origin of the
  // file.
  // In theory, a compromised renderer could guess file tokens in an attempt to
  // read files it normally doesn't have access to.  This would not compromise
  // the NaCl sandbox, however, and only has a 1 in ~2**120 chance of success
  // per guess.
  // TODO(ncbray): move the cache onto NaClProcessHost so that we don't need to
  // rely on tokens being unguessable by another process.
  void PutFilePath(const base::FilePath& path,
                   uint64_t* file_token_lo,
                   uint64_t* file_token_hi);
  bool GetFilePath(uint64_t file_token_lo,
                   uint64_t file_token_hi,
                   base::FilePath* path);

  bool QueryKnownToValidate(const std::string& signature, bool off_the_record);
  void SetKnownToValidate(const std::string& signature, bool off_the_record);
  void ClearValidationCache(const base::Closure& callback);
#if defined(OS_WIN)
  // Get path to NaCl loader on the filesystem if possible.
  // |exe_path| does not change if the method fails.
  bool GetNaCl64ExePath(base::FilePath* exe_path);
#endif

  void EarlyStartup();
  static void SetDelegate(NaClBrowserDelegate* delegate);
  static NaClBrowserDelegate* GetDelegate();

  // Each time a NaCl process ends, the browser is notified.
  void OnProcessEnd(int process_id);
  // Support for NaCl crash throttling.
  // Each time a NaCl module crashes, the browser is notified.
  void OnProcessCrashed();
  // If "too many" crashes occur within a given time period, NaCl is throttled
  // until the rate again drops below the threshold.
  bool IsThrottled();

 private:
  friend struct base::DefaultSingletonTraits<NaClBrowser>;

  enum NaClResourceState {
    NaClResourceUninitialized,
    NaClResourceRequested,
    NaClResourceReady
  };

  NaClBrowser();
  ~NaClBrowser();

  void InitIrtFilePath();

  void OpenIrtLibraryFile();

  void OnIrtOpened(std::unique_ptr<base::FileProxy> file_proxy,
                   base::File::Error error_code);

  void InitValidationCacheFilePath();
  void EnsureValidationCacheAvailable();
  void OnValidationCacheLoaded(const std::string* data);
  void RunWithoutValidationCache();

  // Dispatch waiting tasks if we are ready, or if we know we'll never be ready.
  void CheckWaiting();

  // Indicate that it is impossible to launch a NaCl process.
  void MarkAsFailed();

  void MarkValidationCacheAsModified();
  void PersistValidationCache();


  base::File irt_file_;
  base::FilePath irt_filepath_;
  NaClResourceState irt_state_;
  NaClValidationCache validation_cache_;
  NaClValidationCache off_the_record_validation_cache_;
  base::FilePath validation_cache_file_path_;
  bool validation_cache_is_enabled_;
  bool validation_cache_is_modified_;
  NaClResourceState validation_cache_state_;
  base::Callback<void(int)> debug_stub_port_listener_;

  // Map from process id to debug stub port if any.
  typedef std::map<int, int> GdbDebugStubPortMap;
  GdbDebugStubPortMap gdb_debug_stub_port_map_;

  typedef base::HashingMRUCache<std::string, base::FilePath> PathCacheType;
  PathCacheType path_cache_;

  bool ok_;

  // A list of pending tasks to start NaCl processes.
  std::vector<base::Closure> waiting_;

  std::unique_ptr<NaClBrowserDelegate> browser_delegate_;

  std::deque<base::Time> crash_times_;

  // Singletons get destroyed at shutdown.
  base::WeakPtrFactory<NaClBrowser> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(NaClBrowser);
};

} // namespace nacl

#endif  // COMPONENTS_NACL_BROWSER_NACL_BROWSER_H_
