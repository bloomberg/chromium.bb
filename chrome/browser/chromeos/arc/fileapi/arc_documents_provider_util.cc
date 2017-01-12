// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/fileapi/arc_documents_provider_util.h"

#include <vector>

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "net/base/escape.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "url/gurl.h"

namespace arc {

// This is based on net/base/escape.cc: net::(anonymous namespace)::Escape.
// TODO(nya): Consider consolidating this function with EscapeFileSystemId() in
// chrome/browser/chromeos/file_system_provider/mount_path_util.cc.
// This version differs from the other one in the point that dots are not always
// escaped because authorities often contain harmless dots.
std::string EscapePathComponent(const std::string& name) {
  std::string escaped;
  // Escape dots only when they forms a special file name.
  if (name == "." || name == "..") {
    base::ReplaceChars(name, ".", "%2E", &escaped);
    return escaped;
  }
  // Escape % and / only.
  for (size_t i = 0; i < name.size(); ++i) {
    const char c = name[i];
    if (c == '%' || c == '/')
      base::StringAppendF(&escaped, "%%%02X", c);
    else
      escaped.push_back(c);
  }
  return escaped;
}

std::string UnescapePathComponent(const std::string& escaped) {
  return net::UnescapeURLComponent(
      escaped, net::UnescapeRule::PATH_SEPARATORS |
                   net::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS);
}

const char kDocumentsProviderMountPointName[] = "arc-documents-provider";
const base::FilePath::CharType kDocumentsProviderMountPointPath[] =
    "/special/arc-documents-provider";
const char kAndroidDirectoryMimeType[] = "vnd.android.document/directory";

base::FilePath GetDocumentsProviderMountPath(
    const std::string& authority,
    const std::string& root_document_id) {
  return base::FilePath(kDocumentsProviderMountPointPath)
      .Append(EscapePathComponent(authority))
      .Append(EscapePathComponent(root_document_id));
}

bool ParseDocumentsProviderUrl(const storage::FileSystemURL& url,
                               std::string* authority,
                               std::string* root_document_id,
                               base::FilePath* path) {
  if (url.type() != storage::kFileSystemTypeArcDocumentsProvider)
    return false;
  base::FilePath url_path_stripped = url.path().StripTrailingSeparators();

  if (!base::FilePath(kDocumentsProviderMountPointPath)
           .IsParent(url_path_stripped)) {
    return false;
  }

  // Filesystem URL format for documents provider is:
  // /special/arc-documents-provider/<authority>/<root_doc_id>/<relative_path>
  std::vector<base::FilePath::StringType> components;
  url_path_stripped.GetComponents(&components);
  if (components.size() < 5)
    return false;

  *authority = UnescapePathComponent(components[3]);
  *root_document_id = UnescapePathComponent(components[4]);

  base::FilePath root_path =
      GetDocumentsProviderMountPath(*authority, *root_document_id);
  // Special case: AppendRelativePath() fails for identical paths.
  if (url_path_stripped == root_path) {
    path->clear();
  } else {
    bool success = root_path.AppendRelativePath(url_path_stripped, path);
    DCHECK(success);
  }
  return true;
}

GURL BuildDocumentUrl(const std::string& authority,
                      const std::string& document_id) {
  return GURL(base::StringPrintf(
      "content://%s/document/%s",
      net::EscapeQueryParamValue(authority, false /* use_plus */).c_str(),
      net::EscapeQueryParamValue(document_id, false /* use_plus */).c_str()));
}

}  // namespace arc
