// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/fileapi/arc_documents_provider_util.h"

#include <algorithm>
#include <utility>

#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "net/base/escape.h"
#include "net/base/mime_util.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "url/gurl.h"

namespace arc {

namespace {

struct MimeTypeToExtensions {
  const char* mime_type;
  const char* extensions;
};

// The mapping from MIME types to file name extensions, taken from Android N.
// See: frameworks/base/media/java/android/media/MediaFile.java
constexpr MimeTypeToExtensions kAndroidMimeTypeMappings[] = {
    {"application/mspowerpoint", "ppt"},
    {"application/msword", "doc"},
    {"application/ogg", "ogg,oga"},
    {"application/pdf", "pdf"},
    {"application/vnd.apple.mpegurl", "m3u8"},
    {"application/vnd.ms-excel", "xls"},
    {"application/vnd.ms-wpl", "wpl"},
    {"application/x-android-drm-fl", "fl"},
    {"application/x-mpegurl", "m3u"},
    {"application/zip", "zip"},
    {"audio/aac", "aac"},
    {"audio/aac-adts", "aac"},
    {"audio/amr", "amr"},
    {"audio/amr-wb", "awb"},
    {"audio/flac", "flac"},
    {"audio/imelody", "imy"},
    {"audio/midi", "mid,midi,xmf,rtttl,rtx,ota,mxmf"},
    {"audio/mp4", "m4a"},
    {"audio/mpeg", "mp3,mpga"},
    {"audio/mpegurl", "m3u8"},
    {"audio/ogg", "ogg"},
    {"audio/sp-midi", "smf"},
    {"audio/x-matroska", "mka"},
    {"audio/x-mpegurl", "m3u,m3u8"},
    {"audio/x-ms-wma", "wma"},
    {"audio/x-scpls", "pls"},
    {"audio/x-wav", "wav"},
    {"image/gif", "gif"},
    {"image/jpeg", "jpg,jpeg"},
    {"image/png", "png"},
    {"image/vnd.wap.wbmp", "wbmp"},
    {"image/webp", "webp"},
    {"image/x-adobe-dng", "dng"},
    {"image/x-canon-cr2", "cr2"},
    {"image/x-fuji-raf", "raf"},
    {"image/x-ms-bmp", "bmp"},
    {"image/x-nikon-nef", "nef"},
    {"image/x-nikon-nrw", "nrw"},
    {"image/x-olympus-orf", "orf"},
    {"image/x-panasonic-rw2", "rw2"},
    {"image/x-pentax-pef", "pef"},
    {"image/x-samsung-srw", "srw"},
    {"image/x-sony-arw", "arw"},
    {"text/html", "html,htm"},
    {"text/plain", "txt"},
    {"video/3gpp", "3gp,3gpp"},
    {"video/3gpp2", "3g2,3gpp2"},
    {"video/avi", "avi"},
    {"video/mp2p", "mpg,mpeg"},
    {"video/mp2ts", "ts"},
    {"video/mp4", "mp4,m4v"},
    {"video/mpeg", "mpg,mpeg"},
    {"video/quicktime", "mov"},
    {"video/webm", "webm"},
    {"video/x-matroska", "mkv"},
    {"video/x-ms-asf", "asf"},
    {"video/x-ms-wmv", "wmv"},
};

constexpr char kApplicationOctetStreamMimeType[] = "application/octet-stream";

}  // namespace

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

std::vector<base::FilePath::StringType> GetExtensionsForArcMimeType(
    const std::string& mime_type) {
  // net::GetExtensionsForMimeType() returns unwanted extensions like
  // "exe,com,bin" for application/octet-stream.
  if (net::MatchesMimeType(kApplicationOctetStreamMimeType, mime_type))
    return std::vector<base::FilePath::StringType>();

  // Attempt net::GetExtensionsForMimeType().
  {
    std::vector<base::FilePath::StringType> extensions;
    net::GetExtensionsForMimeType(mime_type, &extensions);
    if (!extensions.empty()) {
      base::FilePath::StringType preferred_extension;
      if (net::GetPreferredExtensionForMimeType(mime_type,
                                                &preferred_extension)) {
        auto iter = std::find(extensions.begin(), extensions.end(),
                              preferred_extension);
        if (iter == extensions.end()) {
          // This is unlikely to happen, but there is no guarantee.
          extensions.insert(extensions.begin(), preferred_extension);
        } else {
          std::swap(extensions.front(), *iter);
        }
      }
      return extensions;
    }
  }

  // Fallback to our hard-coded list.
  for (const auto& entry : kAndroidMimeTypeMappings) {
    if (net::MatchesMimeType(entry.mime_type, mime_type)) {
      // We assume base::FilePath::StringType == std::string as this code is
      // built only on Chrome OS.
      return base::SplitString(entry.extensions, ",", base::TRIM_WHITESPACE,
                               base::SPLIT_WANT_ALL);
    }
  }

  return std::vector<base::FilePath::StringType>();
}

// TODO(crbug.com/675868): Consolidate with the similar logic for Drive.
base::FilePath::StringType GetFileNameForDocument(
    const mojom::DocumentPtr& document) {
  base::FilePath::StringType filename = document->display_name;

  // Replace path separators appearing in the file name.
  // Chrome OS is POSIX and kSeparators is "/".
  base::ReplaceChars(filename, base::FilePath::kSeparators, "_", &filename);

  // Do not allow an empty file name and all-dots file names.
  if (filename.empty() ||
      filename.find_first_not_of('.', 0) == std::string::npos) {
    filename = "_";
  }

  // Since Chrome detects MIME type from file name extensions, we need to change
  // the file name extension of the document if it does not match with its MIME
  // type.
  // For example, Audio Media Provider presents a music file with its title as
  // the file name.
  base::FilePath::StringType extension =
      base::ToLowerASCII(base::FilePath(filename).Extension());
  if (!extension.empty())
    extension = extension.substr(1);  // Strip the leading dot.
  std::vector<base::FilePath::StringType> possible_extensions =
      GetExtensionsForArcMimeType(document->mime_type);
  if (!possible_extensions.empty() &&
      !base::ContainsValue(possible_extensions, extension)) {
    filename =
        base::FilePath(filename).AddExtension(possible_extensions[0]).value();
  }

  return filename;
}

}  // namespace arc
