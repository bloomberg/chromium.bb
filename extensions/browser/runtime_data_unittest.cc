// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/runtime_data.h"

#include <string>

#include "base/memory/ref_counted.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/test_util.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace {

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
  scoped_refptr<Extension> no_background = test_util::CreateEmptyExtension();
  EXPECT_TRUE(runtime_data_.IsBackgroundPageReady(no_background.get()));

  // An extension with a background page is not ready until the flag is set.
  scoped_refptr<Extension> with_background =
      CreateExtensionWithBackgroundPage();
  EXPECT_FALSE(runtime_data_.IsBackgroundPageReady(with_background.get()));

  // The flag can be toggled.
  runtime_data_.SetBackgroundPageReady(with_background.get(), true);
  EXPECT_TRUE(runtime_data_.IsBackgroundPageReady(with_background.get()));
  runtime_data_.SetBackgroundPageReady(with_background.get(), false);
  EXPECT_FALSE(runtime_data_.IsBackgroundPageReady(with_background.get()));
}

TEST_F(RuntimeDataTest, IsBeingUpgraded) {
  scoped_refptr<Extension> extension = test_util::CreateEmptyExtension();

  // An extension is not being upgraded until the flag is set.
  EXPECT_FALSE(runtime_data_.IsBeingUpgraded(extension.get()));

  // The flag can be toggled.
  runtime_data_.SetBeingUpgraded(extension.get(), true);
  EXPECT_TRUE(runtime_data_.IsBeingUpgraded(extension.get()));
  runtime_data_.SetBeingUpgraded(extension.get(), false);
  EXPECT_FALSE(runtime_data_.IsBeingUpgraded(extension.get()));
}

TEST_F(RuntimeDataTest, HasUsedWebRequest) {
  scoped_refptr<Extension> extension = test_util::CreateEmptyExtension();

  // An extension has not used web request until the flag is set.
  EXPECT_FALSE(runtime_data_.HasUsedWebRequest(extension.get()));

  // The flag can be toggled.
  runtime_data_.SetHasUsedWebRequest(extension.get(), true);
  EXPECT_TRUE(runtime_data_.HasUsedWebRequest(extension.get()));
  runtime_data_.SetHasUsedWebRequest(extension.get(), false);
  EXPECT_FALSE(runtime_data_.HasUsedWebRequest(extension.get()));
}

// Unloading an extension stops tracking it.
TEST_F(RuntimeDataTest, OnExtensionUnloaded) {
  scoped_refptr<Extension> extension = CreateExtensionWithBackgroundPage();
  runtime_data_.SetBackgroundPageReady(extension.get(), true);
  ASSERT_TRUE(runtime_data_.HasExtensionForTesting(extension.get()));

  runtime_data_.OnExtensionUnloaded(
      NULL, extension.get(), UnloadedExtensionInfo::REASON_DISABLE);
  EXPECT_FALSE(runtime_data_.HasExtensionForTesting(extension.get()));
}

}  // namespace
}  // namespace extensions
