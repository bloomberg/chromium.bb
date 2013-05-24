// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/history_item_serialization.h"

#include "content/public/common/page_state.h"
#include "webkit/glue/glue_serialize_deprecated.h"

namespace content {

PageState HistoryItemToPageState(const WebKit::WebHistoryItem& item) {
  return PageState::CreateFromEncodedData(
      webkit_glue::HistoryItemToString(item));
}

WebKit::WebHistoryItem PageStateToHistoryItem(const PageState& state) {
  return webkit_glue::HistoryItemFromString(state.ToEncodedData());
}

}  // namespace content
