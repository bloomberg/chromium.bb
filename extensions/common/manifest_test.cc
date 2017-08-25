// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_test.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/path_service.h"
#include "base/strings/pattern.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "extensions/common/extension_l10n_util.h"
#include "extensions/common/extension_paths.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {
namespace {

// |manifest_path| is an absolute path to a manifest file.
std::unique_ptr<base::DictionaryValue> LoadManifestFile(
    const base::FilePath& manifest_path,
    std::string* error) {
  base::FilePath extension_path = manifest_path.DirName();

  EXPECT_TRUE(base::PathExists(manifest_path)) <<
      "Couldn't find " << manifest_path.value();

  JSONFileValueDeserializer deserializer(manifest_path);
  std::unique_ptr<base::DictionaryValue> manifest =
      base::DictionaryValue::From(deserializer.Deserialize(NULL, error));

  // Most unit tests don't need localization, and they'll fail if we try to
  // localize them, since their manifests don't have a default_locale key.
  // Only localize manifests that indicate they want to be localized.
  // Calling LocalizeExtension at this point mirrors file_util::LoadExtension.
  if (manifest &&
      manifest_path.value().find(FILE_PATH_LITERAL("localized")) !=
      std::string::npos)
    extension_l10n_util::LocalizeExtension(extension_path, manifest.get(),
                                           error);

  return manifest;
}

}  // namespace

ManifestTest::ManifestTest()
    : enable_apps_(true) {
}

ManifestTest::~ManifestTest() {
}

// Helper class that simplifies creating methods that take either a filename
// to a manifest or the manifest itself.
ManifestTest::ManifestData::ManifestData(const char* name)
    : name_(name), manifest_(nullptr) {}

// This does not take ownership of |manifest|.
ManifestTest::ManifestData::ManifestData(base::DictionaryValue* manifest,
                                          const char* name)
    : name_(name), manifest_(manifest) {
  CHECK(manifest_) << "Manifest NULL";
}

ManifestTest::ManifestData::ManifestData(
    std::unique_ptr<base::DictionaryValue> manifest)
    : manifest_(manifest.get()), manifest_holder_(std::move(manifest)) {
  CHECK(manifest_) << "Manifest NULL";
}

ManifestTest::ManifestData::ManifestData(
    std::unique_ptr<base::DictionaryValue> manifest,
    const char* name)
    : name_(name),
      manifest_(manifest.get()),
      manifest_holder_(std::move(manifest)) {
  CHECK(manifest_) << "Manifest NULL";
}

ManifestTest::ManifestData::ManifestData(const ManifestData& m) {
  NOTREACHED();
}

ManifestTest::ManifestData::~ManifestData() {
}

base::DictionaryValue* ManifestTest::ManifestData::GetManifest(
    const base::FilePath& test_data_dir,
    std::string* error) const {
  if (manifest_)
    return manifest_;

  base::FilePath manifest_path = test_data_dir.AppendASCII(name_);
  manifest_holder_ = LoadManifestFile(manifest_path, error);
  manifest_ = manifest_holder_.get();
  return manifest_;
}

std::string ManifestTest::GetTestExtensionID() const {
  return std::string();
}

base::FilePath ManifestTest::GetTestDataDir() {
  base::FilePath path;
  PathService::Get(DIR_TEST_DATA, &path);
  return path.AppendASCII("manifest_tests");
}

std::unique_ptr<base::DictionaryValue> ManifestTest::LoadManifest(
    char const* manifest_name,
    std::string* error) {
  base::FilePath manifest_path = GetTestDataDir().AppendASCII(manifest_name);
  return LoadManifestFile(manifest_path, error);
}

scoped_refptr<Extension> ManifestTest::LoadExtension(
    const ManifestData& manifest,
    std::string* error,
    extensions::Manifest::Location location,
    int flags) {
  base::FilePath test_data_dir = GetTestDataDir();
  base::DictionaryValue* value = manifest.GetManifest(test_data_dir, error);
  if (!value)
    return NULL;
  return Extension::Create(test_data_dir.DirName(), location, *value, flags,
      GetTestExtensionID(), error);
}

scoped_refptr<Extension> ManifestTest::LoadAndExpectSuccess(
    const ManifestData& manifest,
    extensions::Manifest::Location location,
    int flags) {
  std::string error;
  scoped_refptr<Extension> extension =
      LoadExtension(manifest, &error, location, flags);
  EXPECT_TRUE(extension.get()) << manifest.name();
  EXPECT_EQ("", error) << manifest.name();
  return extension;
}

scoped_refptr<Extension> ManifestTest::LoadAndExpectSuccess(
    char const* manifest_name,
    extensions::Manifest::Location location,
    int flags) {
  return LoadAndExpectSuccess(ManifestData(manifest_name), location, flags);
}

scoped_refptr<Extension> ManifestTest::LoadAndExpectWarning(
    const ManifestData& manifest,
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

scoped_refptr<Extension> ManifestTest::LoadAndExpectWarning(
    char const* manifest_name,
    const std::string& expected_warning,
    extensions::Manifest::Location location,
    int flags) {
  return LoadAndExpectWarning(
      ManifestData(manifest_name), expected_warning, location, flags);
}

void ManifestTest::VerifyExpectedError(
    Extension* extension,
    const std::string& name,
    const std::string& error,
    const std::string& expected_error) {
  EXPECT_FALSE(extension) <<
      "Expected failure loading extension '" << name <<
      "', but didn't get one.";
  EXPECT_TRUE(base::MatchPattern(error, expected_error))
      << name << " expected '" << expected_error << "' but got '" << error
      << "'";
}

void ManifestTest::LoadAndExpectError(
    const ManifestData& manifest,
    const std::string& expected_error,
    extensions::Manifest::Location location,
    int flags) {
  std::string error;
  scoped_refptr<Extension> extension(
      LoadExtension(manifest, &error, location, flags));
  VerifyExpectedError(extension.get(), manifest.name(), error,
                      expected_error);
}

void ManifestTest::LoadAndExpectError(
    char const* manifest_name,
    const std::string& expected_error,
    extensions::Manifest::Location location,
    int flags) {
  return LoadAndExpectError(
      ManifestData(manifest_name), expected_error, location, flags);
}

void ManifestTest::AddPattern(extensions::URLPatternSet* extent,
                              const std::string& pattern) {
  int schemes = URLPattern::SCHEME_ALL;
  extent->AddPattern(URLPattern(schemes, pattern));
}

ManifestTest::Testcase::Testcase(const std::string& manifest_filename,
                                 const std::string& expected_error,
                                 extensions::Manifest::Location location,
                                 int flags)
    : manifest_filename_(manifest_filename),
      expected_error_(expected_error),
      location_(location),
      flags_(flags) {}

ManifestTest::Testcase::Testcase(const std::string& manifest_filename,
                                 const std::string& expected_error)
    : manifest_filename_(manifest_filename),
      expected_error_(expected_error),
      location_(extensions::Manifest::INTERNAL),
      flags_(Extension::NO_FLAGS) {}

ManifestTest::Testcase::Testcase(const std::string& manifest_filename)
    : manifest_filename_(manifest_filename),
      location_(extensions::Manifest::INTERNAL),
      flags_(Extension::NO_FLAGS) {}

ManifestTest::Testcase::Testcase(const std::string& manifest_filename,
                                 extensions::Manifest::Location location,
                                 int flags)
    : manifest_filename_(manifest_filename),
      location_(location),
      flags_(flags) {}

void ManifestTest::RunTestcases(const Testcase* testcases,
                                         size_t num_testcases,
                                         ExpectType type) {
  for (size_t i = 0; i < num_testcases; ++i)
    RunTestcase(testcases[i], type);
}

void ManifestTest::RunTestcase(const Testcase& testcase,
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

}  // namespace extensions
