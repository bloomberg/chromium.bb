// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <new>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/message_loop.h"
#include "content/common/view_messages.h"
#include "content/renderer/gpu/input_event_filter.h"
#include "ipc/ipc_test_sink.h"
#include "testing/gtest/include/gtest/gtest.h"

using WebKit::WebInputEvent;
using WebKit::WebMouseEvent;

namespace content {
namespace {

const int kTestRoutingID = 13;

class InputEventRecorder {
 public:
  InputEventRecorder()
      : filter_(NULL),
        handle_events_(false),
        send_to_widget_(false) {
  }

  void set_filter(InputEventFilter* filter) { filter_ = filter; }
  void set_handle_events(bool value) { handle_events_ = value; }
  void set_send_to_widget(bool value) { send_to_widget_ = value; }

  size_t record_count() const { return records_.size(); }

  const WebInputEvent* record_at(size_t i) const {
    const Record& record = records_[i];
    return reinterpret_cast<const WebInputEvent*>(&record.event_data[0]);
  }

  void Clear() {
    records_.clear();
  }

  void HandleInputEvent(int routing_id, const WebInputEvent* event) {
    DCHECK_EQ(kTestRoutingID, routing_id);

    records_.push_back(Record(event));

    if (handle_events_) {
      filter_->DidHandleInputEvent();
    } else {
      filter_->DidNotHandleInputEvent(send_to_widget_);
    }
  }

 private:
  struct Record {
    Record(const WebInputEvent* event) {
      const char* ptr = reinterpret_cast<const char*>(event);
      event_data.assign(ptr, ptr + event->size);
    }
    std::vector<char> event_data;
  };

  InputEventFilter* filter_;
  bool handle_events_;
  bool send_to_widget_;
  std::vector<Record> records_;
};

class IPCMessageRecorder : public IPC::Listener {
 public:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE {
    messages_.push_back(message);
    return true;
  }

  size_t message_count() const { return messages_.size(); }

  const IPC::Message& message_at(size_t i) const {
    return messages_[i];
  }

  void Clear() {
    messages_.clear();
  }

 private:
  std::vector<IPC::Message> messages_;
};

void InitMouseEvent(WebMouseEvent* event, WebInputEvent::Type type,
                    int x, int y) {
  // Avoid valgrind false positives by initializing memory completely.
  memset(event, 0, sizeof(*event));

  new (event) WebMouseEvent();
  event->type = type;
  event->x = x;
  event->y = y;
}

void AddEventsToFilter(IPC::ChannelProxy::MessageFilter* message_filter,
                       const WebMouseEvent events[],
                       size_t count) {
  for (size_t i = 0; i < count; ++i) {
    ViewMsg_HandleInputEvent message(kTestRoutingID, &events[i], false);
    message_filter->OnMessageReceived(message);
  }

  MessageLoop::current()->RunUntilIdle();
}

}  // namespace

TEST(InputEventFilterTest, Basic) {
  MessageLoop message_loop;

  // Used to record IPCs sent by the filter to the RenderWidgetHost.
  IPC::TestSink ipc_sink;

  // Used to record IPCs forwarded by the filter to the main thread.
  IPCMessageRecorder message_recorder;

  // Used to record WebInputEvents delivered to the handler.
  InputEventRecorder event_recorder;

  scoped_refptr<InputEventFilter> filter =
      new InputEventFilter(&message_recorder,
                           message_loop.message_loop_proxy(),
                           base::Bind(&InputEventRecorder::HandleInputEvent,
                                      base::Unretained(&event_recorder)));
  event_recorder.set_filter(filter);

  filter->OnFilterAdded(&ipc_sink);

  WebMouseEvent kEvents[3];
  InitMouseEvent(&kEvents[0], WebInputEvent::MouseDown, 10, 10);
  InitMouseEvent(&kEvents[1], WebInputEvent::MouseMove, 20, 20);
  InitMouseEvent(&kEvents[2], WebInputEvent::MouseUp, 30, 30);

  AddEventsToFilter(filter, kEvents, arraysize(kEvents));
  EXPECT_EQ(0U, ipc_sink.message_count());
  EXPECT_EQ(0U, event_recorder.record_count());
  EXPECT_EQ(0U, message_recorder.message_count());

  filter->AddRoute(kTestRoutingID);

  AddEventsToFilter(filter, kEvents, arraysize(kEvents));
  ASSERT_EQ(arraysize(kEvents), ipc_sink.message_count());
  ASSERT_EQ(arraysize(kEvents), event_recorder.record_count());
  EXPECT_EQ(0U, message_recorder.message_count());

  for (size_t i = 0; i < arraysize(kEvents); ++i) {
    const IPC::Message* message = ipc_sink.GetMessageAt(i);
    EXPECT_EQ(kTestRoutingID, message->routing_id());
    EXPECT_EQ(ViewHostMsg_HandleInputEvent_ACK::ID, message->type());

    WebInputEvent::Type event_type = WebInputEvent::Undefined;
    InputEventAckState ack_result = INPUT_EVENT_ACK_STATE_NOT_CONSUMED;
    EXPECT_TRUE(ViewHostMsg_HandleInputEvent_ACK::Read(message, &event_type,
                                                       &ack_result));
    EXPECT_EQ(kEvents[i].type, event_type);
    EXPECT_EQ(ack_result, INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);

    const WebInputEvent* event = event_recorder.record_at(i);
    ASSERT_TRUE(event);

    EXPECT_EQ(kEvents[i].size, event->size);
    EXPECT_TRUE(memcmp(&kEvents[i], event, event->size) == 0);
  }

  event_recorder.set_send_to_widget(true);

  AddEventsToFilter(filter, kEvents, arraysize(kEvents));
  EXPECT_EQ(arraysize(kEvents), ipc_sink.message_count());
  EXPECT_EQ(2 * arraysize(kEvents), event_recorder.record_count());
  EXPECT_EQ(arraysize(kEvents), message_recorder.message_count());

  for (size_t i = 0; i < arraysize(kEvents); ++i) {
    const IPC::Message& message = message_recorder.message_at(i);

    ASSERT_EQ(ViewMsg_HandleInputEvent::ID, message.type());
    const WebInputEvent* event = InputEventFilter::CrackMessage(message);

    EXPECT_EQ(kEvents[i].size, event->size);
    EXPECT_TRUE(memcmp(&kEvents[i], event, event->size) == 0);
  }

  // Now reset everything, and test that DidHandleInputEvent is called.

  ipc_sink.ClearMessages();
  event_recorder.Clear();
  message_recorder.Clear();

  event_recorder.set_handle_events(true);

  AddEventsToFilter(filter, kEvents, arraysize(kEvents));
  EXPECT_EQ(arraysize(kEvents), ipc_sink.message_count());
  EXPECT_EQ(arraysize(kEvents), event_recorder.record_count());
  EXPECT_EQ(0U, message_recorder.message_count());

  for (size_t i = 0; i < arraysize(kEvents); ++i) {
    const IPC::Message* message = ipc_sink.GetMessageAt(i);
    EXPECT_EQ(kTestRoutingID, message->routing_id());
    EXPECT_EQ(ViewHostMsg_HandleInputEvent_ACK::ID, message->type());

    WebInputEvent::Type event_type = WebInputEvent::Undefined;
    InputEventAckState ack_result = INPUT_EVENT_ACK_STATE_NOT_CONSUMED;
    EXPECT_TRUE(ViewHostMsg_HandleInputEvent_ACK::Read(message, &event_type,
                                                       &ack_result));
    EXPECT_EQ(kEvents[i].type, event_type);
    EXPECT_EQ(ack_result, INPUT_EVENT_ACK_STATE_CONSUMED);
  }

  filter->OnFilterRemoved();
}

}  // namespace content
