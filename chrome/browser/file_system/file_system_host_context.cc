// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/file_system/file_system_host_context.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/WebKit/chromium/public/WebCString.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFileSystem.h"
#include "third_party/WebKit/WebKit/chromium/public/WebSecurityOrigin.h"

const FilePath::CharType FileSystemHostContext::kFileSystemDirectory[] =
    FILE_PATH_LITERAL("FileSystem");

const char FileSystemHostContext::kPersistentName[] = "Persistent";
const char FileSystemHostContext::kTemporaryName[] = "Temporary";

FileSystemHostContext::FileSystemHostContext(
    const FilePath& data_path, bool is_incognito)
    : base_path_(data_path.Append(kFileSystemDirectory)),
      is_incognito_(is_incognito) {
}

bool FileSystemHostContext::GetFileSystemRootPath(
    const GURL& origin_url, fileapi::FileSystemType type,
    FilePath* root_path, std::string* name) const {
  // TODO(kinuko): should return an isolated temporary file system space.
  if (is_incognito_)
    return false;
  std::string storage_identifier = GetStorageIdentifierFromURL(origin_url);
  switch (type) {
    case fileapi::kFileSystemTypeTemporary:
      if (root_path)
        *root_path = base_path_.AppendASCII(storage_identifier)
                               .AppendASCII(kTemporaryName);
      if (name)
        *name = storage_identifier + ":" + kTemporaryName;
      return true;
    case fileapi::kFileSystemTypePersistent:
      if (root_path)
        *root_path = base_path_.AppendASCII(storage_identifier)
                               .AppendASCII(kPersistentName);
      if (name)
        *name = storage_identifier + ":" + kPersistentName;
      return true;
  }
  LOG(WARNING) << "Unknown filesystem type is requested:" << type;
  return false;
}

bool FileSystemHostContext::CheckValidFileSystemPath(
    const FilePath& path) const {
  // Any paths that includes parent references are considered invalid.
  return !path.ReferencesParent() && base_path_.IsParent(path);
}

std::string FileSystemHostContext::GetStorageIdentifierFromURL(
    const GURL& url) {
  WebKit::WebSecurityOrigin web_security_origin =
      WebKit::WebSecurityOrigin::createFromString(UTF8ToUTF16(url.spec()));
  return web_security_origin.databaseIdentifier().utf8();
}

COMPILE_ASSERT(int(WebKit::WebFileSystem::TypeTemporary) == \
               int(fileapi::kFileSystemTypeTemporary), mismatching_enums);
COMPILE_ASSERT(int(WebKit::WebFileSystem::TypePersistent) == \
               int(fileapi::kFileSystemTypePersistent), mismatching_enums);
