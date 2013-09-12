// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_BROWSER_INPUT_EVENT_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_BROWSER_INPUT_EVENT_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_vector.h"
#include "content/common/input/input_event.h"
#include "content/common/input/input_event_disposition.h"

namespace content {

class BrowserInputEvent;

// Provides customized dispatch response for BrowserInputEvents.
class BrowserInputEventClient {
 public:
  virtual ~BrowserInputEventClient() {}

  virtual void OnDispatched(const BrowserInputEvent& event,
                            InputEventDisposition disposition) {}

  // Called if the event went unconsumed and can create followup events. Any
  // events added to |followup| by the client will be inserted into the
  // current input event stream. |followup| will never be NULL.
  virtual void OnDispatched(const BrowserInputEvent& event,
                            InputEventDisposition disposition,
                            ScopedVector<BrowserInputEvent>* followup) {}
};

// Augmented InputEvent allowing customized dispatch response in the browser.
class CONTENT_EXPORT BrowserInputEvent : public InputEvent {
 public:
  // |client| is assumed to be non-NULL.
  static scoped_ptr<BrowserInputEvent> Create(
      int64 id,
      scoped_ptr<InputEvent::Payload> payload,
      BrowserInputEventClient* client);

  template <typename PayloadType>
  static scoped_ptr<BrowserInputEvent> Create(int64 id,
                                              scoped_ptr<PayloadType> payload,
                                              BrowserInputEventClient* client) {
    return Create(id, payload.template PassAs<InputEvent::Payload>(), client);
  }

  virtual ~BrowserInputEvent();

  // |followup_events| must not be NULL, and will only be modified if the
  // event went unconsumed and can create followup events.
  void OnDispatched(InputEventDisposition disposition,
                    ScopedVector<BrowserInputEvent>* followup_events);

 protected:
  explicit BrowserInputEvent(BrowserInputEventClient* client);

  bool CanCreateFollowupEvents() const;

 private:
  BrowserInputEventClient* client_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_BROWSER_INPUT_EVENT_H_
