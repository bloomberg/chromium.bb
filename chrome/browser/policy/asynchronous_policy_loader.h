// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_ASYNCHRONOUS_POLICY_LOADER_H_
#define CHROME_BROWSER_POLICY_ASYNCHRONOUS_POLICY_LOADER_H_
#pragma once

#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/values.h"
#include "chrome/browser/policy/asynchronous_policy_provider.h"

namespace policy {

class ConfigurationPolicyProvider;

// Used by the implementation of asynchronous policy provider to manage the
// tasks on the file thread that do the heavy lifting of loading policies.
class AsynchronousPolicyLoader
    : public base::RefCountedThreadSafe<AsynchronousPolicyLoader> {
 public:
  explicit AsynchronousPolicyLoader(
      AsynchronousPolicyProvider::Delegate* delegate);

  // Triggers initial policy load.
  virtual void Init();

  // Reloads policy, sending notification of changes if necessary. Must be
  // called on the file thread.
  virtual void Reload();

  // Stops any pending reload tasks.
  virtual void Stop();

  // Associates a provider with the loader to allow the loader to notify the
  // provider when policy changes.
  void SetProvider(AsynchronousPolicyProvider* provider);

  const DictionaryValue* policy() const { return policy_.get(); }

 protected:
  friend class UpdatePolicyTask;

  // AsynchronousPolicyLoader objects should only be deleted by
  // RefCountedThreadSafe.
  friend class base::RefCountedThreadSafe<AsynchronousPolicyLoader>;
  virtual ~AsynchronousPolicyLoader();

  // Schedules a call to UpdatePolicy on |origin_loop_|. Takes ownership of
  // |new_policy|.
  void PostUpdatePolicyTask(DictionaryValue* new_policy);

  AsynchronousPolicyProvider::Delegate* delegate() {
    return delegate_.get();
  }

  AsynchronousPolicyProvider* provider() {
    return provider_;
  }

 private:
  friend class AsynchronousPolicyLoaderTest;

  // Replaces the existing policy to value map with a new one, sending
  // notification to the provider if there is a policy change. Must be called on
  // |origin_loop_| so that it's safe to call back into the provider, which is
  // not thread-safe. Takes ownership of |new_policy|.
  void UpdatePolicy(DictionaryValue* new_policy);

  // Provides the low-level mechanics for loading policy.
  scoped_ptr<AsynchronousPolicyProvider::Delegate> delegate_;

  // Current policy.
  scoped_ptr<DictionaryValue> policy_;

  // The provider this loader is associated with. Access only on the thread that
  // called the constructor. See |origin_loop_| below.
  AsynchronousPolicyProvider* provider_;

  // The message loop on which this object was constructed. Recorded so that
  // it's possible to call back into the non thread safe provider to fire the
  // notification.
  MessageLoop* origin_loop_;

  DISALLOW_COPY_AND_ASSIGN(AsynchronousPolicyLoader);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_ASYNCHRONOUS_POLICY_LOADER_H_
