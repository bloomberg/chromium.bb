// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_MEDIA_GALLERY_UTIL_PUBLIC_CPP_SAFE_MEDIA_METADATA_PARSER_H_
#define CHROME_SERVICES_MEDIA_GALLERY_UTIL_PUBLIC_CPP_SAFE_MEDIA_METADATA_PARSER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/common/media_galleries/metadata_types.h"
#include "chrome/services/media_gallery_util/public/mojom/media_parser.mojom.h"

namespace content {
class BrowserContext;
}

namespace service_manager {
class Connector;
}

// Parses the media metadata of a Blob safely in a utility process. This class
// expects the MIME type of the Blob to be already known. It creates a utility
// process to do further MIME-type-specific metadata extraction from the Blob
// data.
class SafeMediaMetadataParser {
 public:
  typedef base::OnceCallback<void(
      bool parse_success,
      std::unique_ptr<base::DictionaryValue> metadata_dictionary,
      std::unique_ptr<std::vector<metadata::AttachedImage>> attached_images)>
      DoneCallback;

  SafeMediaMetadataParser(content::BrowserContext* browser_context,
                          const std::string& blob_uuid,
                          int64_t blob_size,
                          const std::string& mime_type,
                          bool get_attached_images);
  ~SafeMediaMetadataParser();

  // Should be called on the thread |connector| is associated with. |callback|
  // is invoked on that same thread.
  void Start(service_manager::Connector* connector, DoneCallback callback);

 private:
  class MediaDataSourceImpl;

  // Callback if the utility process or metadata parse request fails.
  void ParseMediaMetadataFailed();

  // Callback from utility process when it finishes parsing metadata.
  void ParseMediaMetadataDone(
      bool parse_success,
      std::unique_ptr<base::DictionaryValue> metadata_dictionary,
      const std::vector<metadata::AttachedImage>& attached_images);

  // Starts to read the blob data and sends the data back to the utility
  // process.
  void StartBlobRequest(
      chrome::mojom::MediaDataSource::ReadBlobCallback callback,
      int64_t position,
      int64_t length);

  // Invoked when the full blob content has been read.
  void BlobReaderDone(chrome::mojom::MediaDataSource::ReadBlobCallback callback,
                      std::unique_ptr<std::string> data,
                      int64_t /* blob_total_size */);

  content::BrowserContext* const browser_context_;
  const std::string blob_uuid_;
  const int64_t blob_size_;
  const std::string mime_type_;
  bool get_attached_images_;

  chrome::mojom::MediaParserPtr media_parser_ptr_;
  DoneCallback callback_;

  std::unique_ptr<MediaDataSourceImpl> media_data_source_;

  base::WeakPtrFactory<SafeMediaMetadataParser> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SafeMediaMetadataParser);
};

#endif  // CHROME_SERVICES_MEDIA_GALLERY_UTIL_PUBLIC_CPP_SAFE_MEDIA_METADATA_PARSER_H_
