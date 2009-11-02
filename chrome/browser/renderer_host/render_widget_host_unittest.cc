// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/gfx/canvas.h"
#include "base/basictypes.h"
#include "base/keyboard_codes.h"
#include "base/scoped_ptr.h"
#include "base/shared_memory.h"
#include "build/build_config.h"
#include "chrome/browser/renderer_host/backing_store.h"
#include "chrome/browser/renderer_host/test/test_render_view_host.h"
#include "chrome/common/render_messages.h"
#include "testing/gtest/include/gtest/gtest.h"

using WebKit::WebInputEvent;

// RenderWidgetHostProcess -----------------------------------------------------

class RenderWidgetHostProcess : public MockRenderProcessHost {
 public:
  explicit RenderWidgetHostProcess(Profile* profile)
      : MockRenderProcessHost(profile),
        current_paint_buf_(NULL),
        paint_msg_should_reply_(false),
        paint_msg_reply_flags_(0) {
    // DANGER! This is a hack. The RenderWidgetHost checks the channel to see
    // if the process is still alive, but it doesn't actually dereference it.
    // An IPC::SyncChannel is nontrivial, so we just fake it here. If you end up
    // crashing by dereferencing 1, then you'll have to make a real channel.
    channel_.reset(reinterpret_cast<IPC::SyncChannel*>(0x1));
  }
  ~RenderWidgetHostProcess() {
    // We don't want to actually delete the channel, since it's not a real
    // pointer.
    channel_.release();
    if (current_paint_buf_)
      delete current_paint_buf_;
  }

  void set_paint_msg_should_reply(bool reply) {
    paint_msg_should_reply_ = reply;
  }
  void set_paint_msg_reply_flags(int flags) {
    paint_msg_reply_flags_ = flags;
  }

  // Fills the given paint parameters with resonable default values.
  void InitPaintRectParams(ViewHostMsg_PaintRect_Params* params);

 protected:
  virtual bool WaitForPaintMsg(int render_widget_id,
                               const base::TimeDelta& max_delay,
                               IPC::Message* msg);

  TransportDIB* current_paint_buf_;

  // Set to true when WaitForPaintMsg should return a successful paint messaage
  // reply. False implies timeout.
  bool paint_msg_should_reply_;

  // Indicates the flags that should be sent with a the repaint request. This
  // only has an effect when paint_msg_should_reply_ is true.
  int paint_msg_reply_flags_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostProcess);
};

void RenderWidgetHostProcess::InitPaintRectParams(
    ViewHostMsg_PaintRect_Params* params) {
  // Create the shared backing store.
  const int w = 100, h = 100;
  const size_t pixel_size = w * h * 4;

  if (!current_paint_buf_)
    current_paint_buf_ = TransportDIB::Create(pixel_size, 0);
  params->bitmap = current_paint_buf_->id();
  params->bitmap_rect = gfx::Rect(0, 0, w, h);
  params->view_size = gfx::Size(w, h);
  params->flags = paint_msg_reply_flags_;
}

bool RenderWidgetHostProcess::WaitForPaintMsg(int render_widget_id,
                                              const base::TimeDelta& max_delay,
                                              IPC::Message* msg) {
  if (!paint_msg_should_reply_)
    return false;

  // Construct a fake paint reply.
  ViewHostMsg_PaintRect_Params params;
  InitPaintRectParams(&params);

  ViewHostMsg_PaintRect message(render_widget_id, params);
  *msg = message;
  return true;
}

// TestView --------------------------------------------------------------------

// This test view allows us to specify the size.
class TestView : public TestRenderWidgetHostView {
 public:
  TestView(RenderWidgetHost* rwh) : TestRenderWidgetHostView(rwh) {}

  // Sets the bounds returned by GetViewBounds.
  void set_bounds(const gfx::Rect& bounds) {
    bounds_ = bounds;
  }

  // RenderWidgetHostView override.
  virtual gfx::Rect GetViewBounds() const {
    return bounds_;
  }

 protected:
  gfx::Rect bounds_;
  DISALLOW_COPY_AND_ASSIGN(TestView);
};

// MockRenderWidgetHostTest ----------------------------------------------------

class MockRenderWidgetHost : public RenderWidgetHost {
 public:
  MockRenderWidgetHost(RenderProcessHost* process, int routing_id)
      : RenderWidgetHost(process, routing_id),
        unhandled_keyboard_event_called_(false),
        handle_unhandled_keyboard_event_(false),
        unhandled_keyboard_event_type_(WebInputEvent::Undefined) {
  }

  // Tests that make sure we ignore keyboard event acknowledgments to events we
  // didn't send work by making sure we didn't call UnhandledKeyboardEvent().
  bool unhandled_keyboard_event_called() const {
    return unhandled_keyboard_event_called_;
  }

  WebInputEvent::Type unhandled_keyboard_event_type() const {
    return unhandled_keyboard_event_type_;
  }

  void set_handle_unhandled_keyboard_event(bool handle) {
    handle_unhandled_keyboard_event_ = handle;
  }

 protected:
  virtual bool UnhandledKeyboardEvent(const NativeWebKeyboardEvent& event) {
    unhandled_keyboard_event_type_ = event.type;
    unhandled_keyboard_event_called_ = true;
    return handle_unhandled_keyboard_event_;
  }

 private:
  bool unhandled_keyboard_event_called_;
  bool handle_unhandled_keyboard_event_;
  WebInputEvent::Type unhandled_keyboard_event_type_;
};

// RenderWidgetHostTest --------------------------------------------------------

class RenderWidgetHostTest : public testing::Test {
 public:
  RenderWidgetHostTest() : process_(NULL) {
  }
  ~RenderWidgetHostTest() {
  }

 protected:
  // testing::Test
  void SetUp() {
    profile_.reset(new TestingProfile());
    process_ = new RenderWidgetHostProcess(profile_.get());
    host_.reset(new MockRenderWidgetHost(process_, 1));
    view_.reset(new TestView(host_.get()));
    host_->set_view(view_.get());
    host_->Init();
  }
  void TearDown() {
    view_.reset();
    host_.reset();
    process_ = NULL;
    profile_.reset();

    // Process all pending tasks to avoid leaks.
    MessageLoop::current()->RunAllPending();
  }

  void SendInputEventACK(WebInputEvent::Type type, bool processed) {
    scoped_ptr<IPC::Message> response(
        new ViewHostMsg_HandleInputEvent_ACK(0));
    response->WriteInt(type);
    response->WriteBool(processed);
    host_->OnMessageReceived(*response);
  }

  void SimulateKeyboardEvent(WebInputEvent::Type type) {
    NativeWebKeyboardEvent key_event;
    key_event.type = type;
    key_event.windowsKeyCode = base::VKEY_L;  // non-null made up value.
    host_->ForwardKeyboardEvent(key_event);
  }

  MessageLoopForUI message_loop_;

  scoped_ptr<TestingProfile> profile_;
  RenderWidgetHostProcess* process_;  // Deleted automatically by the widget.
  scoped_ptr<MockRenderWidgetHost> host_;
  scoped_ptr<TestView> view_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostTest);
};

// -----------------------------------------------------------------------------

TEST_F(RenderWidgetHostTest, Resize) {
  // The initial bounds is the empty rect, so setting it to the same thing
  // should do nothing.
  view_->set_bounds(gfx::Rect());
  host_->WasResized();
  EXPECT_FALSE(host_->resize_ack_pending_);
  EXPECT_EQ(gfx::Size(), host_->in_flight_size_);
  EXPECT_FALSE(process_->sink().GetUniqueMessageMatching(ViewMsg_Resize::ID));

  // Setting the bounds to a "real" rect should send out the notification.
  gfx::Rect original_size(0, 0, 100, 100);
  process_->sink().ClearMessages();
  view_->set_bounds(original_size);
  host_->WasResized();
  EXPECT_TRUE(host_->resize_ack_pending_);
  EXPECT_EQ(original_size.size(), host_->in_flight_size_);
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(ViewMsg_Resize::ID));

  // Send out a paint that's not a resize ack. This should not clean the
  // resize ack pending flag.
  ViewHostMsg_PaintRect_Params params;
  process_->InitPaintRectParams(&params);
  host_->OnMsgPaintRect(params);
  EXPECT_TRUE(host_->resize_ack_pending_);
  EXPECT_EQ(original_size.size(), host_->in_flight_size_);

  // Sending out a new notification should NOT send out a new IPC message since
  // a resize ACK is pending.
  gfx::Rect second_size(0, 0, 90, 90);
  process_->sink().ClearMessages();
  view_->set_bounds(second_size);
  host_->WasResized();
  EXPECT_TRUE(host_->resize_ack_pending_);
  EXPECT_EQ(original_size.size(), host_->in_flight_size_);
  EXPECT_FALSE(process_->sink().GetUniqueMessageMatching(ViewMsg_Resize::ID));

  // Send a paint that's a resize ack, but for the original_size we sent. Since
  // this isn't the second_size, the message handler should immediately send
  // a new resize message for the new size to the renderer.
  process_->sink().ClearMessages();
  params.flags = ViewHostMsg_PaintRect_Flags::IS_RESIZE_ACK;
  params.view_size = original_size.size();
  host_->OnMsgPaintRect(params);
  EXPECT_TRUE(host_->resize_ack_pending_);
  EXPECT_EQ(second_size.size(), host_->in_flight_size_);
  ASSERT_TRUE(process_->sink().GetUniqueMessageMatching(ViewMsg_Resize::ID));

  // Send the resize ack for the latest size.
  process_->sink().ClearMessages();
  params.view_size = second_size.size();
  host_->OnMsgPaintRect(params);
  EXPECT_FALSE(host_->resize_ack_pending_);
  EXPECT_EQ(gfx::Size(), host_->in_flight_size_);
  ASSERT_FALSE(process_->sink().GetFirstMessageMatching(ViewMsg_Resize::ID));

  // Now clearing the bounds should send out a notification but we shouldn't
  // expect a resize ack (since the renderer won't ack empty sizes). The message
  // should contain the new size (0x0) and not the previous one that we skipped
  process_->sink().ClearMessages();
  view_->set_bounds(gfx::Rect());
  host_->WasResized();
  EXPECT_FALSE(host_->resize_ack_pending_);
  EXPECT_EQ(gfx::Size(), host_->in_flight_size_);
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(ViewMsg_Resize::ID));

  // Send a rect that has no area but has either width or height set.
  process_->sink().ClearMessages();
  view_->set_bounds(gfx::Rect(0, 0, 0, 30));
  host_->WasResized();
  EXPECT_FALSE(host_->resize_ack_pending_);
  EXPECT_EQ(gfx::Size(0, 30), host_->in_flight_size_);
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(ViewMsg_Resize::ID));

  // Set the same size again. It should not be sent again.
  process_->sink().ClearMessages();
  host_->WasResized();
  EXPECT_FALSE(host_->resize_ack_pending_);
  EXPECT_EQ(gfx::Size(0, 30), host_->in_flight_size_);
  EXPECT_FALSE(process_->sink().GetFirstMessageMatching(ViewMsg_Resize::ID));

  // A different size should be sent again, however.
  view_->set_bounds(gfx::Rect(0, 0, 0, 31));
  host_->WasResized();
  EXPECT_FALSE(host_->resize_ack_pending_);
  EXPECT_EQ(gfx::Size(0, 31), host_->in_flight_size_);
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(ViewMsg_Resize::ID));
}

// Test for crbug.com/25097.  If a renderer crashes between a resize and the
// corresponding paint message, we must be sure to clear the resize ack logic.
TEST_F(RenderWidgetHostTest, ResizeThenCrash) {
  // Setting the bounds to a "real" rect should send out the notification.
  gfx::Rect original_size(0, 0, 100, 100);
  view_->set_bounds(original_size);
  host_->WasResized();
  EXPECT_TRUE(host_->resize_ack_pending_);
  EXPECT_EQ(original_size.size(), host_->in_flight_size_);
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(ViewMsg_Resize::ID));

  // Simulate a renderer crash before the paint message.  Ensure all the resize
  // ack logic is cleared.  Must clear the view first so it doesn't get deleted.
  host_->set_view(NULL);
  host_->RendererExited();
  EXPECT_FALSE(host_->resize_ack_pending_);
  EXPECT_EQ(gfx::Size(), host_->in_flight_size_);

  // Reset the view so we can exit the test cleanly.
  host_->set_view(view_.get());
}

// Tests setting custom background
TEST_F(RenderWidgetHostTest, Background) {
#if defined(OS_WIN) || defined(OS_LINUX)
  scoped_ptr<RenderWidgetHostView> view(
      RenderWidgetHostView::CreateViewForWidget(host_.get()));
  host_->set_view(view.get());

  // Create a checkerboard background to test with.
  gfx::Canvas canvas(4, 4, true);
  canvas.FillRectInt(SK_ColorBLACK, 0, 0, 2, 2);
  canvas.FillRectInt(SK_ColorWHITE, 2, 0, 2, 2);
  canvas.FillRectInt(SK_ColorWHITE, 0, 2, 2, 2);
  canvas.FillRectInt(SK_ColorBLACK, 2, 2, 2, 2);
  const SkBitmap& background = canvas.getDevice()->accessBitmap(false);

  // Set the background and make sure we get back a copy.
  view->SetBackground(background);
  EXPECT_EQ(4, view->background().width());
  EXPECT_EQ(4, view->background().height());
  EXPECT_EQ(background.getSize(), view->background().getSize());
  EXPECT_TRUE(0 == memcmp(background.getPixels(),
                          view->background().getPixels(),
                          background.getSize()));

#if defined(OS_WIN)
  // A message should have been dispatched telling the renderer about the new
  // background.
  const IPC::Message* set_background =
      process_->sink().GetUniqueMessageMatching(ViewMsg_SetBackground::ID);
  ASSERT_TRUE(set_background);
  Tuple1<SkBitmap> sent_background;
  ViewMsg_SetBackground::Read(set_background, &sent_background);
  EXPECT_EQ(background.getSize(), sent_background.a.getSize());
  EXPECT_TRUE(0 == memcmp(background.getPixels(),
                          sent_background.a.getPixels(),
                          background.getSize()));
#else
  // TODO(port): When custom backgrounds are implemented for other ports, this
  // test should work (assuming the background must still be copied into the
  // renderer -- if not, then maybe the test doesn't apply?).
#endif

#else
  // TODO(port): Mac does not have gfx::Canvas. Maybe we can just change this
  // test to use SkCanvas directly?
#endif

  // TODO(aa): It would be nice to factor out the painting logic so that we
  // could test that, but it appears that would mean painting everything twice
  // since windows HDC structures are opaque.
}

// Tests getting the backing store with the renderer not setting repaint ack
// flags.
TEST_F(RenderWidgetHostTest, GetBackingStore_NoRepaintAck) {
  // We don't currently have a backing store, and if the renderer doesn't send
  // one in time, we should get nothing.
  process_->set_paint_msg_should_reply(false);
  BackingStore* backing = host_->GetBackingStore(true);
  EXPECT_FALSE(backing);
  // The widget host should have sent a request for a repaint, and there should
  // be no paint ACK.
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(ViewMsg_Repaint::ID));
  EXPECT_FALSE(process_->sink().GetUniqueMessageMatching(
      ViewMsg_PaintRect_ACK::ID));

  // Allowing the renderer to reply in time should give is a backing store.
  process_->sink().ClearMessages();
  process_->set_paint_msg_should_reply(true);
  process_->set_paint_msg_reply_flags(0);
  backing = host_->GetBackingStore(true);
  EXPECT_TRUE(backing);
  // The widget host should NOT have sent a request for a repaint, since there
  // was an ACK already pending.
  EXPECT_FALSE(process_->sink().GetUniqueMessageMatching(ViewMsg_Repaint::ID));
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
      ViewMsg_PaintRect_ACK::ID));
}

// Tests getting the backing store with the renderer sending a repaint ack.
TEST_F(RenderWidgetHostTest, GetBackingStore_RepaintAck) {
  // Doing a request request with the paint message allowed should work and
  // the repaint ack should work.
  process_->set_paint_msg_should_reply(true);
  process_->set_paint_msg_reply_flags(
      ViewHostMsg_PaintRect_Flags::IS_REPAINT_ACK);
  BackingStore* backing = host_->GetBackingStore(true);
  EXPECT_TRUE(backing);
  // We still should not have sent out a repaint request since the last flags
  // didn't have the repaint ack set, and the pending flag will still be set.
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(ViewMsg_Repaint::ID));
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
      ViewMsg_PaintRect_ACK::ID));

  // Asking again for the backing store should just re-use the existing one
  // and not send any messagse.
  process_->sink().ClearMessages();
  backing = host_->GetBackingStore(true);
  EXPECT_TRUE(backing);
  EXPECT_FALSE(process_->sink().GetUniqueMessageMatching(ViewMsg_Repaint::ID));
  EXPECT_FALSE(process_->sink().GetUniqueMessageMatching(
      ViewMsg_PaintRect_ACK::ID));
}

// Test that we don't paint when we're hidden, but we still send the ACK. Most
// of the rest of the painting is tested in the GetBackingStore* ones.
TEST_F(RenderWidgetHostTest, HiddenPaint) {
  // Hide the widget, it should have sent out a message to the renderer.
  EXPECT_FALSE(host_->is_hidden_);
  host_->WasHidden();
  EXPECT_TRUE(host_->is_hidden_);
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(ViewMsg_WasHidden::ID));

  // Send it a paint as from the renderer.
  process_->sink().ClearMessages();
  ViewHostMsg_PaintRect_Params params;
  process_->InitPaintRectParams(&params);
  host_->OnMsgPaintRect(params);

  // It should have sent out the ACK.
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
      ViewMsg_PaintRect_ACK::ID));

  // Now unhide.
  process_->sink().ClearMessages();
  host_->WasRestored();
  EXPECT_FALSE(host_->is_hidden_);

  // It should have sent out a restored message with a request to paint.
  const IPC::Message* restored = process_->sink().GetUniqueMessageMatching(
      ViewMsg_WasRestored::ID);
  ASSERT_TRUE(restored);
  Tuple1<bool> needs_repaint;
  ViewMsg_WasRestored::Read(restored, &needs_repaint);
  EXPECT_TRUE(needs_repaint.a);
}

TEST_F(RenderWidgetHostTest, HandleKeyEventsWeSent) {
  // Simulate a keyboard event.
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown);

  // Make sure we sent the input event to the renderer.
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
                  ViewMsg_HandleInputEvent::ID));
  process_->sink().ClearMessages();

  // Send the simulated response from the renderer back.
  SendInputEventACK(WebInputEvent::RawKeyDown, false);

  EXPECT_TRUE(host_->unhandled_keyboard_event_called());
  EXPECT_EQ(WebInputEvent::RawKeyDown, host_->unhandled_keyboard_event_type());
}

TEST_F(RenderWidgetHostTest, IgnoreKeyEventsWeDidntSend) {
  // Send a simulated, unrequested key response. We should ignore this.
  SendInputEventACK(WebInputEvent::RawKeyDown, false);

  EXPECT_FALSE(host_->unhandled_keyboard_event_called());
}

TEST_F(RenderWidgetHostTest, IgnoreKeyEventsHandledByRenderer) {
  // Simulate a keyboard event.
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown);

  // Make sure we sent the input event to the renderer.
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
                  ViewMsg_HandleInputEvent::ID));
  process_->sink().ClearMessages();

  // Send the simulated response from the renderer back.
  SendInputEventACK(WebInputEvent::RawKeyDown, true);
  EXPECT_FALSE(host_->unhandled_keyboard_event_called());
}

TEST_F(RenderWidgetHostTest, DontSuppressNextCharEventsNoPending) {
  // Simulate a keyboard event.
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown);

  // Make sure we sent the input event to the renderer.
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
                  ViewMsg_HandleInputEvent::ID));
  process_->sink().ClearMessages();

  // Send the simulated response from the renderer back.
  SendInputEventACK(WebInputEvent::RawKeyDown, false);

  EXPECT_TRUE(host_->unhandled_keyboard_event_called());
  EXPECT_EQ(WebInputEvent::RawKeyDown, host_->unhandled_keyboard_event_type());

  // Forward the Char event.
  SimulateKeyboardEvent(WebInputEvent::Char);

  // Make sure we sent the input event to the renderer.
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
                  ViewMsg_HandleInputEvent::ID));
  process_->sink().ClearMessages();

  // Send the simulated response from the renderer back.
  SendInputEventACK(WebInputEvent::Char, false);

  EXPECT_TRUE(host_->unhandled_keyboard_event_called());
  EXPECT_EQ(WebInputEvent::Char, host_->unhandled_keyboard_event_type());

  // Forward the KeyUp event.
  SimulateKeyboardEvent(WebInputEvent::KeyUp);

  // Make sure we sent the input event to the renderer.
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
                  ViewMsg_HandleInputEvent::ID));
  process_->sink().ClearMessages();

  // Send the simulated response from the renderer back.
  SendInputEventACK(WebInputEvent::KeyUp, false);

  EXPECT_TRUE(host_->unhandled_keyboard_event_called());
  EXPECT_EQ(WebInputEvent::KeyUp, host_->unhandled_keyboard_event_type());
}

TEST_F(RenderWidgetHostTest, SuppressNextCharEventsNoPending) {
  // Simulate a keyboard event.
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown);

  // Make sure we sent the input event to the renderer.
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
                  ViewMsg_HandleInputEvent::ID));
  process_->sink().ClearMessages();

  // Simluate the situation that the browser handled the key down event.
  host_->set_handle_unhandled_keyboard_event(true);

  // Send the simulated response from the renderer back.
  SendInputEventACK(WebInputEvent::RawKeyDown, false);

  EXPECT_TRUE(host_->unhandled_keyboard_event_called());
  EXPECT_EQ(WebInputEvent::RawKeyDown, host_->unhandled_keyboard_event_type());

  // Forward the Char event.
  SimulateKeyboardEvent(WebInputEvent::Char);

  // Make sure the Char event is suppressed.
  EXPECT_EQ(0U, process_->sink().message_count());

  // Forward another Char event.
  SimulateKeyboardEvent(WebInputEvent::Char);

  // Make sure the Char event is suppressed.
  EXPECT_EQ(0U, process_->sink().message_count());

  // Forward the KeyUp event.
  SimulateKeyboardEvent(WebInputEvent::KeyUp);

  // Make sure we sent the input event to the renderer.
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
                  ViewMsg_HandleInputEvent::ID));
  process_->sink().ClearMessages();

  // The browser does not handle KeyUp events.
  host_->set_handle_unhandled_keyboard_event(false);

  // Send the simulated response from the renderer back.
  SendInputEventACK(WebInputEvent::KeyUp, false);

  EXPECT_TRUE(host_->unhandled_keyboard_event_called());
  EXPECT_EQ(WebInputEvent::KeyUp, host_->unhandled_keyboard_event_type());
}

TEST_F(RenderWidgetHostTest, DontSuppressNextCharEventsPending) {
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown);

  // Make sure we sent the input event to the renderer.
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
                  ViewMsg_HandleInputEvent::ID));
  process_->sink().ClearMessages();

  // Forward the Char event before receiving the ACK of previous KeyDown event.
  SimulateKeyboardEvent(WebInputEvent::Char);

  // Make sure the Char event is pending.
  EXPECT_EQ(0U, process_->sink().message_count());

  // Forward the KeyUp event before receiving the ACK of previous KeyDown event.
  SimulateKeyboardEvent(WebInputEvent::KeyUp);

  // Make sure the KeyUp event is pending.
  EXPECT_EQ(0U, process_->sink().message_count());

  // Send the simulated response of the KeyDown event from the renderer back.
  SendInputEventACK(WebInputEvent::RawKeyDown, false);

  EXPECT_TRUE(host_->unhandled_keyboard_event_called());
  EXPECT_EQ(WebInputEvent::RawKeyDown, host_->unhandled_keyboard_event_type());

  // Make sure both pending Char and KeyUp were sent to the renderer.
  EXPECT_EQ(2U, process_->sink().message_count());
  EXPECT_EQ(ViewMsg_HandleInputEvent::ID,
            process_->sink().GetMessageAt(0)->type());
  EXPECT_EQ(ViewMsg_HandleInputEvent::ID,
            process_->sink().GetMessageAt(1)->type());
  process_->sink().ClearMessages();

  // Send the simulated response from the renderer back.
  SendInputEventACK(WebInputEvent::Char, false);

  EXPECT_TRUE(host_->unhandled_keyboard_event_called());
  EXPECT_EQ(WebInputEvent::Char, host_->unhandled_keyboard_event_type());

  // Send the simulated response from the renderer back.
  SendInputEventACK(WebInputEvent::KeyUp, false);

  EXPECT_TRUE(host_->unhandled_keyboard_event_called());
  EXPECT_EQ(WebInputEvent::KeyUp, host_->unhandled_keyboard_event_type());
}

TEST_F(RenderWidgetHostTest, SuppressNextCharEventsPending) {
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown);

  // Make sure we sent the KeyDown event to the renderer.
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
                  ViewMsg_HandleInputEvent::ID));
  process_->sink().ClearMessages();

  // Forward the Char event before receiving the ACK of previous KeyDown event.
  SimulateKeyboardEvent(WebInputEvent::Char);

  // Make sure the Char event is pending.
  EXPECT_EQ(0U, process_->sink().message_count());

  // Forward another Char event before receiving the ACK of previous KeyDown
  // event.
  SimulateKeyboardEvent(WebInputEvent::Char);

  // Make sure the Char event is pending.
  EXPECT_EQ(0U, process_->sink().message_count());

  // Forward the KeyUp event before receiving the ACK of previous KeyDown event.
  SimulateKeyboardEvent(WebInputEvent::KeyUp);

  // Make sure the KeyUp event is pending.
  EXPECT_EQ(0U, process_->sink().message_count());

  // Simluate the situation that the browser handled the key down event.
  host_->set_handle_unhandled_keyboard_event(true);

  // Send the simulated response of the KeyDown event from the renderer back.
  SendInputEventACK(WebInputEvent::RawKeyDown, false);

  EXPECT_TRUE(host_->unhandled_keyboard_event_called());
  EXPECT_EQ(WebInputEvent::RawKeyDown, host_->unhandled_keyboard_event_type());

  // Make sure only pending KeyUp was sent to the renderer.
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_EQ(ViewMsg_HandleInputEvent::ID,
            process_->sink().GetMessageAt(0)->type());
  process_->sink().ClearMessages();

  // Send the simulated response from the renderer back.
  SendInputEventACK(WebInputEvent::KeyUp, false);

  EXPECT_TRUE(host_->unhandled_keyboard_event_called());
  EXPECT_EQ(WebInputEvent::KeyUp, host_->unhandled_keyboard_event_type());
}

TEST_F(RenderWidgetHostTest, ManyKeyEventsPending) {
  process_->sink().ClearMessages();

  for (int i = 0; i < 10; ++i) {
    // Forward a KeyDown event.
    SimulateKeyboardEvent(WebInputEvent::RawKeyDown);

    // Forward a Char event before receiving the ACK of previous KeyDown event.
    SimulateKeyboardEvent(WebInputEvent::Char);

    // Forward a KeyUp event before receiving the ACK of previous KeyDown event.
    SimulateKeyboardEvent(WebInputEvent::KeyUp);
  }

  // Make sure only the first KeyDown event was sent to the renderer. All others
  // are pending.
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
                  ViewMsg_HandleInputEvent::ID));
  process_->sink().ClearMessages();

  for (int i = 0; i < 10; ++i) {
    // Send the simulated response of the KeyDown event from the renderer back.
    SendInputEventACK(WebInputEvent::RawKeyDown, false);
    EXPECT_TRUE(host_->unhandled_keyboard_event_called());
    EXPECT_EQ(WebInputEvent::RawKeyDown,
              host_->unhandled_keyboard_event_type());

    // Make sure the following pending Char, KeyUp and KeyDown event were sent
    // to the renderer.
    if (i < 9)
      EXPECT_EQ(3U, process_->sink().message_count());
    else
      EXPECT_EQ(2U, process_->sink().message_count());
    process_->sink().ClearMessages();

    // Send the simulated response of the Char event from the renderer back.
    SendInputEventACK(WebInputEvent::Char, false);
    EXPECT_TRUE(host_->unhandled_keyboard_event_called());
    EXPECT_EQ(WebInputEvent::Char, host_->unhandled_keyboard_event_type());

    // Send the simulated response of the KeyUp event from the renderer back.
    SendInputEventACK(WebInputEvent::KeyUp, false);
    EXPECT_TRUE(host_->unhandled_keyboard_event_called());
    EXPECT_EQ(WebInputEvent::KeyUp, host_->unhandled_keyboard_event_type());
  }
}
