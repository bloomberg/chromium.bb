// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_MANIFEST_TESTS_EXTENSION_MANIFEST_TEST_H_
#define CHROME_COMMON_EXTENSIONS_MANIFEST_TESTS_EXTENSION_MANIFEST_TEST_H_

#include "base/values.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/features/feature.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace errors = extension_manifest_errors;
namespace keys = extension_manifest_keys;

class ExtensionManifestTest : public testing::Test {
 public:
  ExtensionManifestTest();

 protected:
  // If filename is a relative path, LoadManifestFile will treat it relative to
  // the appropriate test directory.
  static DictionaryValue* LoadManifestFile(const std::string& filename,
                                           std::string* error);

  // Helper class that simplifies creating methods that take either a filename
  // to a manifest or the manifest itself.
  class Manifest {
   public:
    explicit Manifest(const char* name);
    Manifest(DictionaryValue* manifest, const char* name);
    // C++98 requires the copy constructor for a type to be visible if you
    // take a const-ref of a temporary for that type.  Since Manifest
    // contains a scoped_ptr, its implicit copy constructor is declared
    // Manifest(Manifest&) according to spec 12.8.5.  This breaks the first
    // requirement and thus you cannot use it with LoadAndExpectError() or
    // LoadAndExpectSuccess() easily.
    //
    // To get around this spec pedantry, we declare the copy constructor
    // explicitly.  It will never get invoked.
    Manifest(const Manifest& m);

    ~Manifest();

    const std::string& name() const { return name_; };

    DictionaryValue* GetManifest(std::string* error) const;

   private:
    const std::string name_;
    mutable DictionaryValue* manifest_;
    mutable scoped_ptr<DictionaryValue> manifest_holder_;
  };

  scoped_refptr<extensions::Extension> LoadExtension(
      const Manifest& manifest,
      std::string* error,
      extensions::Extension::Location location =
          extensions::Extension::INTERNAL,
      int flags = extensions::Extension::NO_FLAGS);

  scoped_refptr<extensions::Extension> LoadAndExpectSuccess(
      const Manifest& manifest,
      extensions::Extension::Location location =
          extensions::Extension::INTERNAL,
      int flags = extensions::Extension::NO_FLAGS);

  scoped_refptr<extensions::Extension> LoadAndExpectSuccess(
      char const* manifest_name,
      extensions::Extension::Location location =
          extensions::Extension::INTERNAL,
      int flags = extensions::Extension::NO_FLAGS);

  scoped_refptr<extensions::Extension> LoadAndExpectWarning(
      const Manifest& manifest,
      const std::string& expected_error,
      extensions::Extension::Location location =
          extensions::Extension::INTERNAL,
      int flags = extensions::Extension::NO_FLAGS);

  scoped_refptr<extensions::Extension> LoadAndExpectWarning(
      char const* manifest_name,
      const std::string& expected_error,
      extensions::Extension::Location location =
          extensions::Extension::INTERNAL,
      int flags = extensions::Extension::NO_FLAGS);

  void VerifyExpectedError(extensions::Extension* extension,
                           const std::string& name,
                           const std::string& error,
                           const std::string& expected_error);

  void LoadAndExpectError(char const* manifest_name,
                          const std::string& expected_error,
                          extensions::Extension::Location location =
                              extensions::Extension::INTERNAL,
                          int flags = extensions::Extension::NO_FLAGS);

  void LoadAndExpectError(const Manifest& manifest,
                          const std::string& expected_error,
                          extensions::Extension::Location location =
                              extensions::Extension::INTERNAL,
                          int flags = extensions::Extension::NO_FLAGS);

  void AddPattern(URLPatternSet* extent, const std::string& pattern);

  // used to differentiate between calls to LoadAndExpectError,
  // LoadAndExpectWarning and LoadAndExpectSuccess via function RunTestcases.
  enum EXPECT_TYPE {
    EXPECT_TYPE_ERROR,
    EXPECT_TYPE_WARNING,
    EXPECT_TYPE_SUCCESS
  };

  struct Testcase {
    std::string manifest_filename_;
    std::string expected_error_; // only used for ExpectedError tests
    extensions::Extension::Location location_;
    int flags_;

    Testcase(std::string manifest_filename, std::string expected_error,
        extensions::Extension::Location location, int flags);

    Testcase(std::string manifest_filename, std::string expected_error);

    explicit Testcase(std::string manifest_filename);

    Testcase(std::string manifest_filename,
             extensions::Extension::Location location,
             int flags);
  };

  void RunTestcases(const Testcase* testcases, size_t num_testcases,
      EXPECT_TYPE type);

  bool enable_apps_;

  // Force the manifest tests to run as though they are on trunk, since several
  // tests rely on manifest features being available that aren't on
  // stable/beta.
  //
  // These objects nest, so if a test wants to explicitly test the behaviour
  // on stable or beta, declare it inside that test.
  extensions::Feature::ScopedCurrentChannel current_channel_;
};

#endif  // CHROME_COMMON_EXTENSIONS_MANIFEST_TESTS_EXTENSION_MANIFEST_TEST_H_
