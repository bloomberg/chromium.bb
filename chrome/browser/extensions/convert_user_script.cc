// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/convert_user_script.h"

#include <string>
#include <vector>

#include "base/base64.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "crypto/sha2.h"
#include "chrome/browser/extensions/user_script_master.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "chrome/common/extensions/user_script.h"
#include "content/common/json_value_serializer.h"
#include "googleurl/src/gurl.h"

namespace keys = extension_manifest_keys;

scoped_refptr<Extension> ConvertUserScriptToExtension(
    const FilePath& user_script_path, const GURL& original_url,
    std::string* error) {
  std::string content;
  if (!file_util::ReadFileToString(user_script_path, &content)) {
    *error = "Could not read source file.";
    return NULL;
  }

  if (!IsStringUTF8(content)) {
    *error = "User script must be UTF8 encoded.";
    return NULL;
  }

  UserScript script;
  if (!UserScriptMaster::ScriptReloader::ParseMetadataHeader(content,
                                                             &script)) {
    *error = "Invalid script header.";
    return NULL;
  }

  FilePath user_data_temp_dir = extension_file_util::GetUserDataTempDir();
  if (user_data_temp_dir.empty()) {
    *error = "Could not get path to profile temporary directory.";
    return NULL;
  }

  ScopedTempDir temp_dir;
  if (!temp_dir.CreateUniqueTempDirUnderPath(user_data_temp_dir)) {
    *error = "Could not create temporary directory.";
    return NULL;
  }

  // Create the manifest
  scoped_ptr<DictionaryValue> root(new DictionaryValue);
  std::string script_name;
  if (!script.name().empty() && !script.name_space().empty())
    script_name = script.name_space() + "/" + script.name();
  else
    script_name = original_url.spec();

  // Create the public key.
  // User scripts are not signed, but the public key for an extension doubles as
  // its unique identity, and we need one of those. A user script's unique
  // identity is its namespace+name, so we hash that to create a public key.
  // There will be no corresponding private key, which means user scripts cannot
  // be auto-updated, or claimed in the gallery.
  char raw[crypto::SHA256_LENGTH] = {0};
  std::string key;
  crypto::SHA256HashString(script_name, raw, crypto::SHA256_LENGTH);
  base::Base64Encode(std::string(raw, crypto::SHA256_LENGTH), &key);

  // The script may not have a name field, but we need one for an extension. If
  // it is missing, use the filename of the original URL.
  if (!script.name().empty())
    root->SetString(keys::kName, script.name());
  else
    root->SetString(keys::kName, original_url.ExtractFileName());

  // Not all scripts have a version, but we need one. Default to 1.0 if it is
  // missing.
  if (!script.version().empty())
    root->SetString(keys::kVersion, script.version());
  else
    root->SetString(keys::kVersion, "1.0");

  root->SetString(keys::kDescription, script.description());
  root->SetString(keys::kPublicKey, key);
  root->SetBoolean(keys::kConvertedFromUserScript, true);

  ListValue* js_files = new ListValue();
  js_files->Append(Value::CreateStringValue("script.js"));

  // If the script provides its own match patterns, we use those. Otherwise, we
  // generate some using the include globs.
  ListValue* matches = new ListValue();
  if (!script.url_patterns().empty()) {
    for (size_t i = 0; i < script.url_patterns().size(); ++i) {
      matches->Append(Value::CreateStringValue(
          script.url_patterns()[i].GetAsString()));
    }
  } else {
    // TODO(aa): Derive tighter matches where possible.
    matches->Append(Value::CreateStringValue("http://*/*"));
    matches->Append(Value::CreateStringValue("https://*/*"));
  }

  ListValue* includes = new ListValue();
  for (size_t i = 0; i < script.globs().size(); ++i)
    includes->Append(Value::CreateStringValue(script.globs().at(i)));

  ListValue* excludes = new ListValue();
  for (size_t i = 0; i < script.exclude_globs().size(); ++i)
    excludes->Append(Value::CreateStringValue(script.exclude_globs().at(i)));

  DictionaryValue* content_script = new DictionaryValue();
  content_script->Set(keys::kMatches, matches);
  content_script->Set(keys::kIncludeGlobs, includes);
  content_script->Set(keys::kExcludeGlobs, excludes);
  content_script->Set(keys::kJs, js_files);

  ListValue* content_scripts = new ListValue();
  content_scripts->Append(content_script);

  root->Set(keys::kContentScripts, content_scripts);

  FilePath manifest_path = temp_dir.path().Append(
      Extension::kManifestFilename);
  JSONFileValueSerializer serializer(manifest_path);
  if (!serializer.Serialize(*root)) {
    *error = "Could not write JSON.";
    return NULL;
  }

  // Write the script file.
  if (!file_util::CopyFile(user_script_path,
                           temp_dir.path().AppendASCII("script.js"))) {
    *error = "Could not copy script file.";
    return NULL;
  }

  scoped_refptr<Extension> extension = Extension::Create(
      temp_dir.path(),
      Extension::INTERNAL,
      *root,
      Extension::NO_FLAGS,
      error);
  if (!extension) {
    NOTREACHED() << "Could not init extension " << *error;
    return NULL;
  }

  temp_dir.Take();  // The caller takes ownership of the directory.
  return extension;
}
