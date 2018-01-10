// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "content/common/resize_params.h"
#include "content/public/renderer/render_frame_visitor.h"
#include "content/public/test/render_view_test.h"
#include "content/renderer/render_frame_proxy.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/render_widget.h"
#include "third_party/WebKit/public/web/WebFrameWidget.h"
#include "third_party/WebKit/public/web/WebInputMethodController.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebRange.h"
#include "ui/base/ime/text_input_type.h"

namespace content {

class RenderWidgetTest : public RenderViewTest {
 protected:
  RenderWidget* widget() {
    return static_cast<RenderViewImpl*>(view_)->GetWidget();
  }

  void OnResize(const ResizeParams& params) {
    widget()->OnResize(params);
  }

  void GetCompositionRange(gfx::Range* range) {
    widget()->GetCompositionRange(range);
  }

  bool next_paint_is_resize_ack() {
    return widget()->next_paint_is_resize_ack();
  }

  blink::WebInputMethodController* GetInputMethodController() {
    return widget()->GetInputMethodController();
  }

  void CommitText(std::string text) {
    widget()->OnImeCommitText(base::UTF8ToUTF16(text),
                              std::vector<blink::WebImeTextSpan>(),
                              gfx::Range::InvalidRange(), 0);
  }

  ui::TextInputType GetTextInputType() { return widget()->GetTextInputType(); }

  void SetFocus(bool focused) { widget()->OnSetFocus(focused); }
};

TEST_F(RenderWidgetTest, OnResize) {
  // The initial bounds is empty, so setting it to the same thing should do
  // nothing.
  viz::LocalSurfaceId local_surface_id;
  viz::ParentLocalSurfaceIdAllocator local_surface_id_allocator;
  ResizeParams resize_params;
  resize_params.screen_info = ScreenInfo();
  resize_params.new_size = gfx::Size();
  resize_params.physical_backing_size = gfx::Size();
  resize_params.top_controls_height = 0.f;
  resize_params.browser_controls_shrink_blink_size = false;
  resize_params.is_fullscreen_granted = false;
  resize_params.needs_resize_ack = false;
  local_surface_id = local_surface_id_allocator.GenerateId();
  resize_params.local_surface_id = local_surface_id;
  OnResize(resize_params);
  EXPECT_EQ(resize_params.needs_resize_ack, next_paint_is_resize_ack());

  // Setting empty physical backing size should not send the ack.
  resize_params.new_size = gfx::Size(10, 10);
  OnResize(resize_params);
  EXPECT_EQ(resize_params.needs_resize_ack, next_paint_is_resize_ack());

  // Setting the bounds to a "real" rect should send the ack.
  render_thread_->sink().ClearMessages();
  gfx::Size size(100, 100);
  resize_params.new_size = size;
  resize_params.physical_backing_size = size;
  resize_params.needs_resize_ack = true;
  OnResize(resize_params);
  EXPECT_EQ(resize_params.needs_resize_ack, next_paint_is_resize_ack());

  // Clear the flag.
  widget()->DidReceiveCompositorFrameAck();

  // Setting the same size again should not send the ack.
  resize_params.needs_resize_ack = false;
  OnResize(resize_params);
  EXPECT_EQ(resize_params.needs_resize_ack, next_paint_is_resize_ack());

  // Resetting the rect to empty should not send the ack.
  resize_params.new_size = gfx::Size();
  resize_params.physical_backing_size = gfx::Size();
  OnResize(resize_params);
  EXPECT_EQ(resize_params.needs_resize_ack, next_paint_is_resize_ack());

  // Changing the screen info should not send the ack.
  resize_params.screen_info.orientation_angle = 90;
  OnResize(resize_params);
  EXPECT_EQ(resize_params.needs_resize_ack, next_paint_is_resize_ack());

  resize_params.screen_info.orientation_type =
      SCREEN_ORIENTATION_VALUES_PORTRAIT_PRIMARY;
  OnResize(resize_params);
  EXPECT_EQ(resize_params.needs_resize_ack, next_paint_is_resize_ack());
}

class RenderWidgetInitialSizeTest : public RenderWidgetTest {
 public:
  RenderWidgetInitialSizeTest() : RenderWidgetTest(), initial_size_(200, 100) {
    local_surface_id_ = local_surface_id_allocator_.GenerateId();
  }

 protected:
  std::unique_ptr<ResizeParams> InitialSizeParams() override {
    std::unique_ptr<ResizeParams> initial_size_params(new ResizeParams());
    initial_size_params->new_size = initial_size_;
    initial_size_params->physical_backing_size = initial_size_;
    initial_size_params->needs_resize_ack = true;
    initial_size_params->local_surface_id = local_surface_id_;
    return initial_size_params;
  }

  gfx::Size initial_size_;
  viz::LocalSurfaceId local_surface_id_;
  viz::ParentLocalSurfaceIdAllocator local_surface_id_allocator_;
};

TEST_F(RenderWidgetInitialSizeTest, InitialSize) {
  EXPECT_EQ(initial_size_, widget()->size());
  EXPECT_EQ(initial_size_, gfx::Size(widget()->GetWebWidget()->Size()));
  EXPECT_TRUE(next_paint_is_resize_ack());
}

TEST_F(RenderWidgetTest, HitTestAPI) {
  LoadHTML(
      "<body style='padding: 0px; margin: 0px'>"
      "<div style='background: green; padding: 100px; margin: 0px;'>"
      "<iframe style='width: 200px; height: 100px;'"
      "srcdoc='<body style=\"margin: 0px; height: 100px; width: 200px;\">"
      "</body>'></iframe><div></body>");
  viz::FrameSinkId main_frame_sink_id =
      widget()->GetFrameSinkIdAtPoint(gfx::Point(10, 10));
  EXPECT_EQ(static_cast<uint32_t>(widget()->routing_id()),
            main_frame_sink_id.sink_id());
  EXPECT_EQ(static_cast<uint32_t>(RenderThreadImpl::Get()->GetClientId()),
            main_frame_sink_id.client_id());

  // Targeting a child frame should also return the FrameSinkId for the main
  // widget.
  viz::FrameSinkId frame_sink_id =
      widget()->GetFrameSinkIdAtPoint(gfx::Point(150, 150));
  EXPECT_EQ(static_cast<uint32_t>(widget()->routing_id()),
            frame_sink_id.sink_id());
  EXPECT_EQ(main_frame_sink_id.client_id(), frame_sink_id.client_id());
}

TEST_F(RenderWidgetTest, GetCompositionRangeValidComposition) {
  LoadHTML(
      "<div contenteditable>EDITABLE</div>"
      "<script> document.querySelector('div').focus(); </script>");
  blink::WebVector<blink::WebImeTextSpan> empty_ime_text_spans;
  DCHECK(widget()->GetInputMethodController());
  widget()->GetInputMethodController()->SetComposition(
      "hello", empty_ime_text_spans, blink::WebRange(), 3, 3);
  gfx::Range range;
  GetCompositionRange(&range);
  EXPECT_TRUE(range.IsValid());
  EXPECT_EQ(0U, range.start());
  EXPECT_EQ(5U, range.end());
}

TEST_F(RenderWidgetTest, GetCompositionRangeForSelection) {
  LoadHTML(
      "<div>NOT EDITABLE</div>"
      "<script> document.execCommand('selectAll'); </script>");
  gfx::Range range;
  GetCompositionRange(&range);
  // Selection range should not be treated as composition range.
  EXPECT_FALSE(range.IsValid());
}

TEST_F(RenderWidgetTest, GetCompositionRangeInvalid) {
  LoadHTML("<div>NOT EDITABLE</div>");
  gfx::Range range;
  GetCompositionRange(&range);
  // If this test ever starts failing, one likely outcome is that WebRange
  // and gfx::Range::InvalidRange are no longer expressed in the same
  // values of start/end.
  EXPECT_FALSE(range.IsValid());
}

// This test verifies that WebInputMethodController always exists as long as
// there is a focused frame inside the page, but, IME events are only executed
// if there is also page focus.
TEST_F(RenderWidgetTest, PageFocusIme) {
  LoadHTML(
      "<input/>"
      " <script> document.querySelector('input').focus(); </script>");

  // Give initial focus to the widget.
  SetFocus(true);

  // We must have an active WebInputMethodController.
  EXPECT_TRUE(GetInputMethodController());

  // Verify the text input type.
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT, GetTextInputType());

  // Commit some text.
  std::string text = "hello";
  CommitText(text);

  // The text should be committed since there is page focus in the beginning.
  EXPECT_EQ(text, GetInputMethodController()->TextInputInfo().value.Utf8());

  // Drop focus.
  SetFocus(false);

  // We must still have an active WebInputMethodController.
  EXPECT_TRUE(GetInputMethodController());

  // The text input type should not change.
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT, GetTextInputType());

  // Commit the text again.
  text = " world";
  CommitText(text);

  // This time is should not work since |m_imeAcceptEvents| is not set.
  EXPECT_EQ("hello", GetInputMethodController()->TextInputInfo().value.Utf8());

  // Now give focus back again and commit text.
  SetFocus(true);
  CommitText(text);
  EXPECT_EQ("hello world",
            GetInputMethodController()->TextInputInfo().value.Utf8());
}

}  // namespace content
