// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <new>
#include <tuple>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/common/input/synthetic_web_input_event_builders.h"
#include "content/common/input_messages.h"
#include "content/common/view_messages.h"
#include "content/public/common/content_features.h"
#include "content/renderer/input/input_event_filter.h"
#include "content/renderer/input/input_handler_manager.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_test_sink.h"
#include "ipc/message_filter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/scheduler/test/mock_renderer_scheduler.h"
#include "ui/events/blink/web_input_event_traits.h"

using blink::WebInputEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebTouchEvent;

namespace content {
namespace {

const int kTestRoutingID = 13;

// Simulate a 16ms frame signal.
const base::TimeDelta kFrameInterval = base::TimeDelta::FromMilliseconds(16);

bool ShouldBlockEventStream(const blink::WebInputEvent& event) {
  return ui::WebInputEventTraits::ShouldBlockEventStream(
      event,
      base::FeatureList::IsEnabled(features::kTouchpadAndWheelScrollLatching));
}

class MainThreadEventQueueTest;

class InputEventRecorder : public content::InputHandlerManager {
 public:
  InputEventRecorder(InputEventFilter* filter)
      : InputHandlerManager(nullptr, filter, nullptr, nullptr),
        handle_events_(false),
        send_to_widget_(false),
        passive_(false),
        needs_main_frame_(false) {}

  ~InputEventRecorder() override {}

  void set_handle_events(bool value) { handle_events_ = value; }
  void set_send_to_widget(bool value) { send_to_widget_ = value; }
  void set_passive(bool value) { passive_ = value; }

  size_t record_count() const { return records_.size(); }

  bool needs_main_frame() const { return needs_main_frame_; }
  void reset_needs_main_frame() { needs_main_frame_ = false; }

  const WebInputEvent* record_at(size_t i) const {
    const Record& record = records_[i];
    return reinterpret_cast<const WebInputEvent*>(&record.event_data[0]);
  }

  void Clear() {
    records_.clear();
  }

  void HandleInputEvent(int routing_id,
                        ui::WebScopedInputEvent event,
                        const ui::LatencyInfo& latency_info,
                        const InputHandlerManager::InputEventAckStateCallback&
                            callback) override {
    DCHECK_EQ(kTestRoutingID, routing_id);
    records_.push_back(Record(event.get()));
    if (handle_events_) {
      callback.Run(INPUT_EVENT_ACK_STATE_CONSUMED, std::move(event),
                   latency_info, nullptr);
    } else if (send_to_widget_) {
      if (passive_)
        callback.Run(INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING, std::move(event),
                     latency_info, nullptr);
      else
        callback.Run(INPUT_EVENT_ACK_STATE_NOT_CONSUMED, std::move(event),
                     latency_info, nullptr);
    } else {
      callback.Run(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS, std::move(event),
                   latency_info, nullptr);
    }
  }

  void NeedsMainFrame() { needs_main_frame_ = true; }

 private:
  struct Record {
    Record(const WebInputEvent* event) {
      const char* ptr = reinterpret_cast<const char*>(event);
      event_data.assign(ptr, ptr + event->size());
    }
    std::vector<char> event_data;
  };

  bool handle_events_;
  bool send_to_widget_;
  bool passive_;
  bool needs_main_frame_;
  std::vector<Record> records_;
};

class ReceivedEvent;
class ReceivedMessage;

class ReceivedItem {
 public:
  ReceivedItem() {}
  virtual ~ReceivedItem() {}

  virtual const ReceivedMessage* ItemAsmessage() const {
    NOTREACHED();
    return nullptr;
  }

  virtual const ReceivedEvent* ItemAsEvent() const {
    NOTREACHED();
    return nullptr;
  }
};

class ReceivedMessage : public ReceivedItem {
 public:
  ReceivedMessage(const IPC::Message& message) : message_(message) {}

  ~ReceivedMessage() override {}

  const IPC::Message& message() const { return message_; }

  const ReceivedMessage* ItemAsmessage() const override { return this; }

 private:
  IPC::Message message_;
};

class ReceivedEvent : public ReceivedItem {
 public:
  ReceivedEvent(const blink::WebCoalescedInputEvent& event)
      : event_(event.Event(), event.GetCoalescedEventsPointers()) {}

  ~ReceivedEvent() override {}

  const ReceivedEvent* ItemAsEvent() const override { return this; }

  const blink::WebInputEvent& event() const { return event_.Event(); }

 private:
  blink::WebCoalescedInputEvent event_;
};

class IPCMessageRecorder : public IPC::Listener {
 public:
  bool OnMessageReceived(const IPC::Message& message) override {
    std::unique_ptr<ReceivedItem> item(new ReceivedMessage(message));
    messages_.push_back(std::move(item));
    return true;
  }

  size_t message_count() const { return messages_.size(); }

  const ReceivedMessage& message_at(size_t i) const {
    return *(messages_[i]->ItemAsmessage());
  }

  const ReceivedEvent& event_at(size_t i) const {
    return *(messages_[i]->ItemAsEvent());
  }

  void AppendEvent(const blink::WebCoalescedInputEvent& event) {
    std::unique_ptr<ReceivedItem> item(new ReceivedEvent(event));
    messages_.push_back(std::move(item));
  }

  void Clear() {
    messages_.clear();
  }

 private:
  std::vector<std::unique_ptr<ReceivedItem>> messages_;
};

}  // namespace

class InputEventFilterTest : public testing::Test,
                             public MainThreadEventQueueClient {
 public:
  InputEventFilterTest()
      : main_task_runner_(new base::TestSimpleTaskRunner()) {}

  void SetUp() override {
    filter_ = new InputEventFilter(
        base::Bind(base::IgnoreResult(&IPCMessageRecorder::OnMessageReceived),
                   base::Unretained(&message_recorder_)),
        main_task_runner_, main_task_runner_);
    event_recorder_ = base::MakeUnique<InputEventRecorder>(filter_.get());
    filter_->SetInputHandlerManager(event_recorder_.get());
    filter_->OnFilterAdded(&ipc_sink_);
  }

  void AddMessagesToFilter(const std::vector<IPC::Message>& events) {
    for (size_t i = 0; i < events.size(); ++i)
      filter_->OnMessageReceived(events[i]);

    // base::RunLoop is the "IO Thread".
    base::RunLoop().RunUntilIdle();

    while (event_recorder_->needs_main_frame() ||
           main_task_runner_->HasPendingTask()) {
      main_task_runner_->RunUntilIdle();
      frame_time_ += kFrameInterval;
      event_recorder_->reset_needs_main_frame();
      input_event_queue_->DispatchRafAlignedInput(frame_time_);

      // Run queued io thread tasks.
      base::RunLoop().RunUntilIdle();
    }
  }

  template <typename T>
  void AddEventsToFilter(const T events[], size_t count) {
    std::vector<IPC::Message> messages;
    for (size_t i = 0; i < count; ++i) {
      messages.push_back(InputMsg_HandleInputEvent(
          kTestRoutingID, &events[i], std::vector<const WebInputEvent*>(),
          ui::LatencyInfo(),
          ShouldBlockEventStream(events[i])
              ? InputEventDispatchType::DISPATCH_TYPE_BLOCKING
              : InputEventDispatchType::DISPATCH_TYPE_NON_BLOCKING));
    }

    AddMessagesToFilter(messages);
  }

  void RegisterRoute() {
    input_event_queue_ = new MainThreadEventQueue(this, main_task_runner_,
                                                  &renderer_scheduler_, true);
    filter_->RegisterRoutingID(kTestRoutingID, input_event_queue_);
  }

  void HandleInputEvent(const blink::WebCoalescedInputEvent& event,
                        const ui::LatencyInfo& latency,
                        HandledEventCallback callback) override {
    message_recorder_.AppendEvent(event);
    std::move(callback).Run(INPUT_EVENT_ACK_STATE_NOT_CONSUMED, latency,
                            nullptr, base::nullopt);
  }

  void SetNeedsMainFrame() override { event_recorder_->NeedsMainFrame(); }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  scoped_refptr<base::TestSimpleTaskRunner> main_task_runner_;

  // Used to record IPCs sent by the filter to the RenderWidgetHost.
  IPC::TestSink ipc_sink_;

  // Used to record IPCs forwarded by the filter to the main thread.
  IPCMessageRecorder message_recorder_;

  scoped_refptr<InputEventFilter> filter_;

  blink::scheduler::MockRendererScheduler renderer_scheduler_;
  scoped_refptr<MainThreadEventQueue> input_event_queue_;

  // Used to record WebInputEvents delivered to the handler.
  std::unique_ptr<InputEventRecorder> event_recorder_;

  base::TimeTicks frame_time_;
};

TEST_F(InputEventFilterTest, Basic) {
  WebMouseEvent kEvents[3] = {SyntheticWebMouseEventBuilder::Build(
                                  WebMouseEvent::kMouseMove, 10, 10, 0),
                              SyntheticWebMouseEventBuilder::Build(
                                  WebMouseEvent::kMouseMove, 20, 20, 0),
                              SyntheticWebMouseEventBuilder::Build(
                                  WebMouseEvent::kMouseMove, 30, 30, 0)};

  AddEventsToFilter(kEvents, arraysize(kEvents));
  EXPECT_EQ(0U, ipc_sink_.message_count());
  EXPECT_EQ(0U, event_recorder_->record_count());
  EXPECT_EQ(0U, message_recorder_.message_count());

  RegisterRoute();

  AddEventsToFilter(kEvents, arraysize(kEvents));
  ASSERT_EQ(arraysize(kEvents), ipc_sink_.message_count());
  ASSERT_EQ(arraysize(kEvents), event_recorder_->record_count());
  EXPECT_EQ(0U, message_recorder_.message_count());

  for (size_t i = 0; i < arraysize(kEvents); ++i) {
    const IPC::Message* message = ipc_sink_.GetMessageAt(i);
    EXPECT_EQ(kTestRoutingID, message->routing_id());
    EXPECT_EQ(InputHostMsg_HandleInputEvent_ACK::ID, message->type());

    InputHostMsg_HandleInputEvent_ACK::Param params;
    EXPECT_TRUE(InputHostMsg_HandleInputEvent_ACK::Read(message, &params));
    WebInputEvent::Type event_type = std::get<0>(params).type;
    InputEventAckState ack_result = std::get<0>(params).state;

    EXPECT_EQ(kEvents[i].GetType(), event_type);
    EXPECT_EQ(ack_result, INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);

    const WebInputEvent* event = event_recorder_->record_at(i);
    ASSERT_TRUE(event);

    EXPECT_EQ(kEvents[i].size(), event->size());
    EXPECT_TRUE(memcmp(&kEvents[i], event, event->size()) == 0);
  }

  event_recorder_->set_send_to_widget(true);

  AddEventsToFilter(kEvents, arraysize(kEvents));
  EXPECT_EQ(2 * arraysize(kEvents), ipc_sink_.message_count());
  EXPECT_EQ(2 * arraysize(kEvents), event_recorder_->record_count());
  EXPECT_EQ(1u, message_recorder_.message_count());

  {
    const WebInputEvent& event = message_recorder_.event_at(0).event();
    EXPECT_EQ(kEvents[2].size(), event.size());
    EXPECT_TRUE(memcmp(&kEvents[2], &event, event.size()) == 0);
  }

  // Now reset everything, and test that DidHandleInputEvent is called.

  ipc_sink_.ClearMessages();
  event_recorder_->Clear();
  message_recorder_.Clear();

  event_recorder_->set_handle_events(true);

  AddEventsToFilter(kEvents, arraysize(kEvents));
  EXPECT_EQ(arraysize(kEvents), ipc_sink_.message_count());
  EXPECT_EQ(arraysize(kEvents), event_recorder_->record_count());
  EXPECT_EQ(0U, message_recorder_.message_count());

  for (size_t i = 0; i < arraysize(kEvents); ++i) {
    const IPC::Message* message = ipc_sink_.GetMessageAt(i);
    EXPECT_EQ(kTestRoutingID, message->routing_id());
    EXPECT_EQ(InputHostMsg_HandleInputEvent_ACK::ID, message->type());

    InputHostMsg_HandleInputEvent_ACK::Param params;
    EXPECT_TRUE(InputHostMsg_HandleInputEvent_ACK::Read(message, &params));
    WebInputEvent::Type event_type = std::get<0>(params).type;
    InputEventAckState ack_result = std::get<0>(params).state;
    EXPECT_EQ(kEvents[i].GetType(), event_type);
    EXPECT_EQ(ack_result, INPUT_EVENT_ACK_STATE_CONSUMED);
  }

  filter_->OnFilterRemoved();
}

TEST_F(InputEventFilterTest, PreserveRelativeOrder) {
  RegisterRoute();
  event_recorder_->set_send_to_widget(true);

  WebMouseEvent mouse_down =
      SyntheticWebMouseEventBuilder::Build(WebMouseEvent::kMouseDown);
  WebMouseEvent mouse_up =
      SyntheticWebMouseEventBuilder::Build(WebMouseEvent::kMouseUp);

  std::vector<IPC::Message> messages;
  messages.push_back(InputMsg_HandleInputEvent(
      kTestRoutingID, &mouse_down, std::vector<const WebInputEvent*>(),
      ui::LatencyInfo(),
      ShouldBlockEventStream(mouse_down)
          ? InputEventDispatchType::DISPATCH_TYPE_BLOCKING
          : InputEventDispatchType::DISPATCH_TYPE_NON_BLOCKING));
  // Control where input events are delivered.
  messages.push_back(InputMsg_MouseCaptureLost(kTestRoutingID));
  messages.push_back(InputMsg_SetFocus(kTestRoutingID, true));

  // Editing operations
  messages.push_back(InputMsg_Undo(kTestRoutingID));
  messages.push_back(InputMsg_Redo(kTestRoutingID));
  messages.push_back(InputMsg_Cut(kTestRoutingID));
  messages.push_back(InputMsg_Copy(kTestRoutingID));
#if defined(OS_MACOSX)
  messages.push_back(InputMsg_CopyToFindPboard(kTestRoutingID));
#endif
  messages.push_back(InputMsg_Paste(kTestRoutingID));
  messages.push_back(InputMsg_PasteAndMatchStyle(kTestRoutingID));
  messages.push_back(InputMsg_Delete(kTestRoutingID));
  messages.push_back(InputMsg_Replace(kTestRoutingID, base::string16()));
  messages.push_back(InputMsg_ReplaceMisspelling(kTestRoutingID,
                                                     base::string16()));
  messages.push_back(InputMsg_Delete(kTestRoutingID));
  messages.push_back(InputMsg_SelectAll(kTestRoutingID));
  messages.push_back(InputMsg_CollapseSelection(kTestRoutingID));
  messages.push_back(InputMsg_SelectRange(kTestRoutingID,
                                         gfx::Point(), gfx::Point()));
  messages.push_back(InputMsg_MoveCaret(kTestRoutingID, gfx::Point()));

  messages.push_back(InputMsg_HandleInputEvent(
      kTestRoutingID, &mouse_up, std::vector<const WebInputEvent*>(),
      ui::LatencyInfo(),
      ShouldBlockEventStream(mouse_up)
          ? InputEventDispatchType::DISPATCH_TYPE_BLOCKING
          : InputEventDispatchType::DISPATCH_TYPE_NON_BLOCKING));
  AddMessagesToFilter(messages);

  // We should have sent all messages back to the main thread and preserved
  // their relative order.
  ASSERT_EQ(message_recorder_.message_count(), messages.size());
  EXPECT_EQ(WebMouseEvent::kMouseDown,
            message_recorder_.event_at(0).event().GetType());
  for (size_t i = 1; i < messages.size() - 1; ++i) {
    EXPECT_EQ(message_recorder_.message_at(i).message().type(),
              messages[i].type())
        << i;
  }
  EXPECT_EQ(WebMouseEvent::kMouseUp,
            message_recorder_.event_at(messages.size() - 1).event().GetType());
}

TEST_F(InputEventFilterTest, NonBlockingWheel) {
  WebMouseWheelEvent kEvents[4] = {
      SyntheticWebMouseWheelEventBuilder::Build(10, 10, 0, 53, 1, false),
      SyntheticWebMouseWheelEventBuilder::Build(20, 20, 0, 53, 0, false),
      SyntheticWebMouseWheelEventBuilder::Build(30, 30, 0, 53, 1, false),
      SyntheticWebMouseWheelEventBuilder::Build(30, 30, 0, 53, 1, false),
  };

  RegisterRoute();
  event_recorder_->set_send_to_widget(true);
  event_recorder_->set_passive(true);

  AddEventsToFilter(kEvents, arraysize(kEvents));
  EXPECT_EQ(arraysize(kEvents), event_recorder_->record_count());
  ASSERT_EQ(4u, ipc_sink_.message_count());

  // All events are handled, one is coalesced.
  EXPECT_EQ(3u, message_recorder_.message_count());

  // First two messages should be identical.
  for (size_t i = 0; i < 2; ++i) {
    const ReceivedEvent& message = message_recorder_.event_at(i);
    const WebInputEvent& event = message.event();
    EXPECT_EQ(kEvents[i].size(), event.size());
    kEvents[i].dispatch_type =
        WebInputEvent::DispatchType::kListenersNonBlockingPassive;
    EXPECT_TRUE(memcmp(&kEvents[i], &event, event.size()) == 0);
  }

  // Third message is coalesced.
  {
    const ReceivedEvent& message = message_recorder_.event_at(2);
    const WebMouseWheelEvent& event =
        static_cast<const WebMouseWheelEvent&>(message.event());
    kEvents[2].dispatch_type =
        WebInputEvent::DispatchType::kListenersNonBlockingPassive;
    EXPECT_EQ(kEvents[2].size(), event.size());
    EXPECT_EQ(kEvents[2].delta_x + kEvents[3].delta_x, event.delta_x);
    EXPECT_EQ(kEvents[2].delta_y + kEvents[3].delta_y, event.delta_y);
  }
}

TEST_F(InputEventFilterTest, NonBlockingTouch) {
  SyntheticWebTouchEvent kEvents[4];
  kEvents[0].PressPoint(10, 10);
  kEvents[1].PressPoint(10, 10);
  kEvents[1].SetModifiers(1);
  kEvents[1].MovePoint(0, 20, 20);
  kEvents[2].PressPoint(10, 10);
  kEvents[2].MovePoint(0, 30, 30);
  kEvents[3].PressPoint(10, 10);
  kEvents[3].MovePoint(0, 35, 35);

  RegisterRoute();
  event_recorder_->set_send_to_widget(true);
  event_recorder_->set_passive(true);

  AddEventsToFilter(kEvents, arraysize(kEvents));
  EXPECT_EQ(arraysize(kEvents), event_recorder_->record_count());
  ASSERT_EQ(4u, ipc_sink_.message_count());

  // All events are handled and one set was coalesced.
  EXPECT_EQ(3u, message_recorder_.message_count());

  // First two messages should be identical.
  for (size_t i = 0; i < 2; ++i) {
    const ReceivedEvent& message = message_recorder_.event_at(i);
    const WebInputEvent& event = message.event();
    EXPECT_EQ(kEvents[i].size(), event.size());
    kEvents[i].dispatch_type =
        WebInputEvent::DispatchType::kListenersNonBlockingPassive;
    EXPECT_TRUE(memcmp(&kEvents[i], &event, event.size()) == 0);
  }

  // Third message is coalesced.
  {
    const ReceivedEvent& message = message_recorder_.event_at(2);
    const WebTouchEvent& event =
        static_cast<const WebTouchEvent&>(message.event());
    EXPECT_EQ(kEvents[3].size(), event.size());
    EXPECT_EQ(1u, kEvents[3].touches_length);
    EXPECT_EQ(kEvents[3].touches[0].PositionInWidget().x,
              event.touches[0].PositionInWidget().x);
    EXPECT_EQ(kEvents[3].touches[0].PositionInWidget().y,
              event.touches[0].PositionInWidget().y);
  }
}

TEST_F(InputEventFilterTest, IntermingledNonBlockingTouch) {
  SyntheticWebTouchEvent kEvents[2];
  kEvents[0].PressPoint(10, 10);
  kEvents[1].PressPoint(10, 10);
  kEvents[1].ReleasePoint(0);
  SyntheticWebTouchEvent kBlockingEvents[1];
  kBlockingEvents[0].PressPoint(10, 10);

  RegisterRoute();
  event_recorder_->set_send_to_widget(true);
  event_recorder_->set_passive(true);
  AddEventsToFilter(kEvents, arraysize(kEvents));
  EXPECT_EQ(arraysize(kEvents), event_recorder_->record_count());

  event_recorder_->set_passive(false);
  AddEventsToFilter(kBlockingEvents, arraysize(kBlockingEvents));
  EXPECT_EQ(arraysize(kEvents) + arraysize(kBlockingEvents),
            event_recorder_->record_count());
  ASSERT_EQ(3u, event_recorder_->record_count());
  EXPECT_EQ(3u, message_recorder_.message_count());

  {
    const ReceivedEvent& message = message_recorder_.event_at(0);
    const WebInputEvent& event = message.event();
    EXPECT_EQ(kEvents[0].size(), event.size());
    kEvents[0].dispatch_type =
        WebInputEvent::DispatchType::kListenersNonBlockingPassive;
    EXPECT_TRUE(memcmp(&kEvents[0], &event, event.size()) == 0);
  }

  {
    const ReceivedEvent& message = message_recorder_.event_at(1);
    const WebInputEvent& event = message.event();
    EXPECT_EQ(kEvents[1].size(), event.size());
    kEvents[1].dispatch_type =
        WebInputEvent::DispatchType::kListenersNonBlockingPassive;
    EXPECT_TRUE(memcmp(&kEvents[1], &event, event.size()) == 0);
  }

  {
    const ReceivedEvent& message = message_recorder_.event_at(2);
    const WebInputEvent& event = message.event();
    EXPECT_EQ(kBlockingEvents[0].size(), event.size());
    EXPECT_TRUE(memcmp(&kBlockingEvents[0], &event, event.size()) == 0);
  }
}

}  // namespace content
