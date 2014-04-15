// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_tests/extension_manifest_test.h"

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/path_service.h"
#include "base/values.h"
#include "chrome/common/chrome_paths.h"
#include "extensions/common/extension_l10n_util.h"
#include "extensions/common/test_util.h"
#include "ui/base/l10n/l10n_util.h"

using extensions::Extension;

namespace {

// If filename is a relative path, LoadManifestFile will treat it relative to
// the appropriate test directory.
base::DictionaryValue* LoadManifestFile(const base::FilePath& filename_path,
                                        std::string* error) {
  base::FilePath extension_path;
  base::FilePath manifest_path;

  PathService::Get(chrome::DIR_TEST_DATA, &manifest_path);
  manifest_path = manifest_path.Append(filename_path);
  extension_path = manifest_path.DirName();

  EXPECT_TRUE(base::PathExists(manifest_path)) <<
      "Couldn't find " << manifest_path.value();

  JSONFileValueSerializer serializer(manifest_path);
  base::DictionaryValue* manifest =
      static_cast<base::DictionaryValue*>(serializer.Deserialize(NULL, error));

  // Most unit tests don't need localization, and they'll fail if we try to
  // localize them, since their manifests don't have a default_locale key.
  // Only localize manifests that indicate they want to be localized.
  // Calling LocalizeExtension at this point mirrors file_util::LoadExtension.
  if (manifest &&
      filename_path.value().find(FILE_PATH_LITERAL("localized")) !=
      std::string::npos)
    extension_l10n_util::LocalizeExtension(extension_path, manifest, error);

  return manifest;
}

}  // namespace

ExtensionManifestTest::ExtensionManifestTest()
    : enable_apps_(true),
      // UNKNOWN == trunk.
      current_channel_(chrome::VersionInfo::CHANNEL_UNKNOWN) {}

// Helper class that simplifies creating methods that take either a filename
// to a manifest or the manifest itself.
ExtensionManifestTest::Manifest::Manifest(const char* name)
    : name_(name), manifest_(NULL) {
}

ExtensionManifestTest::Manifest::Manifest(base::DictionaryValue* manifest,
                                          const char* name)
    : name_(name), manifest_(manifest) {
  CHECK(manifest_) << "Manifest NULL";
}

ExtensionManifestTest::Manifest::Manifest(
    scoped_ptr<base::DictionaryValue> manifest)
    : manifest_(manifest.get()), manifest_holder_(manifest.Pass()) {
  CHECK(manifest_) << "Manifest NULL";
}

ExtensionManifestTest::Manifest::Manifest(const Manifest& m) {
  NOTREACHED();
}

ExtensionManifestTest::Manifest::~Manifest() {
}

base::DictionaryValue* ExtensionManifestTest::Manifest::GetManifest(
    char const* test_data_dir, std::string* error) const {
  if (manifest_)
    return manifest_;

  base::FilePath filename_path;
  filename_path = filename_path.AppendASCII("extensions")
      .AppendASCII(test_data_dir)
      .AppendASCII(name_);
  manifest_ = LoadManifestFile(filename_path, error);
  manifest_holder_.reset(manifest_);
  return manifest_;
}

char const* ExtensionManifestTest::test_data_dir() {
  return "manifest_tests";
}

scoped_ptr<base::DictionaryValue> ExtensionManifestTest::LoadManifest(
    char const* manifest_name, std::string* error) {
  base::FilePath filename_path;
  filename_path = filename_path.AppendASCII("extensions")
      .AppendASCII(test_data_dir())
      .AppendASCII(manifest_name);
  return make_scoped_ptr(LoadManifestFile(filename_path, error));
}

scoped_refptr<Extension> ExtensionManifestTest::LoadExtension(
    const Manifest& manifest,
    std::string* error,
    extensions::Manifest::Location location,
    int flags) {
  base::DictionaryValue* value = manifest.GetManifest(test_data_dir(), error);
  if (!value)
    return NULL;
  base::FilePath path;
  PathService::Get(chrome::DIR_TEST_DATA, &path);
  path = path.AppendASCII("extensions").AppendASCII(test_data_dir());
  return Extension::Create(path.DirName(), location, *value, flags, error);
}

scoped_refptr<Extension> ExtensionManifestTest::LoadAndExpectSuccess(
    const Manifest& manifest,
    extensions::Manifest::Location location,
    int flags) {
  std::string error;
  scoped_refptr<Extension> extension =
      LoadExtension(manifest, &error, location, flags);
  EXPECT_TRUE(extension.get()) << manifest.name();
  EXPECT_EQ("", error) << manifest.name();
  return extension;
}

scoped_refptr<Extension> ExtensionManifestTest::LoadAndExpectSuccess(
    char const* manifest_name,
    extensions::Manifest::Location location,
    int flags) {
  return LoadAndExpectSuccess(Manifest(manifest_name), location, flags);
}

scoped_refptr<Extension> ExtensionManifestTest::LoadFromStringAndExpectSuccess(
    char const* manifest_json) {
  return LoadAndExpectSuccess(
      Manifest(extensions::test_util::ParseJsonDictionaryWithSingleQuotes(
          manifest_json)));
}

scoped_refptr<Extension> ExtensionManifestTest::LoadAndExpectWarning(
    const Manifest& manifest,
    const std::string& expected_warning,
    extensions::Manifest::Location location,
    int flags) {
  std::string error;
  scoped_refptr<Extension> extension =
      LoadExtension(manifest, &error, location, flags);
  EXPECT_TRUE(extension.get()) << manifest.name();
  EXPECT_EQ("", error) << manifest.name();
  EXPECT_EQ(1u, extension->install_warnings().size());
  EXPECT_EQ(expected_warning, extension->install_warnings()[0].message);
  return extension;
}

scoped_refptr<Extension> ExtensionManifestTest::LoadAndExpectWarning(
    char const* manifest_name,
    const std::string& expected_warning,
    extensions::Manifest::Location location,
    int flags) {
  return LoadAndExpectWarning(
      Manifest(manifest_name), expected_warning, location, flags);
}

void ExtensionManifestTest::VerifyExpectedError(
    Extension* extension,
    const std::string& name,
    const std::string& error,
    const std::string& expected_error) {
  EXPECT_FALSE(extension) <<
      "Expected failure loading extension '" << name <<
      "', but didn't get one.";
  EXPECT_TRUE(MatchPattern(error, expected_error)) << name <<
      " expected '" << expected_error << "' but got '" << error << "'";
}

void ExtensionManifestTest::LoadAndExpectError(
    const Manifest& manifest,
    const std::string& expected_error,
    extensions::Manifest::Location location,
    int flags) {
  std::string error;
  scoped_refptr<Extension> extension(
      LoadExtension(manifest, &error, location, flags));
  VerifyExpectedError(extension.get(), manifest.name(), error,
                      expected_error);
}

void ExtensionManifestTest::LoadAndExpectError(
    char const* manifest_name,
    const std::string& expected_error,
    extensions::Manifest::Location location,
    int flags) {
  return LoadAndExpectError(
      Manifest(manifest_name), expected_error, location, flags);
}

void ExtensionManifestTest::LoadFromStringAndExpectError(
    char const* manifest_json,
    const std::string& expected_error) {
  return LoadAndExpectError(
      Manifest(extensions::test_util::ParseJsonDictionaryWithSingleQuotes(
          manifest_json)),
      expected_error);
}

void ExtensionManifestTest::AddPattern(extensions::URLPatternSet* extent,
                                       const std::string& pattern) {
  int schemes = URLPattern::SCHEME_ALL;
  extent->AddPattern(URLPattern(schemes, pattern));
}

ExtensionManifestTest::Testcase::Testcase(
    std::string manifest_filename,
    std::string expected_error,
    extensions::Manifest::Location location,
    int flags)
    : manifest_filename_(manifest_filename),
      expected_error_(expected_error),
      location_(location), flags_(flags) {
}

ExtensionManifestTest::Testcase::Testcase(std::string manifest_filename,
                                          std::string expected_error)
    : manifest_filename_(manifest_filename),
      expected_error_(expected_error),
      location_(extensions::Manifest::INTERNAL),
      flags_(Extension::NO_FLAGS) {
}

ExtensionManifestTest::Testcase::Testcase(std::string manifest_filename)
    : manifest_filename_(manifest_filename),
      location_(extensions::Manifest::INTERNAL),
      flags_(Extension::NO_FLAGS) {}

ExtensionManifestTest::Testcase::Testcase(
    std::string manifest_filename,
    extensions::Manifest::Location location,
    int flags)
    : manifest_filename_(manifest_filename),
      location_(location),
      flags_(flags) {}

void ExtensionManifestTest::RunTestcases(const Testcase* testcases,
                                         size_t num_testcases,
                                         ExpectType type) {
  for (size_t i = 0; i < num_testcases; ++i)
    RunTestcase(testcases[i], type);
}

void ExtensionManifestTest::RunTestcase(const Testcase& testcase,
                                        ExpectType type) {
  switch (type) {
    case EXPECT_TYPE_ERROR:
      LoadAndExpectError(testcase.manifest_filename_.c_str(),
                         testcase.expected_error_,
                         testcase.location_,
                         testcase.flags_);
      break;
    case EXPECT_TYPE_WARNING:
      LoadAndExpectWarning(testcase.manifest_filename_.c_str(),
                           testcase.expected_error_,
                           testcase.location_,
                           testcase.flags_);
      break;
    case EXPECT_TYPE_SUCCESS:
      LoadAndExpectSuccess(testcase.manifest_filename_.c_str(),
                           testcase.location_,
                           testcase.flags_);
      break;
   }
}
