// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCK_WEB_MIDI_ACCESSOR_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCK_WEB_MIDI_ACCESSOR_H_

#include "base/basictypes.h"
#include "content/shell/renderer/test_runner/web_task.h"
#include "third_party/WebKit/public/platform/WebMIDIAccessor.h"

namespace blink {
class WebMIDIAccessorClient;
}

namespace content {

class TestInterfaces;

class MockWebMIDIAccessor : public blink::WebMIDIAccessor {
 public:
  explicit MockWebMIDIAccessor(blink::WebMIDIAccessorClient* client,
                               TestInterfaces* interfaces);
  virtual ~MockWebMIDIAccessor();

  // blink::WebMIDIAccessor implementation.
  virtual void startSession() OVERRIDE;
  virtual void sendMIDIData(unsigned port_index,
                            const unsigned char* data,
                            size_t length,
                            double timestamp) OVERRIDE {}

  // WebTask related methods
  WebTaskList* mutable_task_list() { return &task_list_; }

 private:
  blink::WebMIDIAccessorClient* client_;
  WebTaskList task_list_;
  TestInterfaces* interfaces_;

  DISALLOW_COPY_AND_ASSIGN(MockWebMIDIAccessor);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCK_WEB_MIDI_ACCESSOR_H_
