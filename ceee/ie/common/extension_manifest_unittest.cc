// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for ExtensionManifest.

#include <atlconv.h>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/user_script.h"
#include "ceee/ie/common/extension_manifest.h"
#include "ceee/testing/utils/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ext_keys = extension_manifest_keys;
namespace ext_values = extension_manifest_values;

namespace {

// Static constants
static const char* kToolstripName1 = "MyToolstrip.html";
static const char* kToolstripName2 = "YourToolstrip.html";
static const char* kPublicKeyName = "0123456789ABCDEF0123456789ABCDEF";
static const char* kUrlPattern1 = "http://madlymad.com/*";
static const char* kUrlPattern2 = "https://superdave.com/*";
static const char* kCssPath = "CssPath.css";
static const wchar_t* kCssWPath = L"CssPath.css";
static const char* kJsPath1 = "JsPath1.js";
static const wchar_t* kJsWPath1 = L"JsPath1.js";
static const char* kJsPath2 = "JsPath2.js";
static const wchar_t* kJsWPath2 = L"JsPath2.js";
// This Id value has been computed with a valid version of the code.
// If the algorithm changes, we must update this value.
static const char* kComputedId = "fppgjfcenabdcibneonnejnkdafgjcch";

static const char* kExtensionUrl =
    "chrome-extension://fppgjfcenabdcibneonnejnkdafgjcch/";

// Test fixture to handle the stuff common to all tests.
class ExtensionManifestTest : public testing::Test {
 protected:
  ExtensionManifestTest()
      // We need to remember whether we created the file or not so that we
      // can delete it in TearDown because some tests don't create a file.
      : created_file_(false) {
  }

  // We always use the same temporary file name, so we can make it static.
  static void SetUpTestCase() {
    EXPECT_TRUE(file_util::GetTempDir(&file_dir_));
    file_path_ = file_dir_.AppendASCII(ExtensionManifest::kManifestFilename);
  }

  // This is the common code to write the Json values to the manifest file.
  void WriteJsonToFile(const Value& value) {
    std::string json_string;
    base::JSONWriter::Write(&value, false, &json_string);

    FILE* temp_file = file_util::OpenFile(file_path_, "w");
    EXPECT_TRUE(temp_file != NULL);
    created_file_ = true;

    fwrite(json_string.c_str(), json_string.size(), 1, temp_file);
    file_util::CloseFile(temp_file);
    temp_file = NULL;
  }

  // We must delete the file on each tests that created one.
  virtual void TearDown() {
    if (created_file_) {
      EXPECT_TRUE(file_util::Delete(file_path_, false));
    }
  }

 protected:
  static FilePath file_dir_;
  // We keep both the file path and the dir only path to avoid reconstructing it
  // all the time.
  static FilePath file_path_;

 private:
  bool created_file_;
};

FilePath ExtensionManifestTest::file_dir_;
FilePath ExtensionManifestTest::file_path_;


TEST_F(ExtensionManifestTest, InvalidFileName) {
  testing::LogDisabler no_dchecks;

  // This test assumes that there is no manifest file in the current path.
  base::PlatformFileInfo dummy;
  EXPECT_FALSE(file_util::GetFileInfo(FilePath(L"manifest.json"), &dummy));

  ExtensionManifest manifest;
  EXPECT_HRESULT_FAILED(manifest.ReadManifestFile(FilePath(), false));
  EXPECT_HRESULT_FAILED(manifest.ReadManifestFile(
      FilePath(L"LalaLandBoobooGaloo"), false));
}

TEST_F(ExtensionManifestTest, EmptyFile) {
  testing::LogDisabler no_dchecks;

  // Value's constructor is protected, so we must go dynamic.
  scoped_ptr<Value> value(Value::CreateNullValue());
  WriteJsonToFile(*value);

  ExtensionManifest manifest;
  EXPECT_HRESULT_FAILED(manifest.ReadManifestFile(file_dir_, false));
}

TEST_F(ExtensionManifestTest, InvalidJsonFile) {
  testing::LogDisabler no_dchecks;

  ListValue dummy_list;
  dummy_list.Append(Value::CreateIntegerValue(42));

  WriteJsonToFile(dummy_list);

  ExtensionManifest manifest;
  EXPECT_HRESULT_FAILED(manifest.ReadManifestFile(file_dir_, false));
}

TEST_F(ExtensionManifestTest, InvalidPublicKey) {
  testing::LogDisabler no_dchecks;

  DictionaryValue values;
  values.Set(ext_keys::kPublicKey,
             Value::CreateStringValue("Babebibobu"));

  WriteJsonToFile(values);

  ExtensionManifest manifest;
  EXPECT_HRESULT_FAILED(manifest.ReadManifestFile(file_dir_, false));
  EXPECT_TRUE(manifest.public_key().empty());
}

TEST_F(ExtensionManifestTest, InvalidUserScript) {
  testing::LogDisabler no_dchecks;

  DictionaryValue values;
  ListValue* scripts = new ListValue();
  values.Set(ext_keys::kContentScripts, scripts);
  DictionaryValue* script_dict = new DictionaryValue;
  scripts->Append(script_dict);

  // Empty scripts are not allowed.
  WriteJsonToFile(values);
  ExtensionManifest manifest;
  EXPECT_HRESULT_FAILED(manifest.ReadManifestFile(file_dir_, false));

  ListValue* matches1 = new ListValue();
  script_dict->Set(ext_keys::kMatches, matches1);

  // Matches must have at least one value.
  WriteJsonToFile(values);
  EXPECT_HRESULT_FAILED(manifest.ReadManifestFile(file_dir_, false));

  matches1->Append(Value::CreateStringValue(kUrlPattern1));

  // Having a match isn't enough without at least one CSS or JS file.
  WriteJsonToFile(values);
  EXPECT_HRESULT_FAILED(manifest.ReadManifestFile(file_dir_, false));

  ListValue* css = new ListValue();
  script_dict->Set(ext_keys::kCss, css);

  // CSS list must have at least one item.
  WriteJsonToFile(values);
  EXPECT_HRESULT_FAILED(manifest.ReadManifestFile(file_dir_, false));

  script_dict->Remove(ext_keys::kCss, NULL);
  ListValue* js = new ListValue();
  script_dict->Set(ext_keys::kJs, js);

  // Same thing for JS.
  WriteJsonToFile(values);
  EXPECT_HRESULT_FAILED(manifest.ReadManifestFile(file_dir_, false));
}

TEST_F(ExtensionManifestTest, EmptyValidJsonFile) {
  testing::LogDisabler no_dchecks;

  WriteJsonToFile(DictionaryValue());

  ExtensionManifest manifest;
  EXPECT_HRESULT_SUCCEEDED(manifest.ReadManifestFile(file_dir_, false));
  EXPECT_TRUE(manifest.GetToolstripFileNames().empty());
  EXPECT_STREQ(manifest.path().value().c_str(), file_dir_.value().c_str());
}

TEST_F(ExtensionManifestTest, ValidJsonFileWithOneValue) {
  testing::LogDisabler no_dchecks;

  DictionaryValue values;
  values.SetString("name", "My Name");

  // We must dynamically allocate since DictionaryValue will free this memory.
  ListValue * toolstrips = new ListValue();
  toolstrips->Append(Value::CreateStringValue(kToolstripName1));
  values.Set(ext_keys::kToolstrips, toolstrips);
  toolstrips = NULL;

  WriteJsonToFile(values);

  ExtensionManifest manifest;
  EXPECT_HRESULT_SUCCEEDED(manifest.ReadManifestFile(file_dir_, false));
  EXPECT_FALSE(manifest.GetToolstripFileNames().empty());
  EXPECT_STREQ(manifest.GetToolstripFileNames()[0].c_str(), kToolstripName1);
  EXPECT_TRUE(manifest.public_key().empty());
  EXPECT_TRUE(manifest.extension_id().empty());
}

TEST_F(ExtensionManifestTest, ValidJsonFileWithManyValues) {
  testing::LogDisabler no_dchecks;

  DictionaryValue values;
  values.SetString("name", "My Name");
  values.SetString("job", "Your Job");
  values.SetString(ext_keys::kPublicKey, kPublicKeyName);

  ListValue* toolstrips = new ListValue();
  toolstrips->Append(Value::CreateStringValue(kToolstripName1));
  toolstrips->Append(Value::CreateStringValue(kToolstripName2));
  values.Set(ext_keys::kToolstrips, toolstrips);
  toolstrips = NULL;

  ListValue* scripts = new ListValue();
  values.Set(ext_keys::kContentScripts, scripts);
  DictionaryValue* script_dict1 = new DictionaryValue;
  scripts->Append(script_dict1);
  script_dict1->SetString(ext_keys::kRunAt,
                          ext_values::kRunAtDocumentStart);

  ListValue* matches1 = new ListValue();
  script_dict1->Set(ext_keys::kMatches, matches1);
  matches1->Append(Value::CreateStringValue(kUrlPattern1));

  ListValue* css = new ListValue();
  script_dict1->Set(ext_keys::kCss, css);
  css->Append(Value::CreateStringValue(kCssPath));

  DictionaryValue* script_dict2 = new DictionaryValue;
  scripts->Append(script_dict2);

  ListValue* matches2 = new ListValue();
  script_dict2->Set(ext_keys::kMatches, matches2);
  matches2->Append(Value::CreateStringValue(kUrlPattern1));
  matches2->Append(Value::CreateStringValue(kUrlPattern2));

  ListValue* js = new ListValue();
  script_dict2->Set(ext_keys::kJs, js);
  js->Append(Value::CreateStringValue(kJsPath1));
  js->Append(Value::CreateStringValue(kJsPath2));

  WriteJsonToFile(values);

  ExtensionManifest manifest;
  EXPECT_HRESULT_SUCCEEDED(manifest.ReadManifestFile(file_dir_, true));
  EXPECT_FALSE(manifest.GetToolstripFileNames().empty());
  // Prevent asserts blocking the tests if the test above failed.
  if (manifest.GetToolstripFileNames().size() > 1) {
    EXPECT_STREQ(manifest.GetToolstripFileNames()[0].c_str(), kToolstripName1);
    EXPECT_STREQ(manifest.GetToolstripFileNames()[1].c_str(), kToolstripName2);
  }
  EXPECT_STREQ(manifest.public_key().c_str(), kPublicKeyName);
  EXPECT_STREQ(manifest.extension_id().c_str(), kComputedId);
  EXPECT_STREQ(manifest.path().value().c_str(), file_dir_.value().c_str());
  EXPECT_EQ(manifest.extension_url(), GURL(kExtensionUrl));

  URLPattern url_pattern1(UserScript::kValidUserScriptSchemes);
  url_pattern1.Parse(kUrlPattern1);
  URLPattern url_pattern2(UserScript::kValidUserScriptSchemes);
  url_pattern2.Parse(kUrlPattern2);

  const UserScriptList& script_list = manifest.content_scripts();
  EXPECT_EQ(script_list.size(), 2);
  if (script_list.size() == 2) {
    const UserScript& script1 = script_list[0];
    EXPECT_EQ(script1.run_location(),
              UserScript::DOCUMENT_START);
    const std::vector<URLPattern>& url_patterns1 = script1.url_patterns();
    EXPECT_EQ(url_patterns1.size(), 1);
    if (url_patterns1.size() == 1)
      EXPECT_EQ(url_patterns1[0].GetAsString(), url_pattern1.GetAsString());
    const UserScript::FileList& css_scripts = script1.css_scripts();
    EXPECT_EQ(css_scripts.size(), 1);
    if (css_scripts.size() == 1) {
      EXPECT_STREQ(css_scripts[0].extension_root().value().c_str(),
                   file_dir_.value().c_str());
      EXPECT_STREQ(css_scripts[0].relative_path().value().c_str(), kCssWPath);
    }
    const UserScript& script2 = script_list[1];
    const std::vector<URLPattern>& url_patterns2 = script2.url_patterns();
    EXPECT_EQ(url_patterns2.size(), 2);
    if (url_patterns2.size() == 2) {
      EXPECT_EQ(url_patterns2[0].GetAsString(), url_pattern1.GetAsString());
      EXPECT_EQ(url_patterns2[1].GetAsString(), url_pattern2.GetAsString());
    }
    const UserScript::FileList& js_scripts = script2.js_scripts();
    EXPECT_EQ(js_scripts.size(), 2);
    if (js_scripts.size() == 2) {
      EXPECT_STREQ(js_scripts[0].extension_root().value().c_str(),
                   file_dir_.value().c_str());
      EXPECT_STREQ(js_scripts[0].relative_path().value().c_str(), kJsWPath1);
      EXPECT_STREQ(js_scripts[1].extension_root().value().c_str(),
                   file_dir_.value().c_str());
      EXPECT_STREQ(js_scripts[1].relative_path().value().c_str(), kJsWPath2);
    }
  }
}

}  // namespace
