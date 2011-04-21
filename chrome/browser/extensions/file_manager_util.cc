// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/extensions/file_manager_util.h"

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "third_party/libjingle/source/talk/base/urlencode.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_mount_point_provider.h"
#include "webkit/fileapi/file_system_util.h"

// This is the "well known" url for the file manager extension from
// browser/resources/file_manager.  In the future we may provide a way to swap
// out this file manager for an aftermarket part, but not yet.
const char kBaseFileBrowserUrl[] =
    "chrome-extension://hhaomjibdihmijegdhdafkllkbggdgoj/main.html";

// static
GURL FileManagerUtil::GetFileBrowserUrl() {
  return GURL(kBaseFileBrowserUrl);
}

// static
bool FileManagerUtil::ConvertFileToFileSystemUrl(
    Profile* profile, const FilePath& full_file_path, const GURL& origin_url,
    GURL* url) {
  fileapi::FileSystemPathManager* path_manager =
      profile->GetFileSystemContext()->path_manager();
  fileapi::ExternalFileSystemMountPointProvider* provider =
      path_manager->external_provider();
  if (!provider)
    return false;

  // Find if this file path is managed by the external provider.
  std::vector<FilePath> root_dirs = provider->GetRootDirectories();
  for (std::vector<FilePath>::iterator iter = root_dirs.begin();
       iter != root_dirs.end();
       ++iter) {
    FilePath path;
    const FilePath& root_path = *iter;
    if (root_path.AppendRelativePath(full_file_path, &path)) {
      GURL base_url = fileapi::GetFileSystemRootURI(origin_url,
          fileapi::kFileSystemTypeExternal);
      *url = GURL(base_url.spec() + root_path.Append(path).value().substr(1));
      return true;
    }
  }
  return false;
}

// static
GURL FileManagerUtil::GetFileBrowserUrlWithParams(
    SelectFileDialog::Type type,
    const string16& title,
    const FilePath& default_path,
    const SelectFileDialog::FileTypeInfo* file_types,
    int file_type_index,
    const FilePath::StringType& default_extension) {
  std::string json = GetArgumentsJson(type, title, default_path, file_types,
                                      file_type_index, default_extension);
  return GURL(FileManagerUtil::GetFileBrowserUrl().spec() + "?" +
              UrlEncodeStringWithoutEncodingSpaceAsPlus(json));

}
// static
void FileManagerUtil::ShowFullTabUrl(Profile* profile,
                                     const FilePath& default_path) {
  std::string json = GetArgumentsJson(SelectFileDialog::SELECT_NONE, string16(),
      default_path, NULL, 0, FilePath::StringType());
  GURL url(std::string(kBaseFileBrowserUrl) + "?" +
           UrlEncodeStringWithoutEncodingSpaceAsPlus(json));
  Browser* browser = BrowserList::GetLastActive();
  if (!browser)
    return;

  UserMetrics::RecordAction(UserMetricsAction("ShowFileBrowserFullTab"),
                            profile);
  browser->ShowSingletonTab(GURL(url));
}

// static
std::string FileManagerUtil::GetArgumentsJson(
    SelectFileDialog::Type type,
    const string16& title,
    const FilePath& default_path,
    const SelectFileDialog::FileTypeInfo* file_types,
    int file_type_index,
    const FilePath::StringType& default_extension) {
  DictionaryValue arg_value;
  arg_value.SetString("type", GetDialogTypeAsString(type));
  arg_value.SetString("title", title);
  // TODO(zelidrag): Convert local system path into virtual path for File API.
  arg_value.SetString("defaultPath", default_path.value());
  arg_value.SetString("defaultExtension", default_extension);

  ListValue* types_list = new ListValue();

  if (file_types) {
    for (size_t i = 0; i < file_types->extensions.size(); ++i) {
      ListValue* extensions_list = new ListValue();
      for (size_t j = 0; j < file_types->extensions[i].size(); ++j) {
        extensions_list->Set(
            i, Value::CreateStringValue(file_types->extensions[i][j]));
      }

      DictionaryValue* dict = new DictionaryValue();
      dict->Set("extensions", extensions_list);

      if (i < file_types->extension_description_overrides.size()) {
        string16 desc = file_types->extension_description_overrides[i];
        dict->SetString("description", desc);
      }

      dict->SetBoolean("selected",
                       (static_cast<size_t>(file_type_index) == i));

      types_list->Set(i, dict);
    }
  }

  std::string rv;
  base::JSONWriter::Write(&arg_value, false, &rv);

  return rv;
}

// static
std::string FileManagerUtil::GetDialogTypeAsString(
    SelectFileDialog::Type dialog_type) {
  std::string type_str;
  switch (dialog_type) {
    case SelectFileDialog::SELECT_NONE:
      type_str = "full-page";
      break;

    case SelectFileDialog::SELECT_FOLDER:
      type_str = "folder";
      break;

    case SelectFileDialog::SELECT_SAVEAS_FILE:
      type_str = "saveas-file";
      break;

    case SelectFileDialog::SELECT_OPEN_FILE:
      type_str = "open-file";
      break;

    case SelectFileDialog::SELECT_OPEN_MULTI_FILE:
      type_str = "open-multi-file";
      break;

    default:
      NOTREACHED();
  }

  return type_str;
}
