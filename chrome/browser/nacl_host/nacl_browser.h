// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NACL_HOST_NACL_BROWSER_H_
#define CHROME_BROWSER_NACL_HOST_NACL_BROWSER_H_
#pragma once

#include "base/bind.h"
#include "base/file_util_proxy.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/platform_file.h"
#include "chrome/browser/nacl_host/nacl_validation_cache.h"

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
  const FilePath& GetIrtFilePath();

  // IRT file handle, only available when IsReady().
  base::PlatformFile IrtFile() const;

  NaClValidationCache validation_cache;

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

  // Dispatch waiting tasks if we are ready, or if we know we'll never be ready.
  void CheckWaiting();

  // Indicate that it is impossible to launch a NaCl process.
  void MarkAsFailed();

  // Singletons get destroyed at shutdown.
  base::WeakPtrFactory<NaClBrowser> weak_factory_;

  base::PlatformFile irt_platform_file_;
  FilePath irt_filepath_;

  NaClResourceState irt_state_;
  bool ok_;

  // A list of pending tasks to start NaCl processes.
  std::vector<base::Closure> waiting_;

  DISALLOW_COPY_AND_ASSIGN(NaClBrowser);
};

#endif  // CHROME_BROWSER_NACL_HOST_NACL_BROWSER_H_
