// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/fileapi/arc_documents_provider_root_map.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_documents_provider_root.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_documents_provider_util.h"

namespace arc {

namespace {

struct DocumentsProviderSpec {
  const char* authority;
  const char* root_document_id;
};

// List of allowed documents providers for production.
constexpr DocumentsProviderSpec kDocumentsProviderWhitelist[] = {
    {"com.android.providers.media.documents", "images_root"},
    {"com.android.providers.media.documents", "videos_root"},
    {"com.android.providers.media.documents", "audio_root"},
};

}  // namespace

ArcDocumentsProviderRootMap::ArcDocumentsProviderRootMap() {
  for (auto spec : kDocumentsProviderWhitelist) {
    map_[Key(spec.authority, spec.root_document_id)] =
        base::MakeUnique<ArcDocumentsProviderRoot>(spec.authority,
                                                   spec.root_document_id);
  }
}

ArcDocumentsProviderRootMap::~ArcDocumentsProviderRootMap() = default;

ArcDocumentsProviderRoot* ArcDocumentsProviderRootMap::ParseAndLookup(
    const storage::FileSystemURL& url,
    base::FilePath* path) const {
  std::string authority;
  std::string root_document_id;
  base::FilePath tmp_path;
  if (!ParseDocumentsProviderUrl(url, &authority, &root_document_id, &tmp_path))
    return nullptr;
  auto iter = map_.find(Key(authority, root_document_id));
  if (iter == map_.end())
    return nullptr;
  *path = tmp_path;
  return iter->second.get();
}

}  // namespace arc
