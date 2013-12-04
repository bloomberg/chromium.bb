// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/renderer_date_time_picker.h"

#include "base/strings/string_util.h"
#include "content/common/view_messages.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/WebKit/public/web/WebDateTimeChooserCompletion.h"
#include "third_party/WebKit/public/web/WebDateTimeChooserParams.h"
#include "third_party/WebKit/public/web/WebDateTimeInputType.h"
#include "ui/base/ime/text_input_type.h"

using blink::WebString;

namespace content {

COMPILE_ASSERT(int(blink::WebTextInputTypeDate) == \
               int(ui::TEXT_INPUT_TYPE_DATE), mismatching_enum);
COMPILE_ASSERT(int(blink::WebTextInputTypeDateTime) == \
               int(ui::TEXT_INPUT_TYPE_DATE_TIME), mismatching_enum);
COMPILE_ASSERT(int(blink::WebTextInputTypeDateTimeLocal) == \
               int(ui::TEXT_INPUT_TYPE_DATE_TIME_LOCAL), mismatching_enum);
COMPILE_ASSERT(int(blink::WebTextInputTypeMonth) == \
               int(ui::TEXT_INPUT_TYPE_MONTH), mismatching_enum);
COMPILE_ASSERT(int(blink::WebTextInputTypeTime) == \
               int(ui::TEXT_INPUT_TYPE_TIME), mismatching_enum);
COMPILE_ASSERT(int(blink::WebTextInputTypeWeek) == \
               int(ui::TEXT_INPUT_TYPE_WEEK), mismatching_enum);
COMPILE_ASSERT(int(blink::WebTextInputTypeDateTimeField) == \
               int(ui::TEXT_INPUT_TYPE_DATE_TIME_FIELD), mismatching_enums);

RendererDateTimePicker::RendererDateTimePicker(
    RenderViewImpl* sender,
    const blink::WebDateTimeChooserParams& params,
    blink::WebDateTimeChooserCompletion* completion)
    : RenderViewObserver(sender),
      chooser_params_(params),
      chooser_completion_(completion) {
}

RendererDateTimePicker::~RendererDateTimePicker() {
}

bool RendererDateTimePicker::Open() {
  ViewHostMsg_DateTimeDialogValue_Params message;
  message.dialog_type = static_cast<ui::TextInputType>(chooser_params_.type);
  message.dialog_value = chooser_params_.doubleValue;
  message.minimum = chooser_params_.minimum;
  message.maximum = chooser_params_.maximum;
  message.step = chooser_params_.step;
  Send(new ViewHostMsg_OpenDateTimeDialog(routing_id(), message));
  return true;
}

bool RendererDateTimePicker::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RendererDateTimePicker, message)
    IPC_MESSAGE_HANDLER(ViewMsg_ReplaceDateTime, OnReplaceDateTime)
    IPC_MESSAGE_HANDLER(ViewMsg_CancelDateTimeDialog, OnCancel)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void RendererDateTimePicker::OnReplaceDateTime(double value) {
  if (chooser_completion_)
    chooser_completion_->didChooseValue(value);
#if defined(OS_ANDROID)
  static_cast<RenderViewImpl*>(render_view())->DismissDateTimeDialog();
#endif
}

void RendererDateTimePicker::OnCancel() {
  if (chooser_completion_)
    chooser_completion_->didCancelChooser();
#if defined(OS_ANDROID)
  static_cast<RenderViewImpl*>(render_view())->DismissDateTimeDialog();
#endif
}

}  // namespace content
