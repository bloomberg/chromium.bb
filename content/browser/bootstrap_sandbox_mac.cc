// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/bootstrap_sandbox_mac.h"

#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "content/common/sandbox_init_mac.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/common/sandbox_type_mac.h"
#include "sandbox/mac/bootstrap_sandbox.h"

namespace content {

namespace {

// This class is responsible for creating the BootstrapSandbox global
// singleton, as well as registering all associated policies with it.
class BootstrapSandboxPolicy : public BrowserChildProcessObserver {
 public:
  static BootstrapSandboxPolicy* GetInstance();

  sandbox::BootstrapSandbox* sandbox() const {
    return sandbox_.get();
  }

  // BrowserChildProcessObserver:
  virtual void BrowserChildProcessHostDisconnected(
      const ChildProcessData& data) OVERRIDE;
  virtual void BrowserChildProcessCrashed(
      const ChildProcessData& data) OVERRIDE;

 private:
  friend struct DefaultSingletonTraits<BootstrapSandboxPolicy>;
  BootstrapSandboxPolicy();
  virtual ~BootstrapSandboxPolicy();

  void RegisterSandboxPolicies();

  scoped_ptr<sandbox::BootstrapSandbox> sandbox_;
};

BootstrapSandboxPolicy* BootstrapSandboxPolicy::GetInstance() {
  return Singleton<BootstrapSandboxPolicy>::get();
}

void BootstrapSandboxPolicy::BrowserChildProcessHostDisconnected(
      const ChildProcessData& data) {
  sandbox()->ChildDied(data.handle);
}

void BootstrapSandboxPolicy::BrowserChildProcessCrashed(
      const ChildProcessData& data) {
  sandbox()->ChildDied(data.handle);
}

BootstrapSandboxPolicy::BootstrapSandboxPolicy()
    : sandbox_(sandbox::BootstrapSandbox::Create()) {
  CHECK(sandbox_.get());
  BrowserChildProcessObserver::Add(this);
  RegisterSandboxPolicies();
}

BootstrapSandboxPolicy::~BootstrapSandboxPolicy() {
  BrowserChildProcessObserver::Remove(this);
}

void BootstrapSandboxPolicy::RegisterSandboxPolicies() {
}

}  // namespace

bool ShouldEnableBootstrapSandbox() {
  return base::mac::IsOSMountainLionOrEarlier() ||
         base::mac::IsOSMavericks();
}

sandbox::BootstrapSandbox* GetBootstrapSandbox() {
  return BootstrapSandboxPolicy::GetInstance()->sandbox();
}

}  // namespace content
