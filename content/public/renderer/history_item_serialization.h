// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_HISTORY_ITEM_SERIALIZATION_H_
#define CONTENT_RENDERER_HISTORY_ITEM_SERIALIZATION_H_

#include <string>

#include "content/common/content_export.h"

namespace blink {
class WebHistoryItem;
}

namespace content {
class PageState;

CONTENT_EXPORT PageState HistoryItemToPageState(
    const blink::WebHistoryItem& item);
CONTENT_EXPORT blink::WebHistoryItem PageStateToHistoryItem(
    const PageState& state);

}  // namespace content

#endif  // CONTENT_RENDERER_HISTORY_ITEM_SERIALIZATION_H_
