// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/url_util.h"

#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "net/base/escape.h"

namespace file_manager {
namespace util {
namespace {

// Returns a file manager URL for the given |path|.
GURL GetFileManagerUrl(const char* path) {
  return GURL(std::string("chrome-extension://") + kFileManagerAppId + path);
}

// Converts a numeric dialog type to a string.
std::string GetDialogTypeAsString(
    ui::SelectFileDialog::Type dialog_type) {
  std::string type_str;
  switch (dialog_type) {
    case ui::SelectFileDialog::SELECT_NONE:
      type_str = "full-page";
      break;

    case ui::SelectFileDialog::SELECT_FOLDER:
      type_str = "folder";
      break;

    case ui::SelectFileDialog::SELECT_UPLOAD_FOLDER:
      type_str = "upload-folder";
      break;

    case ui::SelectFileDialog::SELECT_SAVEAS_FILE:
      type_str = "saveas-file";
      break;

    case ui::SelectFileDialog::SELECT_OPEN_FILE:
      type_str = "open-file";
      break;

    case ui::SelectFileDialog::SELECT_OPEN_MULTI_FILE:
      type_str = "open-multi-file";
      break;

    default:
      NOTREACHED();
  }

  return type_str;
}

}  // namespace

GURL GetFileManagerBaseUrl() {
  return GetFileManagerUrl("/");
}

GURL GetFileManagerMainPageUrl() {
  return GetFileManagerUrl("/main.html");
}

GURL GetFileManagerMainPageUrlWithParams(
    ui::SelectFileDialog::Type type,
    const base::string16& title,
    const GURL& current_directory_url,
    const GURL& selection_url,
    const std::string& target_name,
    const ui::SelectFileDialog::FileTypeInfo* file_types,
    int file_type_index,
    const base::FilePath::StringType& default_extension) {
  base::DictionaryValue arg_value;
  arg_value.SetString("type", GetDialogTypeAsString(type));
  arg_value.SetString("title", title);
  arg_value.SetString("currentDirectoryURL", current_directory_url.spec());
  arg_value.SetString("selectionURL", selection_url.spec());
  arg_value.SetString("targetName", target_name);
  arg_value.SetString("defaultExtension", default_extension);

  if (file_types) {
    base::ListValue* types_list = new base::ListValue();
    for (size_t i = 0; i < file_types->extensions.size(); ++i) {
      base::ListValue* extensions_list = new base::ListValue();
      for (size_t j = 0; j < file_types->extensions[i].size(); ++j) {
        extensions_list->Append(
            new base::StringValue(file_types->extensions[i][j]));
      }

      base::DictionaryValue* dict = new base::DictionaryValue();
      dict->Set("extensions", extensions_list);

      if (i < file_types->extension_description_overrides.size()) {
        base::string16 desc = file_types->extension_description_overrides[i];
        dict->SetString("description", desc);
      }

      // file_type_index is 1-based. 0 means no selection at all.
      dict->SetBoolean("selected",
                       (static_cast<size_t>(file_type_index) == (i + 1)));

      types_list->Set(i, dict);
    }
    arg_value.Set("typeList", types_list);

    arg_value.SetBoolean("includeAllFiles", file_types->include_all_files);
  }

  // If the caller cannot handle Drive path, the file chooser dialog need to
  // return resolved local native paths to the selected files.
  arg_value.SetBoolean("shouldReturnLocalPath",
                       !file_types || !file_types->support_drive);

  std::string json_args;
  base::JSONWriter::Write(&arg_value, &json_args);

  std::string url = GetFileManagerMainPageUrl().spec() + '?' +
      net::EscapeUrlEncodedData(json_args,
                                false);  // Space to %20 instead of +.
  return GURL(url);
}

}  // namespace util
}  // namespace file_manager
