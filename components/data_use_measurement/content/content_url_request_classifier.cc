// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_use_measurement/content/content_url_request_classifier.h"

#include <string>

#include "base/strings/string_util.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/resource_type.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"

namespace data_use_measurement {

bool IsUserRequest(const net::URLRequest& request) {
  // The presence of ResourecRequestInfo in |request| implies that this request
  // was created for a content::WebContents. For now we could add a condition to
  // check ProcessType in info is content::PROCESS_TYPE_RENDERER, but it won't
  // be compatible with upcoming PlzNavigate architecture. So just existence of
  // ResourceRequestInfo is verified, and the current check should be compatible
  // with upcoming changes in PlzNavigate.
  // TODO(rajendrant): Verify this condition for different use cases. See
  // crbug.com/626063.
  return content::ResourceRequestInfo::ForRequest(&request) != nullptr;
}

bool ContentURLRequestClassifier::IsUserRequest(
    const net::URLRequest& request) const {
  return data_use_measurement::IsUserRequest(request);
}

DataUseUserData::DataUseContentType ContentURLRequestClassifier::GetContentType(
    const net::URLRequest& request,
    const net::HttpResponseHeaders& response_headers) const {
  const content::ResourceRequestInfo* request_info =
      content::ResourceRequestInfo::ForRequest(&request);
  std::string mime_type;
  if (response_headers.GetMimeType(&mime_type)) {
    if (mime_type == "text/html" && request_info &&
        request_info->GetResourceType() ==
            content::ResourceType::RESOURCE_TYPE_MAIN_FRAME) {
      return DataUseUserData::MAIN_FRAME_HTML;
    } else if (mime_type == "text/html") {
      return DataUseUserData::NON_MAIN_FRAME_HTML;
    } else if (mime_type == "text/css") {
      return DataUseUserData::CSS;
    } else if (base::StartsWith(mime_type, "image/",
                                base::CompareCase::SENSITIVE)) {
      return DataUseUserData::IMAGE;
    } else if (base::EndsWith(mime_type, "javascript",
                              base::CompareCase::SENSITIVE) ||
               base::EndsWith(mime_type, "ecmascript",
                              base::CompareCase::SENSITIVE)) {
      return DataUseUserData::JAVASCRIPT;
    } else if (mime_type.find("font") != std::string::npos) {
      return DataUseUserData::FONT;
    } else if (base::StartsWith(mime_type, "audio/",
                                base::CompareCase::SENSITIVE)) {
      return DataUseUserData::AUDIO;
    } else if (base::StartsWith(mime_type, "video/",
                                base::CompareCase::SENSITIVE)) {
      return DataUseUserData::VIDEO;
    }
  }
  return DataUseUserData::OTHER;
}

}  // namespace data_use_measurement
