// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/stl_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_builder.h"
#include "chrome/common/extensions/manifest_handler.h"
#include "chrome/common/extensions/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class ManifestHandlerTest : public testing::Test {
 public:
  class ParsingWatcher {
   public:
    // Called when a manifest handler parses.
    void Record(const std::string& name) {
      parsed_names_.push_back(name);
    }

    const std::vector<std::string>& parsed_names() {
      return parsed_names_;
    }

    // Returns true if |name_before| was parsed before |name_after|.
    bool ParsedBefore(const std::string& name_before,
                      const std::string& name_after) {
      size_t i_before = parsed_names_.size();
      size_t i_after = 0;
      for (size_t i = 0; i < parsed_names_.size(); ++i) {
        if (parsed_names_[i] == name_before)
          i_before = i;
        if (parsed_names_[i] == name_after)
          i_after = i;
      }
      if (i_before < i_after)
        return true;
      return false;
    }

   private:
    // The order of manifest handlers that we watched parsing.
    std::vector<std::string> parsed_names_;
  };

  class TestManifestHandler : public ManifestHandler {
   public:
    TestManifestHandler(const std::string& name,
                        const std::vector<std::string>& prereqs,
                        ParsingWatcher* watcher)
        : name_(name), prereqs_(prereqs), watcher_(watcher) {
    }

    virtual bool Parse(Extension* extension, string16* error) OVERRIDE {
      watcher_->Record(name_);
      return true;
    }

    virtual const std::vector<std::string>& PrerequisiteKeys() OVERRIDE {
      return prereqs_;
    }

   protected:
    std::string name_;
    std::vector<std::string> prereqs_;
    ParsingWatcher* watcher_;
  };

  class FailingTestManifestHandler : public TestManifestHandler {
   public:
    FailingTestManifestHandler(const std::string& name,
                               const std::vector<std::string> prereqs,
                               ParsingWatcher* watcher)
        : TestManifestHandler(name, prereqs, watcher) {
    }
    virtual bool Parse(Extension* extension, string16* error) OVERRIDE {
      *error = ASCIIToUTF16(name_);
      return false;
    }
  };

  class AlwaysParseTestManifestHandler : public TestManifestHandler {
   public:
    AlwaysParseTestManifestHandler(const std::string& name,
                                   const std::vector<std::string> prereqs,
                                   ParsingWatcher* watcher)
        : TestManifestHandler(name, prereqs, watcher) {
    }

    virtual bool AlwaysParseForType(Manifest::Type type) OVERRIDE {
      return true;
    }
  };

 protected:
  virtual void TearDown() OVERRIDE {
    ManifestHandler::ClearRegistryForTesting();
  }
};

TEST_F(ManifestHandlerTest, DependentHandlers) {
  ParsingWatcher watcher;
  std::vector<std::string> prereqs;
  ManifestHandler::Register(
      "a", make_linked_ptr(new TestManifestHandler("A", prereqs, &watcher)));
  ManifestHandler::Register(
      "b", make_linked_ptr(new TestManifestHandler("B", prereqs, &watcher)));
  ManifestHandler::Register(
      "j", make_linked_ptr(new TestManifestHandler("J", prereqs, &watcher)));
  ManifestHandler::Register(
      "k", make_linked_ptr(
          new AlwaysParseTestManifestHandler("K", prereqs, &watcher)));
  prereqs.push_back("c.d");
  linked_ptr<TestManifestHandler> handler_c2(
      new TestManifestHandler("C.EZ", prereqs, &watcher));
  ManifestHandler::Register("c.e", handler_c2);
  ManifestHandler::Register("c.z", handler_c2);
  prereqs.clear();
  prereqs.push_back("b");
  prereqs.push_back("k");
  ManifestHandler::Register(
      "c.d", make_linked_ptr(new TestManifestHandler(
          "C.D", prereqs, &watcher)));

  scoped_refptr<Extension> extension = ExtensionBuilder()
      .SetManifest(DictionaryBuilder()
                   .Set("name", "no name")
                   .Set("version", "0")
                   .Set("manifest_version", 2)
                   .Set("a", 1)
                   .Set("b", 2)
                   .Set("c", DictionaryBuilder()
                        .Set("d", 3)
                        .Set("e", 4)
                        .Set("f", 5))
                   .Set("g", 6))
      .Build();

  // A, B, C.EZ, C.D, K
  EXPECT_EQ(5u, watcher.parsed_names().size());
  EXPECT_TRUE(watcher.ParsedBefore("B", "C.D"));
  EXPECT_TRUE(watcher.ParsedBefore("K", "C.D"));
  EXPECT_TRUE(watcher.ParsedBefore("C.D", "C.EZ"));
}

TEST_F(ManifestHandlerTest, FailingHandlers) {
  // Can't use ExtensionBuilder, because this extension will fail to
  // be parsed.
  scoped_ptr<base::DictionaryValue> manifest_a(
      DictionaryBuilder()
      .Set("name", "no name")
      .Set("version", "0")
      .Set("manifest_version", 2)
      .Set("a", 1)
      .Build());

  // Succeeds when "a" is not recognized.
  std::string error;
  scoped_refptr<Extension> extension = Extension::Create(
      base::FilePath(),
      Manifest::INVALID_LOCATION,
      *manifest_a,
      Extension::NO_FLAGS,
      &error);
  EXPECT_TRUE(extension);

  // Register a handler for "a" that fails.
  ParsingWatcher watcher;
  ManifestHandler::Register(
      "a", make_linked_ptr(new FailingTestManifestHandler(
          "A", std::vector<std::string>(), &watcher)));

  extension = Extension::Create(
      base::FilePath(),
      Manifest::INVALID_LOCATION,
      *manifest_a,
      Extension::NO_FLAGS,
      &error);
  EXPECT_FALSE(extension);
  EXPECT_EQ("A", error);
}

}  // namespace extensions
