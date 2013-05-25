// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NACL_HOST_NACL_BROWSER_H_
#define CHROME_BROWSER_NACL_HOST_NACL_BROWSER_H_

#include "base/bind.h"
#include "base/files/file_util_proxy.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/platform_file.h"
#include "chrome/browser/nacl_host/nacl_validation_cache.h"

class URLPattern;
class GURL;

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
  base::PlatformFile IrtFile() const;

  // Set match patterns which will be checked before enabling debug stub.
  void SetDebugPatterns(std::string debug_patterns);

  // Returns whether NaCl application with this manifest URL should be debugged.
  bool URLMatchesDebugPatterns(GURL manifest_url);

  // Methods for testing GDB debug stub in browser. If test adds debug stub
  // port listener, Chrome will allocate a currently-unused TCP port number for
  // debug stub server instead of a fixed one.

  // Notify listener that new debug stub TCP port is allocated.
  void FireGdbDebugStubPortOpened(int port);
  bool HasGdbDebugStubPortListener();
  void SetGdbDebugStubPortListener(base::Callback<void(int)> listener);
  void ClearGdbDebugStubPortListener();

  bool ValidationCacheIsEnabled() const {
    return validation_cache_is_enabled_;
  }

  const std::string& GetValidationCacheKey() const {
    return validation_cache_.GetValidationCacheKey();
  }

  bool QueryKnownToValidate(const std::string& signature, bool off_the_record);
  void SetKnownToValidate(const std::string& signature, bool off_the_record);
  void ClearValidationCache(const base::Closure& callback);

 private:
  friend struct DefaultSingletonTraits<NaClBrowser>;

  enum NaClResourceState {
    NaClResourceUninitialized,
    NaClResourceRequested,
    NaClResourceReady
  };

  NaClBrowser();
  ~NaClBrowser();

  void InitIrtFilePath();

  void OpenIrtLibraryFile();

  void OnIrtOpened(base::PlatformFileError error_code,
                   base::PassPlatformFile file, bool created);

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

  // Singletons get destroyed at shutdown.
  base::WeakPtrFactory<NaClBrowser> weak_factory_;

  base::PlatformFile irt_platform_file_;
  base::FilePath irt_filepath_;
  NaClResourceState irt_state_;
  std::vector<URLPattern> debug_patterns_;
  bool inverse_debug_patterns_;
  NaClValidationCache validation_cache_;
  NaClValidationCache off_the_record_validation_cache_;
  base::FilePath validation_cache_file_path_;
  bool validation_cache_is_enabled_;
  bool validation_cache_is_modified_;
  NaClResourceState validation_cache_state_;
  base::Callback<void(int)> debug_stub_port_listener_;

  bool ok_;

  // A list of pending tasks to start NaCl processes.
  std::vector<base::Closure> waiting_;

  DISALLOW_COPY_AND_ASSIGN(NaClBrowser);
};

#endif  // CHROME_BROWSER_NACL_HOST_NACL_BROWSER_H_
