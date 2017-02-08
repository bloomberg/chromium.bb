// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/drop_data.h"

#include "base/strings/utf_string_conversions.h"
#include "components/mime_util/mime_util.h"
#include "net/base/filename_util.h"
#include "net/base/mime_util.h"

namespace content {

DropData::Metadata::Metadata() {}

// static
DropData::Metadata DropData::Metadata::CreateForMimeType(
    const Kind& kind,
    const base::string16& mime_type) {
  Metadata metadata;
  metadata.kind = kind;
  metadata.mime_type = mime_type;
  return metadata;
}

// static
DropData::Metadata DropData::Metadata::CreateForFilePath(
    const base::FilePath& filename) {
  Metadata metadata;
  metadata.kind = Kind::FILENAME;
  metadata.filename = filename;
  return metadata;
}

// static
DropData::Metadata DropData::Metadata::CreateForFileSystemUrl(
    const GURL& file_system_url) {
  Metadata metadata;
  metadata.kind = Kind::FILESYSTEMFILE;
  metadata.file_system_url = file_system_url;
  return metadata;
}

DropData::Metadata::Metadata(const DropData::Metadata& other) = default;

DropData::Metadata::~Metadata() {}

DropData::DropData()
    : did_originate_from_renderer(false),
      referrer_policy(blink::WebReferrerPolicyDefault),
      key_modifiers(0) {}

DropData::DropData(const DropData& other) = default;

DropData::~DropData() {}

base::Optional<base::FilePath> DropData::GetSafeFilenameForImageFileContents()
    const {
  base::FilePath file_name = net::GenerateFileName(
      file_contents_source_url, file_contents_content_disposition,
      std::string(),   // referrer_charset
      std::string(),   // suggested_name
      std::string(),   // mime_type
      std::string());  // default_name
  std::string mime_type;
  if (net::GetWellKnownMimeTypeFromExtension(file_contents_filename_extension,
                                             &mime_type) &&
      mime_util::IsSupportedImageMimeType(mime_type)) {
    return file_name.ReplaceExtension(file_contents_filename_extension);
  }
  return base::nullopt;
}

}  // namespace content
