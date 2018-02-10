// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_MEDIA_GALLERY_UTIL_MEDIA_PARSER_H_
#define CHROME_SERVICES_MEDIA_GALLERY_UTIL_MEDIA_PARSER_H_

#include <memory>
#include <string>

#include "base/files/file.h"
#include "base/time/time.h"
#include "chrome/services/media_gallery_util/public/mojom/media_parser.mojom.h"
#include "services/service_manager/public/cpp/service_context_ref.h"

class MediaParser : public chrome::mojom::MediaParser {
 public:
  explicit MediaParser(
      std::unique_ptr<service_manager::ServiceContextRef> service_ref);
  ~MediaParser() override;

 private:
  // chrome::mojom::MediaParser:
  void ParseMediaMetadata(const std::string& mime_type,
                          int64_t total_size,
                          bool get_attached_images,
                          chrome::mojom::MediaDataSourcePtr media_data_source,
                          ParseMediaMetadataCallback callback) override;
  void CheckMediaFile(base::TimeDelta decode_time,
                      base::File file,
                      CheckMediaFileCallback callback) override;

  const std::unique_ptr<service_manager::ServiceContextRef> service_ref_;

  DISALLOW_COPY_AND_ASSIGN(MediaParser);
};

#endif  // CHROME_SERVICES_MEDIA_GALLERY_UTIL_MEDIA_PARSER_H_
