// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/discarding_event_filter.h"

DiscardingEventFilter::DiscardingEventFilter() = default;

DiscardingEventFilter::~DiscardingEventFilter() = default;

ui::EventRewriteStatus DiscardingEventFilter::RewriteEvent(
    const ui::Event& event,
    std::unique_ptr<ui::Event>* rewritten_event) {
  if (discard_events_)
    return ui::EventRewriteStatus::EVENT_REWRITE_DISCARD;
  else
    return ui::EventRewriteStatus::EVENT_REWRITE_CONTINUE;
}

ui::EventRewriteStatus DiscardingEventFilter::NextDispatchEvent(
    const ui::Event& last_event,
    std::unique_ptr<ui::Event>* new_event) {
  // Never invoked because RewriteEvent() does not return
  // EVENT_REWRITE_DISPATCH_ANOTHER. Refer to the EventRewriter base class'
  // comments for more information on this API quirk.
  NOTREACHED();

  return ui::EventRewriteStatus::EVENT_REWRITE_DISPATCH_ANOTHER;
}
