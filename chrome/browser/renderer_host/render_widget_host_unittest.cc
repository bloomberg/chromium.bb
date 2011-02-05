// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/shared_memory.h"
#include "base/timer.h"
#include "build/build_config.h"
#include "chrome/browser/renderer_host/backing_store.h"
#include "chrome/browser/renderer_host/test/test_render_view_host.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas_skia.h"

using base::TimeDelta;

using WebKit::WebInputEvent;
using WebKit::WebMouseWheelEvent;

namespace gfx {
class Size;
}

// RenderWidgetHostProcess -----------------------------------------------------

class RenderWidgetHostProcess : public MockRenderProcessHost {
 public:
  explicit RenderWidgetHostProcess(Profile* profile)
      : MockRenderProcessHost(profile),
        current_update_buf_(NULL),
        update_msg_should_reply_(false),
        update_msg_reply_flags_(0) {
    // DANGER! This is a hack. The RenderWidgetHost checks the channel to see
    // if the process is still alive, but it doesn't actually dereference it.
    // An IPC::SyncChannel is nontrivial, so we just fake it here. If you end up
    // crashing by dereferencing 1, then you'll have to make a real channel.
    channel_.reset(reinterpret_cast<IPC::SyncChannel*>(0x1));
  }
  ~RenderWidgetHostProcess() {
    // We don't want to actually delete the channel, since it's not a real
    // pointer.
    ignore_result(channel_.release());
    delete current_update_buf_;
  }

  void set_update_msg_should_reply(bool reply) {
    update_msg_should_reply_ = reply;
  }
  void set_update_msg_reply_flags(int flags) {
    update_msg_reply_flags_ = flags;
  }

  // Fills the given update parameters with resonable default values.
  void InitUpdateRectParams(ViewHostMsg_UpdateRect_Params* params);

 protected:
  virtual bool WaitForUpdateMsg(int render_widget_id,
                                const base::TimeDelta& max_delay,
                                IPC::Message* msg);

  TransportDIB* current_update_buf_;

  // Set to true when WaitForUpdateMsg should return a successful update message
  // reply. False implies timeout.
  bool update_msg_should_reply_;

  // Indicates the flags that should be sent with a the repaint request. This
  // only has an effect when update_msg_should_reply_ is true.
  int update_msg_reply_flags_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostProcess);
};

void RenderWidgetHostProcess::InitUpdateRectParams(
    ViewHostMsg_UpdateRect_Params* params) {
  // Create the shared backing store.
  const int w = 100, h = 100;
  const size_t pixel_size = w * h * 4;

  if (!current_update_buf_)
    current_update_buf_ = TransportDIB::Create(pixel_size, 0);
  params->bitmap = current_update_buf_->id();
  params->bitmap_rect = gfx::Rect(0, 0, w, h);
  params->dx = 0;
  params->dy = 0;
  params->copy_rects.push_back(params->bitmap_rect);
  params->view_size = gfx::Size(w, h);
  params->flags = update_msg_reply_flags_;
}

bool RenderWidgetHostProcess::WaitForUpdateMsg(int render_widget_id,
                                               const base::TimeDelta& max_delay,
                                               IPC::Message* msg) {
  if (!update_msg_should_reply_)
    return false;

  // Construct a fake update reply.
  ViewHostMsg_UpdateRect_Params params;
  InitUpdateRectParams(&params);

  ViewHostMsg_UpdateRect message(render_widget_id, params);
  *msg = message;
  return true;
}

// TestView --------------------------------------------------------------------

// This test view allows us to specify the size.
class TestView : public TestRenderWidgetHostView {
 public:
  explicit TestView(RenderWidgetHost* rwh) : TestRenderWidgetHostView(rwh) {}

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

// MockRenderWidgetHost ----------------------------------------------------

class MockRenderWidgetHost : public RenderWidgetHost {
 public:
  MockRenderWidgetHost(RenderProcessHost* process, int routing_id)
      : RenderWidgetHost(process, routing_id),
        prehandle_keyboard_event_(false),
        prehandle_keyboard_event_called_(false),
        prehandle_keyboard_event_type_(WebInputEvent::Undefined),
        unhandled_keyboard_event_called_(false),
        unhandled_keyboard_event_type_(WebInputEvent::Undefined),
        unresponsive_timer_fired_(false) {
  }

  // Tests that make sure we ignore keyboard event acknowledgments to events we
  // didn't send work by making sure we didn't call UnhandledKeyboardEvent().
  bool unhandled_keyboard_event_called() const {
    return unhandled_keyboard_event_called_;
  }

  WebInputEvent::Type unhandled_keyboard_event_type() const {
    return unhandled_keyboard_event_type_;
  }

  bool prehandle_keyboard_event_called() const {
    return prehandle_keyboard_event_called_;
  }

  WebInputEvent::Type prehandle_keyboard_event_type() const {
    return prehandle_keyboard_event_type_;
  }

  void set_prehandle_keyboard_event(bool handle) {
    prehandle_keyboard_event_ = handle;
  }

  bool unresponsive_timer_fired() const {
    return unresponsive_timer_fired_;
  }

 protected:
  virtual bool PreHandleKeyboardEvent(const NativeWebKeyboardEvent& event,
                                      bool* is_keyboard_shortcut) {
    prehandle_keyboard_event_type_ = event.type;
    prehandle_keyboard_event_called_ = true;
    return prehandle_keyboard_event_;
  }

  virtual void UnhandledKeyboardEvent(const NativeWebKeyboardEvent& event) {
    unhandled_keyboard_event_type_ = event.type;
    unhandled_keyboard_event_called_ = true;
  }

  virtual void NotifyRendererUnresponsive() {
    unresponsive_timer_fired_ = true;
  }

 private:
  bool prehandle_keyboard_event_;
  bool prehandle_keyboard_event_called_;
  WebInputEvent::Type prehandle_keyboard_event_type_;

  bool unhandled_keyboard_event_called_;
  WebInputEvent::Type unhandled_keyboard_event_type_;

  bool unresponsive_timer_fired_;
};

// MockPaintingObserver --------------------------------------------------------

class MockPaintingObserver : public NotificationObserver {
 public:
  void WidgetDidReceivePaintAtSizeAck(RenderWidgetHost* host,
                                      int tag,
                                      const gfx::Size& size) {
    host_ = reinterpret_cast<MockRenderWidgetHost*>(host);
    tag_ = tag;
    size_ = size;
  }

  void Observe(NotificationType type,
               const NotificationSource& source,
               const NotificationDetails& details) {
    if (type ==
        NotificationType::RENDER_WIDGET_HOST_DID_RECEIVE_PAINT_AT_SIZE_ACK) {
      RenderWidgetHost::PaintAtSizeAckDetails* size_ack_details =
          Details<RenderWidgetHost::PaintAtSizeAckDetails>(details).ptr();
      WidgetDidReceivePaintAtSizeAck(
          Source<RenderWidgetHost>(source).ptr(),
          size_ack_details->tag,
          size_ack_details->size);
    }
  }

  MockRenderWidgetHost* host() const { return host_; }
  int tag() const { return tag_; }
  gfx::Size size() const { return size_; }

 private:
  MockRenderWidgetHost* host_;
  int tag_;
  gfx::Size size_;
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
    key_event.windowsKeyCode = ui::VKEY_L;  // non-null made up value.
    host_->ForwardKeyboardEvent(key_event);
  }

  void SimulateWheelEvent(float dX, float dY, int modifiers) {
    WebMouseWheelEvent wheel_event;
    wheel_event.type = WebInputEvent::MouseWheel;
    wheel_event.deltaX = dX;
    wheel_event.deltaY = dY;
    wheel_event.modifiers = modifiers;
    host_->ForwardWheelEvent(wheel_event);
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

  // Send out a update that's not a resize ack. This should not clean the
  // resize ack pending flag.
  ViewHostMsg_UpdateRect_Params params;
  process_->InitUpdateRectParams(&params);
  host_->OnMsgUpdateRect(params);
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

  // Send a update that's a resize ack, but for the original_size we sent. Since
  // this isn't the second_size, the message handler should immediately send
  // a new resize message for the new size to the renderer.
  process_->sink().ClearMessages();
  params.flags = ViewHostMsg_UpdateRect_Flags::IS_RESIZE_ACK;
  params.view_size = original_size.size();
  host_->OnMsgUpdateRect(params);
  EXPECT_TRUE(host_->resize_ack_pending_);
  EXPECT_EQ(second_size.size(), host_->in_flight_size_);
  ASSERT_TRUE(process_->sink().GetUniqueMessageMatching(ViewMsg_Resize::ID));

  // Send the resize ack for the latest size.
  process_->sink().ClearMessages();
  params.view_size = second_size.size();
  host_->OnMsgUpdateRect(params);
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
  EXPECT_EQ(gfx::Size(), host_->current_size_);
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(ViewMsg_Resize::ID));

  // Send a rect that has no area but has either width or height set.
  // since we do not expect ACK, current_size_ should be updated right away.
  process_->sink().ClearMessages();
  view_->set_bounds(gfx::Rect(0, 0, 0, 30));
  host_->WasResized();
  EXPECT_FALSE(host_->resize_ack_pending_);
  EXPECT_EQ(gfx::Size(), host_->in_flight_size_);
  EXPECT_EQ(gfx::Size(0, 30), host_->current_size_);
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(ViewMsg_Resize::ID));

  // Set the same size again. It should not be sent again.
  process_->sink().ClearMessages();
  host_->WasResized();
  EXPECT_FALSE(host_->resize_ack_pending_);
  EXPECT_EQ(gfx::Size(), host_->in_flight_size_);
  EXPECT_EQ(gfx::Size(0, 30), host_->current_size_);
  EXPECT_FALSE(process_->sink().GetFirstMessageMatching(ViewMsg_Resize::ID));

  // A different size should be sent again, however.
  view_->set_bounds(gfx::Rect(0, 0, 0, 31));
  host_->WasResized();
  EXPECT_FALSE(host_->resize_ack_pending_);
  EXPECT_EQ(gfx::Size(), host_->in_flight_size_);
  EXPECT_EQ(gfx::Size(0, 31), host_->current_size_);
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(ViewMsg_Resize::ID));
}

// Test for crbug.com/25097.  If a renderer crashes between a resize and the
// corresponding update message, we must be sure to clear the resize ack logic.
TEST_F(RenderWidgetHostTest, ResizeThenCrash) {
  // Setting the bounds to a "real" rect should send out the notification.
  gfx::Rect original_size(0, 0, 100, 100);
  view_->set_bounds(original_size);
  host_->WasResized();
  EXPECT_TRUE(host_->resize_ack_pending_);
  EXPECT_EQ(original_size.size(), host_->in_flight_size_);
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(ViewMsg_Resize::ID));

  // Simulate a renderer crash before the update message.  Ensure all the
  // resize ack logic is cleared.  Must clear the view first so it doesn't get
  // deleted.
  host_->set_view(NULL);
  host_->RendererExited(base::TERMINATION_STATUS_PROCESS_CRASHED, -1);
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
  gfx::CanvasSkia canvas(4, 4, true);
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
  process_->set_update_msg_should_reply(false);
  BackingStore* backing = host_->GetBackingStore(true);
  EXPECT_FALSE(backing);
  // The widget host should have sent a request for a repaint, and there should
  // be no paint ACK.
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(ViewMsg_Repaint::ID));
  EXPECT_FALSE(process_->sink().GetUniqueMessageMatching(
      ViewMsg_UpdateRect_ACK::ID));

  // Allowing the renderer to reply in time should give is a backing store.
  process_->sink().ClearMessages();
  process_->set_update_msg_should_reply(true);
  process_->set_update_msg_reply_flags(0);
  backing = host_->GetBackingStore(true);
  EXPECT_TRUE(backing);
  // The widget host should NOT have sent a request for a repaint, since there
  // was an ACK already pending.
  EXPECT_FALSE(process_->sink().GetUniqueMessageMatching(ViewMsg_Repaint::ID));
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
      ViewMsg_UpdateRect_ACK::ID));
}

// Tests getting the backing store with the renderer sending a repaint ack.
TEST_F(RenderWidgetHostTest, GetBackingStore_RepaintAck) {
  // Doing a request request with the update message allowed should work and
  // the repaint ack should work.
  process_->set_update_msg_should_reply(true);
  process_->set_update_msg_reply_flags(
      ViewHostMsg_UpdateRect_Flags::IS_REPAINT_ACK);
  BackingStore* backing = host_->GetBackingStore(true);
  EXPECT_TRUE(backing);
  // We still should not have sent out a repaint request since the last flags
  // didn't have the repaint ack set, and the pending flag will still be set.
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(ViewMsg_Repaint::ID));
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
      ViewMsg_UpdateRect_ACK::ID));

  // Asking again for the backing store should just re-use the existing one
  // and not send any messagse.
  process_->sink().ClearMessages();
  backing = host_->GetBackingStore(true);
  EXPECT_TRUE(backing);
  EXPECT_FALSE(process_->sink().GetUniqueMessageMatching(ViewMsg_Repaint::ID));
  EXPECT_FALSE(process_->sink().GetUniqueMessageMatching(
      ViewMsg_UpdateRect_ACK::ID));
}

// Test that we don't paint when we're hidden, but we still send the ACK. Most
// of the rest of the painting is tested in the GetBackingStore* ones.
TEST_F(RenderWidgetHostTest, HiddenPaint) {
  // Hide the widget, it should have sent out a message to the renderer.
  EXPECT_FALSE(host_->is_hidden_);
  host_->WasHidden();
  EXPECT_TRUE(host_->is_hidden_);
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(ViewMsg_WasHidden::ID));

  // Send it an update as from the renderer.
  process_->sink().ClearMessages();
  ViewHostMsg_UpdateRect_Params params;
  process_->InitUpdateRectParams(&params);
  host_->OnMsgUpdateRect(params);

  // It should have sent out the ACK.
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
      ViewMsg_UpdateRect_ACK::ID));

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

TEST_F(RenderWidgetHostTest, PaintAtSize) {
  const int kPaintAtSizeTag = 42;
  host_->PaintAtSize(TransportDIB::GetFakeHandleForTest(), kPaintAtSizeTag,
                     gfx::Size(40, 60), gfx::Size(20, 30));
  EXPECT_TRUE(
      process_->sink().GetUniqueMessageMatching(ViewMsg_PaintAtSize::ID));

  NotificationRegistrar registrar;
  MockPaintingObserver observer;
  registrar.Add(
      &observer,
      NotificationType::RENDER_WIDGET_HOST_DID_RECEIVE_PAINT_AT_SIZE_ACK,
      Source<RenderWidgetHost>(host_.get()));

  host_->OnMsgPaintAtSizeAck(kPaintAtSizeTag, gfx::Size(20, 30));
  EXPECT_EQ(host_.get(), observer.host());
  EXPECT_EQ(kPaintAtSizeTag, observer.tag());
  EXPECT_EQ(20, observer.size().width());
  EXPECT_EQ(30, observer.size().height());
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

TEST_F(RenderWidgetHostTest, PreHandleRawKeyDownEvent) {
  // Simluate the situation that the browser handled the key down event during
  // pre-handle phrase.
  host_->set_prehandle_keyboard_event(true);
  process_->sink().ClearMessages();

  // Simulate a keyboard event.
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown);

  EXPECT_TRUE(host_->prehandle_keyboard_event_called());
  EXPECT_EQ(WebInputEvent::RawKeyDown, host_->prehandle_keyboard_event_type());

  // Make sure the RawKeyDown event is not sent to the renderer.
  EXPECT_EQ(0U, process_->sink().message_count());

  // The browser won't pre-handle a Char event.
  host_->set_prehandle_keyboard_event(false);

  // Forward the Char event.
  SimulateKeyboardEvent(WebInputEvent::Char);

  // Make sure the Char event is suppressed.
  EXPECT_EQ(0U, process_->sink().message_count());

  // Forward the KeyUp event.
  SimulateKeyboardEvent(WebInputEvent::KeyUp);

  // Make sure only KeyUp was sent to the renderer.
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_EQ(ViewMsg_HandleInputEvent::ID,
            process_->sink().GetMessageAt(0)->type());
  process_->sink().ClearMessages();

  // Send the simulated response from the renderer back.
  SendInputEventACK(WebInputEvent::KeyUp, false);

  EXPECT_TRUE(host_->unhandled_keyboard_event_called());
  EXPECT_EQ(WebInputEvent::KeyUp, host_->unhandled_keyboard_event_type());
}

TEST_F(RenderWidgetHostTest, CoalescesWheelEvents) {
  process_->sink().ClearMessages();

  // Simulate wheel events.
  SimulateWheelEvent(0, -5, 0);  // sent directly
  SimulateWheelEvent(0, -10, 0);  // enqueued
  SimulateWheelEvent(8, -6, 0);  // coalesced into previous event
  SimulateWheelEvent(9, -7, 1);  // enqueued, different modifiers

  // Check that only the first event was sent.
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
                  ViewMsg_HandleInputEvent::ID));
  process_->sink().ClearMessages();

  // Check that the ACK sends the second message.
  SendInputEventACK(WebInputEvent::MouseWheel, true);
  // The coalesced events can queue up a delayed ack
  // so that additional input events can be processed before
  // we turn off coalescing.
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
                  ViewMsg_HandleInputEvent::ID));
  process_->sink().ClearMessages();

  // One more time.
  SendInputEventACK(WebInputEvent::MouseWheel, true);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(1U, process_->sink().message_count());
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
                  ViewMsg_HandleInputEvent::ID));
  process_->sink().ClearMessages();

  // After the final ack, the queue should be empty.
  SendInputEventACK(WebInputEvent::MouseWheel, true);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(0U, process_->sink().message_count());
}

// Test that the hang monitor timer expires properly if a new timer is started
// while one is in progress (see crbug.com/11007).
TEST_F(RenderWidgetHostTest, DontPostponeHangMonitorTimeout) {
  // Start with a short timeout.
  host_->StartHangMonitorTimeout(TimeDelta::FromMilliseconds(10));

  // Immediately try to add a long 30 second timeout.
  EXPECT_FALSE(host_->unresponsive_timer_fired());
  host_->StartHangMonitorTimeout(TimeDelta::FromMilliseconds(30000));

  // Wait long enough for first timeout and see if it fired.
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
                                          new MessageLoop::QuitTask(), 10);
  MessageLoop::current()->Run();
  EXPECT_TRUE(host_->unresponsive_timer_fired());
}

// Test that the hang monitor timer expires properly if it is started, stopped,
// and then started again.
TEST_F(RenderWidgetHostTest, StopAndStartHangMonitorTimeout) {
  // Start with a short timeout, then stop it.
  host_->StartHangMonitorTimeout(TimeDelta::FromMilliseconds(10));
  host_->StopHangMonitorTimeout();

  // Start it again to ensure it still works.
  EXPECT_FALSE(host_->unresponsive_timer_fired());
  host_->StartHangMonitorTimeout(TimeDelta::FromMilliseconds(10));

  // Wait long enough for first timeout and see if it fired.
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
                                          new MessageLoop::QuitTask(), 10);
  MessageLoop::current()->Run();
  EXPECT_TRUE(host_->unresponsive_timer_fired());
}
