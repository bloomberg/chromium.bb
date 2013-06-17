// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/page_state.h"

#include "third_party/WebKit/public/platform/WebHTTPBody.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebHistoryItem.h"
#include "webkit/base/file_path_string_conversions.h"
#include "webkit/glue/glue_serialize_deprecated.h"

namespace content {

// static
PageState PageState::CreateFromURL(const GURL& url) {
  return PageState(webkit_glue::CreateHistoryStateForURL(url));
}

// static
PageState PageState::CreateForTesting(
    const GURL& url,
    bool body_contains_password_data,
    const char* optional_body_data,
    const base::FilePath* optional_body_file_path) {
  WebKit::WebHistoryItem history_item;
  history_item.initialize();
  history_item.setURLString(
      WebKit::WebString::fromUTF8(url.possibly_invalid_spec()));
  if (optional_body_data || optional_body_file_path) {
    WebKit::WebHTTPBody http_body;
    http_body.initialize();
    http_body.setContainsPasswordData(body_contains_password_data);
    if (optional_body_data) {
      http_body.appendData(
          WebKit::WebData(optional_body_data, strlen(optional_body_data)));
    }
    if (optional_body_file_path) {
      http_body.appendFile(
          webkit_base::FilePathToWebString(*optional_body_file_path));
    }
    history_item.setHTTPBody(http_body);
  }
  return PageState(webkit_glue::HistoryItemToString(history_item));
}

std::vector<base::FilePath> PageState::GetReferencedFiles() const {
  return webkit_glue::FilePathsFromHistoryState(data_);
}

PageState PageState::RemovePasswordData() const {
  return PageState(webkit_glue::RemovePasswordDataFromHistoryState(data_));
}

PageState PageState::RemoveScrollOffset() const {
  return PageState(webkit_glue::RemoveScrollOffsetFromHistoryState(data_));
}

}  // namespace content
