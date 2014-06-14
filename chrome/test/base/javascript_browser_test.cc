// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/javascript_browser_test.h"

#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/resource/resource_bundle.h"

// static
const base::FilePath::CharType
    JavaScriptBrowserTest::kA11yAuditLibraryJSPath[] =
        FILE_PATH_LITERAL("third_party/accessibility-audit/axs_testing.js");

// static
const base::FilePath::CharType JavaScriptBrowserTest::kMockJSPath[] =
    FILE_PATH_LITERAL("chrome/third_party/mock4js/mock4js.js");

// static
const base::FilePath::CharType JavaScriptBrowserTest::kWebUILibraryJS[] =
    FILE_PATH_LITERAL("test_api.js");

// static
const base::FilePath::CharType JavaScriptBrowserTest::kWebUITestFolder[] =
    FILE_PATH_LITERAL("webui");

void JavaScriptBrowserTest::AddLibrary(const base::FilePath& library_path) {
  user_libraries_.push_back(library_path);
}

JavaScriptBrowserTest::JavaScriptBrowserTest() {
}

JavaScriptBrowserTest::~JavaScriptBrowserTest() {
}

void JavaScriptBrowserTest::SetUpOnMainThread() {
  base::FilePath test_data_directory;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_data_directory));
  test_data_directory = test_data_directory.Append(kWebUITestFolder);
  library_search_paths_.push_back(test_data_directory);

  base::FilePath gen_test_data_directory;
  ASSERT_TRUE(
      PathService::Get(chrome::DIR_GEN_TEST_DATA, &gen_test_data_directory));
  library_search_paths_.push_back(gen_test_data_directory);

  base::FilePath source_root_directory;
  ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &source_root_directory));
  library_search_paths_.push_back(source_root_directory);

  base::FilePath resources_pack_path;
  PathService::Get(chrome::FILE_RESOURCES_PACK, &resources_pack_path);
  ResourceBundle::GetSharedInstance().AddDataPackFromPath(
      resources_pack_path, ui::SCALE_FACTOR_NONE);

  AddLibrary(base::FilePath(kMockJSPath));
  AddLibrary(base::FilePath(kWebUILibraryJS));
}

// TODO(dtseng): Make this return bool (success/failure) and remove ASSERt_TRUE
// calls.
void JavaScriptBrowserTest::BuildJavascriptLibraries(
    std::vector<base::string16>* libraries) {
  ASSERT_TRUE(libraries != NULL);
  std::vector<base::FilePath>::iterator user_libraries_iterator;
  for (user_libraries_iterator = user_libraries_.begin();
       user_libraries_iterator != user_libraries_.end();
       ++user_libraries_iterator) {
    std::string library_content;
    if (user_libraries_iterator->IsAbsolute()) {
      ASSERT_TRUE(
          base::ReadFileToString(*user_libraries_iterator, &library_content))
          << user_libraries_iterator->value();
    } else {
      bool ok = false;
      std::vector<base::FilePath>::iterator library_search_path_iterator;
      for (library_search_path_iterator = library_search_paths_.begin();
           library_search_path_iterator != library_search_paths_.end();
           ++library_search_path_iterator) {
        ok = base::ReadFileToString(
            base::MakeAbsoluteFilePath(
                library_search_path_iterator->Append(*user_libraries_iterator)),
            &library_content);
        if (ok)
          break;
      }
      ASSERT_TRUE(ok) << "User library not found: "
                      << user_libraries_iterator->value();
    }
    library_content.append(";\n");

    // This magic code puts filenames in stack traces.
    library_content.append("//# sourceURL=");
    library_content.append(user_libraries_iterator->BaseName().AsUTF8Unsafe());
    library_content.append("\n");
    libraries->push_back(base::UTF8ToUTF16(library_content));
  }
}

base::string16 JavaScriptBrowserTest::BuildRunTestJSCall(
    bool is_async,
    const std::string& function_name,
    const ConstValueVector& test_func_args) {
  ConstValueVector arguments;
  base::FundamentalValue* is_async_arg = new base::FundamentalValue(is_async);
  arguments.push_back(is_async_arg);
  base::StringValue* function_name_arg = new base::StringValue(function_name);
  arguments.push_back(function_name_arg);
  base::ListValue* baked_argument_list = new base::ListValue();
  ConstValueVector::const_iterator arguments_iterator;
  for (arguments_iterator = test_func_args.begin();
       arguments_iterator != test_func_args.end();
       ++arguments_iterator) {
    baked_argument_list->Append((*arguments_iterator)->DeepCopy());
  }
  arguments.push_back(baked_argument_list);
  return content::WebUI::GetJavascriptCall(std::string("runTest"),
                                           arguments.get());
}
