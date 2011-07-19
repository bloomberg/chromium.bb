// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_FILE_BASED_POLICY_LOADER_H_
#define CHROME_BROWSER_POLICY_FILE_BASED_POLICY_LOADER_H_
#pragma once

#include "base/files/file_path_watcher.h"
#include "chrome/browser/policy/asynchronous_policy_loader.h"
#include "chrome/browser/policy/file_based_policy_provider.h"

namespace policy {

// A customized asynchronous policy loader that handles loading policy from a
// file using a FilePathWatcher. The loader creates a fallback task to load
// policy periodically in case the watcher fails and retries policy loads when
// the watched file is in flux.
class FileBasedPolicyLoader : public AsynchronousPolicyLoader {
 public:
  FileBasedPolicyLoader(
      FileBasedPolicyProvider::ProviderDelegate* provider_delegate);

  // AsynchronousPolicyLoader overrides:
  virtual void Reload();

  void OnFilePathChanged(const FilePath& path);
  void OnFilePathError(const FilePath& path);

 protected:
  // FileBasedPolicyLoader objects should only be deleted by
  // RefCountedThreadSafe.
  friend class base::RefCountedThreadSafe<AsynchronousPolicyLoader>;
  virtual ~FileBasedPolicyLoader();

  const FilePath& config_file_path() { return config_file_path_; }

  // AsynchronousPolicyLoader overrides:

  // Creates the file path watcher and configures it to watch
  // |config_file_path_|.  Must be called on the file thread.
  virtual void InitOnFileThread();
  virtual void StopOnFileThread();

 private:
  // Checks whether policy information is safe to read. If not, returns false
  // and then delays until it is considered safe to reload in |delay|.
  // Must be called on the file thread.
  bool IsSafeToReloadPolicy(const base::Time& now, base::TimeDelta* delay);

  // The path at which we look for configuration files.
  const FilePath config_file_path_;

  // Managed with a scoped_ptr rather than being declared as an inline member to
  // decouple the watcher's life cycle from the loader's. This decoupling makes
  // it possible to destroy the watcher before the loader's destructor is called
  // (e.g. during Stop), since |watcher_| internally holds a reference to the
  // loader and keeps it alive.
  scoped_ptr<base::files::FilePathWatcher> watcher_;

  // Settle interval.
  const base::TimeDelta settle_interval_;

  // Records last known modification timestamp of |config_file_path_|.
  base::Time last_modification_file_;

  // The wall clock time at which the last modification timestamp was
  // recorded.  It's better to not assume the file notification time and the
  // wall clock times come from the same source, just in case there is some
  // non-local filesystem involved.
  base::Time last_modification_clock_;

  DISALLOW_COPY_AND_ASSIGN(FileBasedPolicyLoader);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_FILE_BASED_POLICY_LOADER_H_
