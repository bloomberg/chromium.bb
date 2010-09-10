// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CONFIG_DIR_POLICY_PROVIDER_H_
#define CHROME_BROWSER_POLICY_CONFIG_DIR_POLICY_PROVIDER_H_
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

class ConfigDirPolicyProvider;

// FilePathWatcher delegate implementation that handles change notifications for
// the configuration directory. It keeps the authorative version of the
// currently effective policy dictionary and updates it as appropriate.
class PolicyDirLoader : public FilePathWatcher::Delegate {
 public:
  // Creates a new loader that'll load its data from |config_dir|. The
  // parameters |settle_interval_seconds| and |reload_interval_minutes| specify
  // the time to wait before reading the directory contents after a change and
  // the period for checking |config_dir| for changes, respectively.
  PolicyDirLoader(base::WeakPtr<ConfigDirPolicyProvider> provider,
                  const FilePath& config_dir,
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

  const FilePath& config_dir() { return config_dir_; }

  // FilePathWatcher::Delegate implementation:
  void OnFilePathChanged(const FilePath& path);
  void OnError();

 private:
  // Loads the policy information. Ownership of the return value is transferred
  // to the caller.
  DictionaryValue* Load();

  // Checks the directory modification time to see whether reading the
  // configuration directory is safe. If not, returns false and the delay until
  // it is considered safe to reload in |delay|.
  bool IsSafeToReloadPolicy(const base::Time& now, base::TimeDelta* delay);

  // Schedules a reload task to run when |delay| expires. Must be called on the
  // file thread.
  void ScheduleReloadTask(const base::TimeDelta& delay);

  // Notifies the policy provider to send out a policy changed notification.
  // Must be called on |origin_loop_|.
  void NotifyPolicyChanged();

  // Invoked from the reload task on the file thread.
  void ReloadFromTask();

  // The provider this loader is associated with. Access only on the thread that
  // called the constructor. See |origin_loop_| below.
  base::WeakPtr<ConfigDirPolicyProvider> provider_;

  // The message loop on which this object was constructed and |provider_|
  // received on. Recorded so we can call back into the non thread safe provider
  // to fire the notification.
  MessageLoop* origin_loop_;

  // The directory in which we look for configuration files.
  const FilePath config_dir_;

  // Records last known modification timestamp of |config_dir_|.
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

  DISALLOW_COPY_AND_ASSIGN(PolicyDirLoader);
};

// Wraps a FilePathWatcher for the configuration directory and takes care of
// initializing the watcher object on the file thread.
class PolicyDirWatcher : public base::RefCountedThreadSafe<PolicyDirWatcher> {
 public:
  PolicyDirWatcher() {}

  // Runs initialization. This is in a separate method since we need to post a
  // task (which cannot be done from the constructor).
  void Init(PolicyDirLoader* loader);

 private:
  // PolicyDirWatcher objects should only be deleted by RefCountedThreadSafe.
  friend class base::RefCountedThreadSafe<PolicyDirWatcher>;
  ~PolicyDirWatcher() {}

  // Actually sets up the watch with the FilePathWatcher code.
  void InitWatcher(const scoped_refptr<PolicyDirLoader>& loader);

  // Wrapped watcher that takes care of the actual watching.
  FilePathWatcher watcher_;

  DISALLOW_COPY_AND_ASSIGN(PolicyDirWatcher);
};

// A policy provider implementation backed by a set of files in a given
// directory. The files should contain JSON-formatted policy settings. They are
// merged together and the result is returned via the
// ConfigurationPolicyProvider interface. The files are consulted in
// lexicographic file name order, so the last value read takes precedence in
// case of preference key collisions.
class ConfigDirPolicyProvider
    : public ConfigurationPolicyProvider,
      public base::SupportsWeakPtr<ConfigDirPolicyProvider> {
 public:
  explicit ConfigDirPolicyProvider(
      const ConfigurationPolicyProvider::StaticPolicyValueMap& policy_map,
      const FilePath& config_dir);
  virtual ~ConfigDirPolicyProvider();

  // ConfigurationPolicyProvider implementation.
  virtual bool Provide(ConfigurationPolicyStore* store);

 private:
  // Decodes the value tree and writes the configuration to the given |store|.
  void DecodePolicyValueTree(DictionaryValue* policies,
                             ConfigurationPolicyStore* store);

  // Watches for changes to the configuration directory.
  scoped_refptr<PolicyDirWatcher> watcher_;

  // The loader object we use internally.
  scoped_refptr<PolicyDirLoader> loader_;

  DISALLOW_COPY_AND_ASSIGN(ConfigDirPolicyProvider);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CONFIG_DIR_POLICY_PROVIDER_H_
