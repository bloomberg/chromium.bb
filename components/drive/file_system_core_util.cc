// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/drive/file_system_core_util.h"

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/sequenced_worker_pool.h"
#include "components/drive/drive.pb.h"
#include "components/drive/drive_pref_names.h"
#include "components/drive/job_list.h"

namespace drive {
namespace util {

namespace {

std::string ReadStringFromGDocFile(const base::FilePath& file_path,
                                   const std::string& key) {
  const int64_t kMaxGDocSize = 4096;
  int64_t file_size = 0;
  if (!base::GetFileSize(file_path, &file_size) || file_size > kMaxGDocSize) {
    LOG(WARNING) << "File too large to be a GDoc file " << file_path.value();
    return std::string();
  }

  JSONFileValueDeserializer reader(file_path);
  std::string error_message;
  scoped_ptr<base::Value> root_value(reader.Deserialize(NULL, &error_message));
  if (!root_value) {
    LOG(WARNING) << "Failed to parse " << file_path.value() << " as JSON."
                 << " error = " << error_message;
    return std::string();
  }

  base::DictionaryValue* dictionary_value = NULL;
  std::string result;
  if (!root_value->GetAsDictionary(&dictionary_value) ||
      !dictionary_value->GetString(key, &result)) {
    LOG(WARNING) << "No value for the given key is stored in "
                 << file_path.value() << ". key = " << key;
    return std::string();
  }

  return result;
}

}  // namespace

const base::FilePath& GetDriveGrandRootPath() {
  CR_DEFINE_STATIC_LOCAL(
      base::FilePath, grand_root_path,
      (base::FilePath::FromUTF8Unsafe(kDriveGrandRootDirName)));
  return grand_root_path;
}

const base::FilePath& GetDriveMyDriveRootPath() {
  CR_DEFINE_STATIC_LOCAL(
      base::FilePath, drive_root_path,
      (GetDriveGrandRootPath().AppendASCII(kDriveMyDriveRootDirName)));
  return drive_root_path;
}

std::string EscapeCacheFileName(const std::string& filename) {
  // This is based on net/base/escape.cc: net::(anonymous namespace)::Escape
  std::string escaped;
  for (size_t i = 0; i < filename.size(); ++i) {
    char c = filename[i];
    if (c == '%' || c == '.' || c == '/') {
      base::StringAppendF(&escaped, "%%%02X", c);
    } else {
      escaped.push_back(c);
    }
  }
  return escaped;
}

std::string UnescapeCacheFileName(const std::string& filename) {
  std::string unescaped;
  for (size_t i = 0; i < filename.size(); ++i) {
    char c = filename[i];
    if (c == '%' && i + 2 < filename.length()) {
      c = (base::HexDigitToInt(filename[i + 1]) << 4) +
          base::HexDigitToInt(filename[i + 2]);
      i += 2;
    }
    unescaped.push_back(c);
  }
  return unescaped;
}

std::string NormalizeFileName(const std::string& input) {
  DCHECK(base::IsStringUTF8(input));

  std::string output;
  if (!base::ConvertToUtf8AndNormalize(input, base::kCodepageUTF8, &output))
    output = input;
  base::ReplaceChars(output, "/", "_", &output);
  if (!output.empty() && output.find_first_not_of('.', 0) == std::string::npos)
    output = "_";
  return output;
}

void EmptyFileOperationCallback(FileError error) {
}

bool CreateGDocFile(const base::FilePath& file_path,
                    const GURL& url,
                    const std::string& resource_id) {
  std::string content =
      base::StringPrintf("{\"url\": \"%s\", \"resource_id\": \"%s\"}",
                         url.spec().c_str(), resource_id.c_str());
  return base::WriteFile(file_path, content.data(), content.size()) ==
         static_cast<int>(content.size());
}

GURL ReadUrlFromGDocFile(const base::FilePath& file_path) {
  return GURL(ReadStringFromGDocFile(file_path, "url"));
}

std::string ReadResourceIdFromGDocFile(const base::FilePath& file_path) {
  return ReadStringFromGDocFile(file_path, "resource_id");
}

}  // namespace util
}  // namespace drive
