// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/runtime_data.h"

#include <string>

#include "base/memory/ref_counted.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace {

// Creates a very simple extension.
scoped_refptr<Extension> CreateExtension() {
  return ExtensionBuilder()
      .SetManifest(
           DictionaryBuilder().Set("name", "test").Set("version", "0.1"))
      .SetID("id1")
      .Build();
}

// Creates a very simple extension with a background page.
scoped_refptr<Extension> CreateExtensionWithBackgroundPage() {
  return ExtensionBuilder()
      .SetManifest(
           DictionaryBuilder()
             .Set("name", "test")
             .Set("version", "0.1")
             .Set("background", DictionaryBuilder().Set("page", "bg.html")))
      .SetID("id2")
      .Build();
}

class RuntimeDataTest : public testing::Test {
 public:
  RuntimeDataTest() : registry_(NULL), runtime_data_(&registry_) {}
  virtual ~RuntimeDataTest() {}

 protected:
  ExtensionRegistry registry_;
  RuntimeData runtime_data_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RuntimeDataTest);
};

TEST_F(RuntimeDataTest, IsBackgroundPageReady) {
  // An extension without a background page is always considered ready.
  scoped_refptr<Extension> no_background = CreateExtension();
  EXPECT_TRUE(runtime_data_.IsBackgroundPageReady(no_background));

  // An extension with a background page is not ready until the flag is set.
  scoped_refptr<Extension> with_background =
      CreateExtensionWithBackgroundPage();
  EXPECT_FALSE(runtime_data_.IsBackgroundPageReady(with_background));

  // The flag can be toggled.
  runtime_data_.SetBackgroundPageReady(with_background, true);
  EXPECT_TRUE(runtime_data_.IsBackgroundPageReady(with_background));
  runtime_data_.SetBackgroundPageReady(with_background, false);
  EXPECT_FALSE(runtime_data_.IsBackgroundPageReady(with_background));
}

TEST_F(RuntimeDataTest, IsBeingUpgraded) {
  scoped_refptr<Extension> extension = CreateExtension();

  // An extension is not being upgraded until the flag is set.
  EXPECT_FALSE(runtime_data_.IsBeingUpgraded(extension));

  // The flag can be toggled.
  runtime_data_.SetBeingUpgraded(extension, true);
  EXPECT_TRUE(runtime_data_.IsBeingUpgraded(extension));
  runtime_data_.SetBeingUpgraded(extension, false);
  EXPECT_FALSE(runtime_data_.IsBeingUpgraded(extension));
}

TEST_F(RuntimeDataTest, HasUsedWebRequest) {
  scoped_refptr<Extension> extension = CreateExtension();

  // An extension has not used web request until the flag is set.
  EXPECT_FALSE(runtime_data_.HasUsedWebRequest(extension));

  // The flag can be toggled.
  runtime_data_.SetHasUsedWebRequest(extension, true);
  EXPECT_TRUE(runtime_data_.HasUsedWebRequest(extension));
  runtime_data_.SetHasUsedWebRequest(extension, false);
  EXPECT_FALSE(runtime_data_.HasUsedWebRequest(extension));
}

// Unloading an extension stops tracking it.
TEST_F(RuntimeDataTest, OnExtensionUnloaded) {
  scoped_refptr<Extension> extension = CreateExtensionWithBackgroundPage();
  runtime_data_.SetBackgroundPageReady(extension, true);
  ASSERT_TRUE(runtime_data_.HasExtensionForTesting(extension));

  runtime_data_.OnExtensionUnloaded(
      NULL, extension, UnloadedExtensionInfo::REASON_DISABLE);
  EXPECT_FALSE(runtime_data_.HasExtensionForTesting(extension));
}

}  // namespace
}  // namespace extensions
