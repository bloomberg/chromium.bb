// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/test_extension_registry_observer.h"

#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"

namespace extensions {

class TestExtensionRegistryObserver::Waiter {
 public:
  explicit Waiter(const std::string& extension_id)
      : observed_(false), runner_(NULL) {}

  void Wait() {
    if (observed_)
      return;

    runner_ = new content::MessageLoopRunner();
    runner_->Run();
  }

  void OnObserved() {
    observed_ = true;

    if (runner_.get()) {
      runner_->Quit();
      runner_ = NULL;
    }
  }

 private:
  bool observed_;
  scoped_refptr<content::MessageLoopRunner> runner_;

  DISALLOW_COPY_AND_ASSIGN(Waiter);
};

TestExtensionRegistryObserver::TestExtensionRegistryObserver(
    ExtensionRegistry* registry,
    const std::string& extension_id)
    : will_be_installed_waiter_(new Waiter(extension_id)),
      uninstalled_waiter_(new Waiter(extension_id)),
      loaded_waiter_(new Waiter(extension_id)),
      unloaded_waiter_(new Waiter(extension_id)),
      extension_registry_observer_(this),
      extension_id_(extension_id) {
  extension_registry_observer_.Add(registry);
}

TestExtensionRegistryObserver::~TestExtensionRegistryObserver() {
}

void TestExtensionRegistryObserver::WaitForExtensionUninstalled() {
  uninstalled_waiter_->Wait();
}

void TestExtensionRegistryObserver::WaitForExtensionWillBeInstalled() {
  will_be_installed_waiter_->Wait();
}

void TestExtensionRegistryObserver::WaitForExtensionLoaded() {
  loaded_waiter_->Wait();
}

void TestExtensionRegistryObserver::WaitForExtensionUnloaded() {
  unloaded_waiter_->Wait();
}

void TestExtensionRegistryObserver::OnExtensionWillBeInstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    bool is_update,
    bool from_ephemeral,
    const std::string& old_name) {
  if (extension->id() == extension_id_)
    will_be_installed_waiter_->OnObserved();
}

void TestExtensionRegistryObserver::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    extensions::UninstallReason reason) {
  if (extension->id() == extension_id_)
    uninstalled_waiter_->OnObserved();
}

void TestExtensionRegistryObserver::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  if (extension->id() == extension_id_)
    loaded_waiter_->OnObserved();
}

void TestExtensionRegistryObserver::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionInfo::Reason reason) {
  if (extension->id() == extension_id_)
    unloaded_waiter_->OnObserved();
}

}  // namespace extensions
