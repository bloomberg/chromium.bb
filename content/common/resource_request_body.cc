// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/resource_request_body.h"

#include "base/strings/utf_string_conversions.h"
#include "content/common/page_state_serialization.h"

using blink::WebHTTPBody;
using blink::WebString;

namespace content {

ResourceRequestBody::ResourceRequestBody()
    : identifier_(0) {
}

void ResourceRequestBody::AppendExplodedHTTPBodyElement(
    const ExplodedHttpBodyElement& element) {
  // Note: this code is based on GetRequestBodyForWebURLRequest (in
  // web_url_request_util.cc). The other function transforms a
  // blink::WebHTTPBody into a ResourceRequestBody. This function is used to
  // transform an ExplodedHttpBody into a ResourceRequestBody.
  switch (element.type) {
    case WebHTTPBody::Element::TypeData:
      if (!element.data.empty()) {
        // Blink sometimes gives empty data to append. These aren't
        // necessary so they are just optimized out here.
        AppendBytes(element.data.data(), static_cast<int>(element.data.size()));
      }
      break;
    case WebHTTPBody::Element::TypeFile:
      if (element.file_length == -1) {
        AppendFileRange(
            base::FilePath::FromUTF16Unsafe(element.file_path.string()), 0,
            std::numeric_limits<uint64_t>::max(), base::Time());
      } else {
        AppendFileRange(
            base::FilePath::FromUTF16Unsafe(element.file_path.string()),
            static_cast<uint64_t>(element.file_start),
            static_cast<uint64_t>(element.file_length),
            base::Time::FromDoubleT(element.file_modification_time));
      }
      break;
    case WebHTTPBody::Element::TypeFileSystemURL: {
      GURL file_system_url = element.filesystem_url;
      DCHECK(file_system_url.SchemeIsFileSystem());
      AppendFileSystemFileRange(
          file_system_url, static_cast<uint64_t>(element.file_start),
          static_cast<uint64_t>(element.file_length),
          base::Time::FromDoubleT(element.file_modification_time));
      break;
    }
    case WebHTTPBody::Element::TypeBlob:
      AppendBlob(element.blob_uuid);
      break;
    default:
      NOTREACHED();
  }
}

void ResourceRequestBody::AppendBytes(const char* bytes, int bytes_len) {
  if (bytes_len > 0) {
    elements_.push_back(Element());
    elements_.back().SetToBytes(bytes, bytes_len);
  }
}

void ResourceRequestBody::AppendFileRange(
    const base::FilePath& file_path,
    uint64_t offset,
    uint64_t length,
    const base::Time& expected_modification_time) {
  elements_.push_back(Element());
  elements_.back().SetToFilePathRange(file_path, offset, length,
                                      expected_modification_time);
}

void ResourceRequestBody::AppendBlob(const std::string& uuid) {
  elements_.push_back(Element());
  elements_.back().SetToBlob(uuid);
}

void ResourceRequestBody::AppendFileSystemFileRange(
    const GURL& url,
    uint64_t offset,
    uint64_t length,
    const base::Time& expected_modification_time) {
  elements_.push_back(Element());
  elements_.back().SetToFileSystemUrlRange(url, offset, length,
                                           expected_modification_time);
}

ResourceRequestBody::~ResourceRequestBody() {
}

}  // namespace content
