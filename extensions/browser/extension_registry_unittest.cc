// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_registry.h"

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace {

typedef testing::Test ExtensionRegistryTest;

testing::AssertionResult HasSingleExtension(
    const ExtensionList& list,
    const scoped_refptr<const Extension>& extension) {
  if (list.empty())
    return testing::AssertionFailure() << "No extensions in list";
  if (list.size() > 1) {
    return testing::AssertionFailure() << list.size()
                                       << " extensions, expected 1";
  }
  const Extension* did_load = list[0].get();
  if (did_load != extension.get()) {
    return testing::AssertionFailure() << "Expected " << extension->id()
                                       << " found " << did_load->id();
  }
  return testing::AssertionSuccess();
}

class TestObserver : public ExtensionRegistryObserver {
 public:
  TestObserver() {}

  void Reset() {
    loaded_.clear();
    unloaded_.clear();
    installed_.clear();
    uninstalled_.clear();
  }

  const ExtensionList& loaded() { return loaded_; }
  const ExtensionList& unloaded() { return unloaded_; }
  const ExtensionList& installed() { return installed_; }
  const ExtensionList& uninstalled() { return uninstalled_; }

 private:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override {
    loaded_.push_back(extension);
  }

  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override {
    unloaded_.push_back(extension);
  }

  void OnExtensionWillBeInstalled(content::BrowserContext* browser_context,
                                  const Extension* extension,
                                  bool is_update,
                                  const std::string& old_name) override {
    installed_.push_back(extension);
  }

  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const Extension* extension,
                              extensions::UninstallReason reason) override {
    uninstalled_.push_back(extension);
  }

  void OnShutdown(extensions::ExtensionRegistry* registry) override { Reset(); }

  ExtensionList loaded_;
  ExtensionList unloaded_;
  ExtensionList installed_;
  ExtensionList uninstalled_;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

TEST_F(ExtensionRegistryTest, FillAndClearRegistry) {
  ExtensionRegistry registry(NULL);
  scoped_refptr<Extension> extension1 = test_util::CreateEmptyExtension("id1");
  scoped_refptr<Extension> extension2 = test_util::CreateEmptyExtension("id2");
  scoped_refptr<Extension> extension3 = test_util::CreateEmptyExtension("id3");
  scoped_refptr<Extension> extension4 = test_util::CreateEmptyExtension("id4");

  // All the sets start empty.
  EXPECT_EQ(0u, registry.enabled_extensions().size());
  EXPECT_EQ(0u, registry.disabled_extensions().size());
  EXPECT_EQ(0u, registry.terminated_extensions().size());
  EXPECT_EQ(0u, registry.blacklisted_extensions().size());

  // Extensions can be added to each set.
  registry.AddEnabled(extension1);
  registry.AddDisabled(extension2);
  registry.AddTerminated(extension3);
  registry.AddBlacklisted(extension4);

  EXPECT_EQ(1u, registry.enabled_extensions().size());
  EXPECT_EQ(1u, registry.disabled_extensions().size());
  EXPECT_EQ(1u, registry.terminated_extensions().size());
  EXPECT_EQ(1u, registry.blacklisted_extensions().size());

  // Clearing the registry clears all sets.
  registry.ClearAll();

  EXPECT_EQ(0u, registry.enabled_extensions().size());
  EXPECT_EQ(0u, registry.disabled_extensions().size());
  EXPECT_EQ(0u, registry.terminated_extensions().size());
  EXPECT_EQ(0u, registry.blacklisted_extensions().size());
}

// A simple test of adding and removing things from sets.
TEST_F(ExtensionRegistryTest, AddAndRemoveExtensionFromRegistry) {
  ExtensionRegistry registry(NULL);

  // Adding an extension works.
  scoped_refptr<Extension> extension = test_util::CreateEmptyExtension("id");
  EXPECT_TRUE(registry.AddEnabled(extension));
  EXPECT_EQ(1u, registry.enabled_extensions().size());

  // The extension was only added to one set.
  EXPECT_EQ(0u, registry.disabled_extensions().size());
  EXPECT_EQ(0u, registry.terminated_extensions().size());
  EXPECT_EQ(0u, registry.blacklisted_extensions().size());

  // Removing an extension works.
  EXPECT_TRUE(registry.RemoveEnabled(extension->id()));
  EXPECT_EQ(0u, registry.enabled_extensions().size());

  // Trying to remove an extension that isn't in the set fails cleanly.
  EXPECT_FALSE(registry.RemoveEnabled(extension->id()));
}

TEST_F(ExtensionRegistryTest, AddExtensionToRegistryTwice) {
  ExtensionRegistry registry(NULL);
  scoped_refptr<Extension> extension = test_util::CreateEmptyExtension("id");

  // An extension can exist in two sets at once. It would be nice to eliminate
  // this functionality, but some users of ExtensionRegistry need it.
  EXPECT_TRUE(registry.AddEnabled(extension));
  EXPECT_TRUE(registry.AddDisabled(extension));

  EXPECT_EQ(1u, registry.enabled_extensions().size());
  EXPECT_EQ(1u, registry.disabled_extensions().size());
  EXPECT_EQ(0u, registry.terminated_extensions().size());
  EXPECT_EQ(0u, registry.blacklisted_extensions().size());
}

TEST_F(ExtensionRegistryTest, GetExtensionById) {
  ExtensionRegistry registry(NULL);

  // Trying to get an extension fails cleanly when the sets are empty.
  EXPECT_FALSE(
      registry.GetExtensionById("id", ExtensionRegistry::EVERYTHING));

  scoped_refptr<Extension> enabled = test_util::CreateEmptyExtension("enabled");
  scoped_refptr<Extension> disabled =
      test_util::CreateEmptyExtension("disabled");
  scoped_refptr<Extension> terminated =
      test_util::CreateEmptyExtension("terminated");
  scoped_refptr<Extension> blacklisted =
      test_util::CreateEmptyExtension("blacklisted");

  // Add an extension to each set.
  registry.AddEnabled(enabled);
  registry.AddDisabled(disabled);
  registry.AddTerminated(terminated);
  registry.AddBlacklisted(blacklisted);

  // Enabled is part of everything and the enabled list.
  EXPECT_TRUE(
      registry.GetExtensionById("enabled", ExtensionRegistry::EVERYTHING));
  EXPECT_TRUE(
      registry.GetExtensionById("enabled", ExtensionRegistry::ENABLED));
  EXPECT_FALSE(
      registry.GetExtensionById("enabled", ExtensionRegistry::DISABLED));
  EXPECT_FALSE(
      registry.GetExtensionById("enabled", ExtensionRegistry::TERMINATED));
  EXPECT_FALSE(
      registry.GetExtensionById("enabled", ExtensionRegistry::BLACKLISTED));

  // Disabled is part of everything and the disabled list.
  EXPECT_TRUE(
      registry.GetExtensionById("disabled", ExtensionRegistry::EVERYTHING));
  EXPECT_FALSE(
      registry.GetExtensionById("disabled", ExtensionRegistry::ENABLED));
  EXPECT_TRUE(
      registry.GetExtensionById("disabled", ExtensionRegistry::DISABLED));
  EXPECT_FALSE(
      registry.GetExtensionById("disabled", ExtensionRegistry::TERMINATED));
  EXPECT_FALSE(
      registry.GetExtensionById("disabled", ExtensionRegistry::BLACKLISTED));

  // Terminated is part of everything and the terminated list.
  EXPECT_TRUE(
      registry.GetExtensionById("terminated", ExtensionRegistry::EVERYTHING));
  EXPECT_FALSE(
      registry.GetExtensionById("terminated", ExtensionRegistry::ENABLED));
  EXPECT_FALSE(
      registry.GetExtensionById("terminated", ExtensionRegistry::DISABLED));
  EXPECT_TRUE(
      registry.GetExtensionById("terminated", ExtensionRegistry::TERMINATED));
  EXPECT_FALSE(
      registry.GetExtensionById("terminated", ExtensionRegistry::BLACKLISTED));

  // Blacklisted is part of everything and the blacklisted list.
  EXPECT_TRUE(
      registry.GetExtensionById("blacklisted", ExtensionRegistry::EVERYTHING));
  EXPECT_FALSE(
      registry.GetExtensionById("blacklisted", ExtensionRegistry::ENABLED));
  EXPECT_FALSE(
      registry.GetExtensionById("blacklisted", ExtensionRegistry::DISABLED));
  EXPECT_FALSE(
      registry.GetExtensionById("blacklisted", ExtensionRegistry::TERMINATED));
  EXPECT_TRUE(
      registry.GetExtensionById("blacklisted", ExtensionRegistry::BLACKLISTED));

  // Enabled can be found with multiple flags set.
  EXPECT_TRUE(registry.GetExtensionById(
      "enabled", ExtensionRegistry::ENABLED | ExtensionRegistry::TERMINATED));

  // Enabled isn't found if the wrong flags are set.
  EXPECT_FALSE(registry.GetExtensionById(
      "enabled", ExtensionRegistry::DISABLED | ExtensionRegistry::BLACKLISTED));
}

TEST_F(ExtensionRegistryTest, Observer) {
  ExtensionRegistry registry(NULL);
  TestObserver observer;
  registry.AddObserver(&observer);

  EXPECT_TRUE(observer.loaded().empty());
  EXPECT_TRUE(observer.unloaded().empty());
  EXPECT_TRUE(observer.installed().empty());

  scoped_refptr<const Extension> extension =
      test_util::CreateEmptyExtension("id");

  registry.TriggerOnWillBeInstalled(extension.get(), false, std::string());
  EXPECT_TRUE(HasSingleExtension(observer.installed(), extension.get()));

  registry.AddEnabled(extension);
  registry.TriggerOnLoaded(extension.get());

  registry.TriggerOnWillBeInstalled(extension.get(), true, "foo");

  EXPECT_TRUE(HasSingleExtension(observer.loaded(), extension.get()));
  EXPECT_TRUE(observer.unloaded().empty());
  registry.Shutdown();

  registry.RemoveEnabled(extension->id());
  registry.TriggerOnUnloaded(extension.get(), UnloadedExtensionReason::DISABLE);

  EXPECT_TRUE(observer.loaded().empty());
  EXPECT_TRUE(HasSingleExtension(observer.unloaded(), extension.get()));
  registry.Shutdown();

  registry.TriggerOnUninstalled(extension.get(), UNINSTALL_REASON_FOR_TESTING);
  EXPECT_TRUE(observer.installed().empty());
  EXPECT_TRUE(HasSingleExtension(observer.uninstalled(), extension.get()));

  registry.RemoveObserver(&observer);
}

}  // namespace
}  // namespace extensions
