// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Utility class to access Chrome Extension manifest data.

#include "ceee/ie/common/extension_manifest.h"

#include "base/base64.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/third_party/nss/blapi.h"
#include "base/third_party/nss/sha256.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/url_constants.h"
#include "net/base/net_util.h"

namespace ext_keys = extension_manifest_keys;
namespace ext_values = extension_manifest_values;

const char ExtensionManifest::kManifestFilename[] = "manifest.json";

// First 16 bytes of SHA256 hashed public key.
const size_t ExtensionManifest::kIdSize = 16;

HRESULT ExtensionManifest::ReadManifestFile(const FilePath& extension_path,
                                            bool require_key) {
  // TODO(mad@chromium.org): Unbranch (taken from constructor of
  // Extension class).
  DCHECK(extension_path.IsAbsolute());
#if defined(OS_WIN)
  // Normalize any drive letter to upper-case. We do this for consistency with
  // net_utils::FilePathToFileURL(), which does the same thing, to make string
  // comparisons simpler.
  std::wstring path_str = extension_path.value();
  if (path_str.size() >= 2 && path_str[0] >= L'a' && path_str[0] <= L'z' &&
      path_str[1] == ':')
    path_str[0] += ('A' - 'a');

  path_ = FilePath(path_str);
#else
  path_ = path;
#endif

  // This piece comes from ExtensionsServiceBackend::LoadExtension()
  FilePath manifest_path =
      extension_path.AppendASCII(ExtensionManifest::kManifestFilename);
  std::string json_string;
  if (!file_util::ReadFileToString(manifest_path, &json_string)) {
    LOG(ERROR) << "Invalid extension path or manifest file: " <<
                  extension_path.value();
    return E_FAIL;
  }

  scoped_ptr<Value> value(base::JSONReader::Read(json_string, true));
  if (!value.get() || !value->IsType(Value::TYPE_DICTIONARY)) {
    LOG(ERROR) << "Invalid manifest file";
    return E_FAIL;
  }

  DictionaryValue* dict = static_cast<DictionaryValue*>(value.get());

  // And the rest is highly inspired from Extension::InitFromValue()

  // Initialize public key and id.
  if (dict->HasKey(ext_keys::kPublicKey)) {
    if (!dict->GetString(ext_keys::kPublicKey, &public_key_) ||
        FAILED(CalculateIdFromPublicKey())) {
      public_key_.clear();
      extension_id_.clear();
      LOG(ERROR) << "Invalid public key format";
      return E_FAIL;
    }
  } else if (require_key) {
    LOG(ERROR) << "Required key not found in manifest file";
    return E_FAIL;
  }

  // Initialize the URL.
  extension_url_ = GURL(std::string(chrome::kExtensionScheme) +
                        chrome::kStandardSchemeSeparator + extension_id_ + "/");

  // Initialize content scripts (optional).
  // MUST BE DONE AFTER setting up extension_id_, extension_url_ and path_
  if (dict->HasKey(ext_keys::kContentScripts)) {
    ListValue* list_value;
    if (!dict->GetList(ext_keys::kContentScripts, &list_value)) {
      LOG(ERROR) << "Invalid content script JSON value";
      return E_FAIL;
    }

    for (size_t i = 0; i < list_value->GetSize(); ++i) {
      DictionaryValue* content_script;
      if (!list_value->GetDictionary(i, &content_script)) {
        LOG(ERROR) << "Invalid content script JSON value";
        return E_FAIL;
      }

      UserScript script;
      HRESULT hr = LoadUserScriptHelper(content_script, &script);
      if (FAILED(hr))
        return hr;
      script.set_extension_id(extension_id_);
      content_scripts_.push_back(script);
    }
  }

  // Initialize toolstrips (optional).
  toolstrip_file_names_.clear();
  if (dict->HasKey(ext_keys::kToolstrips)) {
    ListValue* list_value;
    if (!dict->GetList(ext_keys::kToolstrips, &list_value)) {
      LOG(ERROR) << "Invalid toolstrip JSON value";
      return E_FAIL;
    }
    for (size_t i = 0; i < list_value->GetSize(); ++i) {
      std::string toolstrip_file_name;
      if (!list_value->GetString(i, &toolstrip_file_name)) {
        LOG(ERROR) << "Invalid toolstrip JSON value";
        return E_FAIL;
      }
      toolstrip_file_names_.push_back(toolstrip_file_name);
    }
  }
  // Add code here to read other manifest properties.
  return S_OK;
}

// TODO(mad@chromium.org): Unbranch (taken from Extension class).

// static
GURL ExtensionManifest::GetResourceUrl(const GURL& extension_url,
                                       const std::string& relative_path) {
  DCHECK(extension_url.SchemeIs(chrome::kExtensionScheme));
  DCHECK_EQ("/", extension_url.path());

  GURL ret_val = GURL(extension_url.spec() + relative_path);
  DCHECK(StartsWithASCII(ret_val.spec(), extension_url.spec(), false));

  return ret_val;
}

// TODO(mad@chromium.org): Reuse code in common\extensions\extension.cc
HRESULT ExtensionManifest::CalculateIdFromPublicKey() {
  std::string public_key_bytes;
  if (!base::Base64Decode(public_key_, &public_key_bytes))
    return E_FAIL;

  if (public_key_bytes.length() == 0)
    return E_FAIL;

  // SHA256 needs to work with an array of bytes, which we get from a string.
  const uint8* ubuf =
      reinterpret_cast<const unsigned char*>(public_key_bytes.data());
  SHA256Context ctx;
  SHA256_Begin(&ctx);
  SHA256_Update(&ctx, ubuf, public_key_bytes.length());
  // We must hash this value to a fixed size array.
  uint8 hash[kIdSize];
  SHA256_End(&ctx, hash, NULL, sizeof(hash));
  // To stay in sync with the code in Chrome, we start by converting the bytes
  // to a string representing the concatenation of the Hex values of all bytes.
  extension_id_ = base::HexEncode(hash, sizeof(hash));
  for (size_t i = 0; i < extension_id_.size(); ++i) {
    // And then, for each nibble represented by a single Hex digit, we use
    // the value to offset from the letter 'a' to construct the key in the
    // limited alphabet used for Chrome extension Ids ['a', 'q'].
    int val = -1;
    if (base::HexStringToInt(extension_id_.substr(i, 1), &val))
      extension_id_[i] = val + 'a';
    else
      extension_id_[i] = 'a';
  }
  return S_OK;
}


// TODO(mad@chromium.org): Unbranch (taken from the Extension class).
HRESULT ExtensionManifest::LoadUserScriptHelper(
    const DictionaryValue* content_script, UserScript* result) {
  // run_at
  if (content_script->HasKey(ext_keys::kRunAt)) {
    std::string run_location;
    if (!content_script->GetString(ext_keys::kRunAt, &run_location)) {
      LOG(ERROR) << "Invalid toolstrip JSON value";
      return E_FAIL;
    }

    if (run_location == ext_values::kRunAtDocumentStart) {
      result->set_run_location(UserScript::DOCUMENT_START);
    } else if (run_location == ext_values::kRunAtDocumentEnd) {
      result->set_run_location(UserScript::DOCUMENT_END);
    } else if (run_location == ext_values::kRunAtDocumentIdle) {
      result->set_run_location(UserScript::DOCUMENT_IDLE);
    } else {
      LOG(ERROR) << "Invalid toolstrip JSON value";
      return E_FAIL;
    }
  }

  // all frames
  if (content_script->HasKey(ext_keys::kAllFrames)) {
    bool all_frames = false;
    if (!content_script->GetBoolean(ext_keys::kAllFrames, &all_frames)) {
      LOG(ERROR) << "Invalid toolstrip JSON value";
      return E_FAIL;
    }
    result->set_match_all_frames(all_frames);
  }

  // matches
  ListValue* matches = NULL;
  if (!content_script->GetList(ext_keys::kMatches, &matches)) {
    LOG(ERROR) << "Invalid manifest without a matches value";
    return E_FAIL;
  }

  if (matches->GetSize() == 0) {
    LOG(ERROR) << "Invalid manifest without a matches value";
    return E_FAIL;
  }
  for (size_t i = 0; i < matches->GetSize(); ++i) {
    std::string match_str;
    if (!matches->GetString(i, &match_str)) {
      LOG(ERROR) << "Invalid matches JSON value";
      return E_FAIL;
    }

    URLPattern pattern(UserScript::kValidUserScriptSchemes);
    if (pattern.Parse(match_str) != URLPattern::PARSE_SUCCESS) {
      LOG(ERROR) << "Invalid matches value";
      return E_FAIL;
    }

    result->add_url_pattern(pattern);
  }

  // js and css keys
  ListValue* js = NULL;
  if (content_script->HasKey(ext_keys::kJs) &&
      !content_script->GetList(ext_keys::kJs, &js)) {
    LOG(ERROR) << "Invalid content script JS JSON value";
    return E_FAIL;
  }

  ListValue* css = NULL;
  if (content_script->HasKey(ext_keys::kCss) &&
      !content_script->GetList(ext_keys::kCss, &css)) {
    LOG(ERROR) << "Invalid content script CSS JSON value";
    return E_FAIL;
  }

  // The manifest needs to have at least one js or css user script definition.
  if (((js ? js->GetSize() : 0) + (css ? css->GetSize() : 0)) == 0) {
    LOG(ERROR) << "Invalid manifest without any content script";
    return E_FAIL;
  }

  if (js) {
    for (size_t script_index = 0; script_index < js->GetSize();
         ++script_index) {
      Value* value;
      std::wstring relative;
      if (!js->Get(script_index, &value) || !value->GetAsString(&relative)) {
        LOG(ERROR) << "Invalid content script JS JSON value";
        return E_FAIL;
      }
      // TODO(mad@chromium.org): Make GetResourceUrl accept wstring
      // too and check with georged@chromium.org who has the same todo
      // in chrome\common\extensions\extension.cc
      GURL url = GetResourceUrl(WideToUTF8(relative));
      result->js_scripts().push_back(
          UserScript::File(path(), FilePath(relative), url));
      // TODO(mad@chromium.org): Verify that the path refers to an
      // existing file.
    }
  }

  if (css) {
    for (size_t script_index = 0; script_index < css->GetSize();
         ++script_index) {
      Value* value;
      std::wstring relative;
      if (!css->Get(script_index, &value) || !value->GetAsString(&relative)) {
        LOG(ERROR) << "Invalid content script CSS JSON value";
        return E_FAIL;
      }
      // TODO(mad@chromium.org): Make GetResourceUrl accept wstring
      // too and check with georged@chromium.org who has the same todo
      // in chrome\common\extensions\extension.cc
      GURL url = GetResourceUrl(WideToUTF8(relative));
      result->css_scripts().push_back(
          UserScript::File(path(), FilePath(relative), url));
      // TODO(mad@chromium.org): Verify that the path refers to an
      // existing file.
    }
  }

  return S_OK;
}
