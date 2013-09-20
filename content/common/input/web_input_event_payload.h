// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_WEB_INPUT_EVENT_PAYLOAD_H_
#define CONTENT_COMMON_INPUT_WEB_INPUT_EVENT_PAYLOAD_H_

#include "base/basictypes.h"
#include "content/common/content_export.h"
#include "content/common/input/input_event.h"
#include "ui/base/latency_info.h"

namespace WebKit {
class WebInputEvent;
}

namespace content {

// Wraps a WebKit::WebInputEvent.
class CONTENT_EXPORT WebInputEventPayload : public InputEvent::Payload {
 public:
  static scoped_ptr<WebInputEventPayload> Create();
  static scoped_ptr<WebInputEventPayload> Create(
      const WebKit::WebInputEvent& event,
      const ui::LatencyInfo& latency_info,
      bool is_keyboard_shortcut);

  static const WebInputEventPayload* Cast(const Payload* payload);

  virtual ~WebInputEventPayload();

  // InputEvent::Payload
  virtual Type GetType() const OVERRIDE;

  void Initialize(const WebKit::WebInputEvent& event,
                  const ui::LatencyInfo& latency_info,
                  bool is_keyboard_shortcut);

  // True for WebTouchEvents only.
  bool CanCreateFollowupEvents() const;

  const WebKit::WebInputEvent* web_event() const { return web_event_.get(); }
  const ui::LatencyInfo& latency_info() const { return latency_info_; }
  bool is_keyboard_shortcut() const { return is_keyboard_shortcut_; }

  // WebKit::WebInputEvent does not provide a virtual destructor.
  struct WebInputEventDeleter {
    WebInputEventDeleter();
    void operator()(WebKit::WebInputEvent* event) const;
  };
  typedef scoped_ptr<WebKit::WebInputEvent,
                     WebInputEventDeleter> ScopedWebInputEvent;

 protected:
  WebInputEventPayload();

 private:
  ScopedWebInputEvent web_event_;
  ui::LatencyInfo latency_info_;
  bool is_keyboard_shortcut_;

  DISALLOW_COPY_AND_ASSIGN(WebInputEventPayload);
};

}  // namespace content

#endif  // CONTENT_COMMON_INPUT_WEB_INPUT_EVENT_PAYLOAD_H_
