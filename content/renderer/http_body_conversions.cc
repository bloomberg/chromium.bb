// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/http_body_conversions.h"

#include <stddef.h>

#include "base/files/file_path.h"
#include "base/strings/nullable_string16.h"
#include "base/time/time.h"
#include "third_party/WebKit/public/platform/WebData.h"
#include "third_party/WebKit/public/platform/WebHTTPBody.h"
#include "third_party/WebKit/public/platform/WebString.h"

using blink::WebData;
using blink::WebHTTPBody;
using blink::WebString;

namespace content {

void ConvertToHttpBodyElement(const WebHTTPBody::Element& input,
                              ResourceRequestBody::Element* output) {
  switch (input.type) {
    case WebHTTPBody::Element::TypeData:
      output->SetToBytes(input.data.data(), input.data.size());
      break;
    case WebHTTPBody::Element::TypeFile:
      output->SetToFilePathRange(
          base::FilePath::FromUTF8Unsafe(input.filePath.utf8()),
          static_cast<uint64_t>(input.fileStart),
          static_cast<uint64_t>(input.fileLength),
          base::Time::FromDoubleT(input.modificationTime));
      break;
    case WebHTTPBody::Element::TypeFileSystemURL:
      output->SetToFileSystemUrlRange(
          input.fileSystemURL, static_cast<uint64_t>(input.fileStart),
          static_cast<uint64_t>(input.fileLength),
          base::Time::FromDoubleT(input.modificationTime));
      break;
    case WebHTTPBody::Element::TypeBlob:
      output->SetToBlob(input.blobUUID.utf8());
      break;
    default:
      output->SetToEmptyBytes();
      NOTREACHED();
      break;
  }
}

// TODO(lukasza): Dedupe wrt GetRequestBodyForWebURLRequest.
void AppendHttpBodyElement(const ResourceRequestBody::Element& element,
                           WebHTTPBody* http_body) {
  switch (element.type()) {
    case storage::DataElement::TYPE_BYTES:
      http_body->appendData(WebData(element.bytes(), element.length()));
      break;
    case storage::DataElement::TYPE_FILE:
      http_body->appendFileRange(
          element.path().AsUTF16Unsafe(), element.offset(),
          (element.length() != std::numeric_limits<uint64_t>::max())
              ? element.length()
              : -1,
          element.expected_modification_time().ToDoubleT());
      break;
    case storage::DataElement::TYPE_FILE_FILESYSTEM:
      http_body->appendFileSystemURLRange(
          element.filesystem_url(), element.offset(),
          (element.length() != std::numeric_limits<uint64_t>::max())
              ? element.length()
              : -1,
          element.expected_modification_time().ToDoubleT());
      break;
    case storage::DataElement::TYPE_BLOB:
      http_body->appendBlob(WebString::fromUTF8(element.blob_uuid()));
      break;
    case storage::DataElement::TYPE_BYTES_DESCRIPTION:
    case storage::DataElement::TYPE_DISK_CACHE_ENTRY:
    default:
      NOTREACHED();
      break;
  }
}

}  // namespace content
