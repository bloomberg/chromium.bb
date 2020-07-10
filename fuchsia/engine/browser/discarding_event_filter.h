// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_BROWSER_DISCARDING_EVENT_FILTER_H_
#define FUCHSIA_ENGINE_BROWSER_DISCARDING_EVENT_FILTER_H_

#include <memory>

#include "base/macros.h"
#include "ui/events/event_rewriter.h"

// Event filter which will drop incoming events when |discard_events_| is set.
class DiscardingEventFilter : public ui::EventRewriter {
 public:
  DiscardingEventFilter();
  ~DiscardingEventFilter() override;

  void set_discard_events(bool discard_events) {
    discard_events_ = discard_events;
  }

 private:
  // ui::EventRewriter overrides.
  ui::EventRewriteStatus RewriteEvent(
      const ui::Event& event,
      std::unique_ptr<ui::Event>* rewritten_event) override;
  ui::EventRewriteStatus NextDispatchEvent(
      const ui::Event& last_event,
      std::unique_ptr<ui::Event>* new_event) override;

  // When set, all incoming events will be discarded before they are
  // delivered to their sink.
  bool discard_events_ = false;

  DISALLOW_COPY_AND_ASSIGN(DiscardingEventFilter);
};

#endif  // FUCHSIA_ENGINE_BROWSER_DISCARDING_EVENT_FILTER_H_
