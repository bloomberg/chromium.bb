// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_FILE_BASED_POLICY_PROVIDER_H_
#define CHROME_BROWSER_POLICY_FILE_BASED_POLICY_PROVIDER_H_
#pragma once

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/lock.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "base/weak_ptr.h"
#include "chrome/browser/file_path_watcher.h"
#include "chrome/browser/policy/configuration_policy_provider.h"

class CancelableTask;
class DictionaryValue;
class MessageLoop;

namespace policy {

class FileBasedPolicyLoader;
class FileBasedPolicyWatcher;

// File based policy provider that coordinates watching and reloading policy
// information from the configuration path. Actual logic for loading policy
// information is handled by a delegate passed at construction time.
class FileBasedPolicyProvider
    : public ConfigurationPolicyProvider,
      public base::SupportsWeakPtr<FileBasedPolicyProvider> {
 public:
  // Delegate interface for actual policy loading from the system.
  class Delegate {
   public:
    virtual ~Delegate();

    // Loads the policy information. Ownership of the return value is
    // transferred to the caller.
    virtual DictionaryValue* Load() = 0;

    // Gets the last modification timestamp for the policy information from the
    // filesystem. Returns base::Time() if the information is not present, in
    // which case Load() should return an empty dictionary.
    virtual base::Time GetLastModification() = 0;

    const FilePath& config_file_path() { return config_file_path_; }

   protected:
    explicit Delegate(const FilePath& config_file_path);

   private:
    // The path at which we look for configuration files.
    const FilePath config_file_path_;

    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  // Assumes ownership of |delegate|.
  FileBasedPolicyProvider(const PolicyDefinitionList* policy_list,
                          Delegate* delegate);
  virtual ~FileBasedPolicyProvider();

  // ConfigurationPolicyProvider implementation.
  virtual bool Provide(ConfigurationPolicyStoreInterface* store);

 private:
  // Decodes the value tree and writes the configuration to the given |store|.
  void DecodePolicyValueTree(DictionaryValue* policies,
                             ConfigurationPolicyStoreInterface* store);

  // Watches for changes to the configuration directory.
  scoped_refptr<FileBasedPolicyWatcher> watcher_;

  // The loader object we use internally.
  scoped_refptr<FileBasedPolicyLoader> loader_;

  DISALLOW_COPY_AND_ASSIGN(FileBasedPolicyProvider);
};

// FilePathWatcher delegate implementation that handles change notifications for
// the configuration file or directory. It keeps the authorative version of the
// currently effective policy dictionary and updates it as appropriate. The
// actual loading logic is handled by a delegate.
class FileBasedPolicyLoader : public FilePathWatcher::Delegate {
 public:
  // Creates a new loader that'll load its data from |config_file_path|.
  // Assumes ownership of |delegate|, which  provides the actual loading logic.
  // The parameters |settle_interval_seconds| and |reload_interval_minutes|
  // specify the time to wait before reading the file contents after a change
  // and the period for checking |config_file_path| for changes, respectively.
  FileBasedPolicyLoader(base::WeakPtr<ConfigurationPolicyProvider> provider,
                        FileBasedPolicyProvider::Delegate* delegate,
                        int settle_interval_seconds,
                        int reload_interval_minutes);

  // Stops any pending reload tasks.
  void Stop();

  // Reloads the policies and sends out a notification, if appropriate. Must be
  // called on the file thread.
  void Reload();

  // Gets the current dictionary value object. Ownership of the returned value
  // is transferred to the caller.
  DictionaryValue* GetPolicy();

  const FilePath& config_file_path() { return delegate_->config_file_path(); }

  // FilePathWatcher::Delegate implementation:
  void OnFilePathChanged(const FilePath& path);
  void OnError();

 private:
  // FileBasedPolicyLoader objects should only be deleted by
  // RefCountedThreadSafe.
  friend class base::RefCountedThreadSafe<FileBasedPolicyLoader>;
  virtual ~FileBasedPolicyLoader();

  // Checks whether reading policy information  is safe to do. If not, returns
  // false and the delay until it is considered safe to reload in |delay|.
  bool IsSafeToReloadPolicy(const base::Time& now, base::TimeDelta* delay);

  // Schedules a reload task to run when |delay| expires. Must be called on the
  // file thread.
  void ScheduleReloadTask(const base::TimeDelta& delay);

  // Notifies the policy provider to send out a policy changed notification.
  // Must be called on |origin_loop_|.
  void NotifyPolicyChanged();

  // Invoked from the reload task on the file thread.
  void ReloadFromTask();

  // The delegate.
  scoped_ptr<FileBasedPolicyProvider::Delegate> delegate_;

  // The provider this loader is associated with. Access only on the thread that
  // called the constructor. See |origin_loop_| below.
  base::WeakPtr<ConfigurationPolicyProvider> provider_;

  // The message loop on which this object was constructed and |provider_|
  // received on. Recorded so we can call back into the non thread safe provider
  // to fire the notification.
  MessageLoop* origin_loop_;

  // Records last known modification timestamp of |config_file_path_|.
  base::Time last_modification_file_;

  // The wall clock time at which the last modification timestamp was recorded.
  // It's better to not assume the file notification time and the wall clock
  // times come from the same source, just in case there is some non-local
  // filesystem involved.
  base::Time last_modification_clock_;

  // Protects |policy_|.
  Lock lock_;

  // The current policy definition.
  scoped_ptr<DictionaryValue> policy_;

  // The reload task. Access only on the file thread. Holds a reference to the
  // currently posted task, so we can cancel and repost it if necessary.
  CancelableTask* reload_task_;

  // Settle and reload intervals.
  const int settle_interval_seconds_;
  const int reload_interval_minutes_;

  DISALLOW_COPY_AND_ASSIGN(FileBasedPolicyLoader);
};

// Wraps a FilePathWatcher for the configuration path and takes care of
// initializing the watcher object on the file thread.
class FileBasedPolicyWatcher
    : public base::RefCountedThreadSafe<FileBasedPolicyWatcher> {
 public:
  FileBasedPolicyWatcher();

  // Runs initialization. This is in a separate method since we need to post a
  // task (which cannot be done from the constructor).
  void Init(FileBasedPolicyLoader* loader);

 private:
  // FileBasedPolicyWatcher objects should only be deleted by
  // RefCountedThreadSafe.
  friend class base::RefCountedThreadSafe<FileBasedPolicyWatcher>;
  virtual ~FileBasedPolicyWatcher();

  // Actually sets up the watch with the FilePathWatcher code.
  void InitWatcher(const scoped_refptr<FileBasedPolicyLoader>& loader);

  // Wrapped watcher that takes care of the actual watching.
  FilePathWatcher watcher_;

  DISALLOW_COPY_AND_ASSIGN(FileBasedPolicyWatcher);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_FILE_BASED_POLICY_PROVIDER_H_
