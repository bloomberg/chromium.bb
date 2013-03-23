// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/convert_user_script.h"

#include <string>
#include <vector>

#include "base/base64.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_file_value_serializer.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/user_script_master.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/user_script.h"
#include "crypto/sha2.h"
#include "extensions/common/constants.h"
#include "googleurl/src/gurl.h"

namespace keys = extension_manifest_keys;
namespace values = extension_manifest_values;

namespace extensions {

scoped_refptr<Extension> ConvertUserScriptToExtension(
    const base::FilePath& user_script_path, const GURL& original_url,
    const base::FilePath& extensions_dir, string16* error) {
  std::string content;
  if (!file_util::ReadFileToString(user_script_path, &content)) {
    *error = ASCIIToUTF16("Could not read source file.");
    return NULL;
  }

  if (!IsStringUTF8(content)) {
    *error = ASCIIToUTF16("User script must be UTF8 encoded.");
    return NULL;
  }

  UserScript script;
  if (!UserScriptMaster::ScriptReloader::ParseMetadataHeader(content,
                                                             &script)) {
    *error = ASCIIToUTF16("Invalid script header.");
    return NULL;
  }

  base::FilePath install_temp_dir =
      extension_file_util::GetInstallTempDir(extensions_dir);
  if (install_temp_dir.empty()) {
    *error = ASCIIToUTF16("Could not get path to profile temporary directory.");
    return NULL;
  }

  base::ScopedTempDir temp_dir;
  if (!temp_dir.CreateUniqueTempDirUnderPath(install_temp_dir)) {
    *error = ASCIIToUTF16("Could not create temporary directory.");
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
  char raw[crypto::kSHA256Length] = {0};
  std::string key;
  crypto::SHA256HashString(script_name, raw, crypto::kSHA256Length);
  base::Base64Encode(std::string(raw, crypto::kSHA256Length), &key);

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
  if (!script.url_patterns().is_empty()) {
    for (URLPatternSet::const_iterator i = script.url_patterns().begin();
         i != script.url_patterns().end(); ++i) {
      matches->Append(Value::CreateStringValue(i->GetAsString()));
    }
  } else {
    // TODO(aa): Derive tighter matches where possible.
    matches->Append(Value::CreateStringValue("http://*/*"));
    matches->Append(Value::CreateStringValue("https://*/*"));
  }

  // Read the exclude matches, if any are present.
  ListValue* exclude_matches = new ListValue();
  if (!script.exclude_url_patterns().is_empty()) {
    for (URLPatternSet::const_iterator i =
         script.exclude_url_patterns().begin();
         i != script.exclude_url_patterns().end(); ++i) {
      exclude_matches->Append(Value::CreateStringValue(i->GetAsString()));
    }
  }

  ListValue* includes = new ListValue();
  for (size_t i = 0; i < script.globs().size(); ++i)
    includes->Append(Value::CreateStringValue(script.globs().at(i)));

  ListValue* excludes = new ListValue();
  for (size_t i = 0; i < script.exclude_globs().size(); ++i)
    excludes->Append(Value::CreateStringValue(script.exclude_globs().at(i)));

  DictionaryValue* content_script = new DictionaryValue();
  content_script->Set(keys::kMatches, matches);
  content_script->Set(keys::kExcludeMatches, exclude_matches);
  content_script->Set(keys::kIncludeGlobs, includes);
  content_script->Set(keys::kExcludeGlobs, excludes);
  content_script->Set(keys::kJs, js_files);

  if (script.run_location() == UserScript::DOCUMENT_START)
    content_script->SetString(keys::kRunAt, values::kRunAtDocumentStart);
  else if (script.run_location() == UserScript::DOCUMENT_END)
    content_script->SetString(keys::kRunAt, values::kRunAtDocumentEnd);
  else if (script.run_location() == UserScript::DOCUMENT_IDLE)
    // This is the default, but store it just in case we change that.
    content_script->SetString(keys::kRunAt, values::kRunAtDocumentIdle);

  ListValue* content_scripts = new ListValue();
  content_scripts->Append(content_script);

  root->Set(keys::kContentScripts, content_scripts);

  base::FilePath manifest_path = temp_dir.path().Append(kManifestFilename);
  JSONFileValueSerializer serializer(manifest_path);
  if (!serializer.Serialize(*root)) {
    *error = ASCIIToUTF16("Could not write JSON.");
    return NULL;
  }

  // Write the script file.
  if (!file_util::CopyFile(user_script_path,
                           temp_dir.path().AppendASCII("script.js"))) {
    *error = ASCIIToUTF16("Could not copy script file.");
    return NULL;
  }

  // TODO(rdevlin.cronin): Continue removing std::string errors and replacing
  // with string16
  std::string utf8_error;
  scoped_refptr<Extension> extension = Extension::Create(
      temp_dir.path(),
      Manifest::INTERNAL,
      *root,
      Extension::NO_FLAGS,
      &utf8_error);
  *error = UTF8ToUTF16(utf8_error);
  if (!extension) {
    NOTREACHED() << "Could not init extension " << *error;
    return NULL;
  }

  temp_dir.Take();  // The caller takes ownership of the directory.
  return extension;
}

}  // namespace extensions
