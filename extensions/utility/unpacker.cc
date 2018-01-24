// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/utility/unpacker.h"

#include <stddef.h>

#include <algorithm>
#include <tuple>
#include <utility>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_file_value_serializer.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "extensions/common/api/declarative_net_request/dnr_manifest_data.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_l10n_util.h"
#include "extensions/common/extension_utility_types.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "net/base/file_stream.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/size.h"

namespace extensions {

namespace {

namespace errors = manifest_errors;
namespace keys = manifest_keys;

constexpr const base::FilePath::CharType* kAllowedThemeFiletypes[] = {
    FILE_PATH_LITERAL(".bmp"),  FILE_PATH_LITERAL(".gif"),
    FILE_PATH_LITERAL(".jpeg"), FILE_PATH_LITERAL(".jpg"),
    FILE_PATH_LITERAL(".json"), FILE_PATH_LITERAL(".png"),
    FILE_PATH_LITERAL(".webp")};

}  // namespace

Unpacker::Unpacker(const base::FilePath& working_dir,
                   const base::FilePath& extension_dir,
                   const std::string& extension_id,
                   Manifest::Location location,
                   int creation_flags)
    : working_dir_(working_dir),
      extension_dir_(extension_dir),
      extension_id_(extension_id),
      location_(location),
      creation_flags_(creation_flags) {}

Unpacker::~Unpacker() {
}

// static
bool Unpacker::ShouldExtractFile(bool is_theme,
                                 const base::FilePath& file_path) {
  if (is_theme) {
    const base::FilePath::StringType extension =
        base::ToLowerASCII(file_path.FinalExtension());
    // Allow filenames with no extension.
    if (extension.empty())
      return true;
    return std::find(kAllowedThemeFiletypes,
                     kAllowedThemeFiletypes + arraysize(kAllowedThemeFiletypes),
                     extension) !=
           (kAllowedThemeFiletypes + arraysize(kAllowedThemeFiletypes));
  }
  return !base::FilePath::CompareEqualIgnoreCase(file_path.FinalExtension(),
                                                 FILE_PATH_LITERAL(".exe"));
}

// static
bool Unpacker::IsManifestFile(const base::FilePath& file_path) {
  CHECK(!file_path.IsAbsolute());
  return base::FilePath::CompareEqualIgnoreCase(file_path.value(),
                                                kManifestFilename);
}

// static
std::unique_ptr<base::DictionaryValue> Unpacker::ReadManifest(
    const base::FilePath& extension_dir,
    std::string* error) {
  DCHECK(error);
  base::FilePath manifest_path = extension_dir.Append(kManifestFilename);
  if (!base::PathExists(manifest_path)) {
    *error = errors::kInvalidManifest;
    return nullptr;
  }

  JSONFileValueDeserializer deserializer(manifest_path);
  std::unique_ptr<base::Value> root = deserializer.Deserialize(NULL, error);
  if (!root.get()) {
    return nullptr;
  }

  if (!root->is_dict()) {
    *error = errors::kInvalidManifest;
    return nullptr;
  }

  return base::DictionaryValue::From(std::move(root));
}

bool Unpacker::Run() {
  // Parse the manifest.
  std::string error;
  parsed_manifest_ = ReadManifest(extension_dir_, &error);
  if (!parsed_manifest_.get()) {
    SetError(error);
    return false;
  }

  scoped_refptr<Extension> extension(
      Extension::Create(extension_dir_, location_, *parsed_manifest_,
                        creation_flags_, extension_id_, &error));
  if (!extension.get()) {
    SetError(error);
    return false;
  }

  std::vector<InstallWarning> warnings;
  if (!file_util::ValidateExtension(extension.get(), &error, &warnings)) {
    SetError(error);
    return false;
  }
  extension->AddInstallWarnings(warnings);

  return ReadJSONRulesetIfNeeded(extension.get());
}

bool Unpacker::ReadJSONRulesetIfNeeded(const Extension* extension) {
  const ExtensionResource* resource =
      declarative_net_request::DNRManifestData::GetRulesetResource(extension);
  // The extension did not provide a ruleset.
  if (!resource)
    return true;

  std::string error;
  JSONFileValueDeserializer deserializer(resource->GetFilePath());
  std::unique_ptr<base::Value> root = deserializer.Deserialize(nullptr, &error);
  if (!root) {
    SetError(error);
    return false;
  }

  if (!root->is_list()) {
    SetError(errors::kDeclarativeNetRequestListNotPassed);
    return false;
  }

  parsed_json_ruleset_ = base::ListValue::From(std::move(root));
  return true;
}

void Unpacker::SetError(const std::string& error) {
  SetUTF16Error(base::UTF8ToUTF16(error));
}

void Unpacker::SetUTF16Error(const base::string16& error) {
  error_message_ = error;
}

}  // namespace extensions
