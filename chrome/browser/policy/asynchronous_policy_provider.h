// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_ASYNCHRONOUS_POLICY_PROVIDER_H_
#define CHROME_BROWSER_POLICY_ASYNCHRONOUS_POLICY_PROVIDER_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/browser/policy/configuration_policy_provider.h"

namespace policy {

class AsynchronousPolicyLoader;
class PolicyBundle;

// Policy provider that loads policy asynchronously. Providers should subclass
// from this class if loading the policy requires disk access or must for some
// other reason be performed on the file thread. The actual logic for loading
// policy is handled by a delegate passed at construction time.
class AsynchronousPolicyProvider
    : public ConfigurationPolicyProvider,
      public base::NonThreadSafe {
 public:
  // Must be implemented by subclasses of the asynchronous policy provider to
  // provide the implementation details of how policy is loaded.
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Load policy from the delegate's source, and return a PolicyBundle.
    virtual scoped_ptr<PolicyBundle> Load() = 0;
  };

  // Assumes ownership of |loader|.
  AsynchronousPolicyProvider(
      const PolicyDefinitionList* policy_list,
      scoped_refptr<AsynchronousPolicyLoader> loader);
  virtual ~AsynchronousPolicyProvider();

  // ConfigurationPolicyProvider implementation.
  virtual void RefreshPolicies() OVERRIDE;

 private:
  // Used to trigger a Reload on |loader| on the FILE thread.
  static void PostReloadOnFileThread(AsynchronousPolicyLoader* loader);

  // Used to notify UI that a reload task has been submitted.
  void OnReloadPosted();

  // Callback from the loader. This is invoked whenever the loader has completed
  // a reload of the policies.
  void OnLoaderReloaded(scoped_ptr<PolicyBundle> bundle);

  // The loader object used internally.
  scoped_refptr<AsynchronousPolicyLoader> loader_;

  // Number of refreshes requested whose reload is still pending. Used to only
  // fire notifications when all pending refreshes are done.
  int pending_refreshes_;

  // Used to post tasks to self on UI.
  base::WeakPtrFactory<AsynchronousPolicyProvider> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AsynchronousPolicyProvider);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_ASYNCHRONOUS_POLICY_PROVIDER_H_
