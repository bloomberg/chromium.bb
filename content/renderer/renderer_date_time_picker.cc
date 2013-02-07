// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/renderer_date_time_picker.h"

#include "base/string_util.h"
#include "content/common/view_messages.h"
#include "content/renderer/date_time_formatter.h"
#include "content/renderer/render_view_impl.h"

#include "third_party/WebKit/Source/WebKit/chromium/public/WebDateTimeChooserCompletion.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDateTimeChooserParams.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDateTimeInputType.h"

using WebKit::WebString;

namespace content {

RendererDateTimePicker::RendererDateTimePicker(
    RenderViewImpl* sender,
    const WebKit::WebDateTimeChooserParams& params,
    WebKit::WebDateTimeChooserCompletion* completion)
    : RenderViewObserver(sender),
      chooser_params_(params),
      chooser_completion_(completion){
}

RendererDateTimePicker::~RendererDateTimePicker() {
}

bool RendererDateTimePicker::Open() {
  DateTimeFormatter parser(chooser_params_);
  std::string test_s = chooser_params_.currentValue.utf8();

  ViewHostMsg_DateTimeDialogValue_Params message;
  message.year =  parser.GetYear();
  message.month =  parser.GetMonth();
  message.day =  parser.GetDay();
  message.hour =  parser.GetHour();
  message.minute = parser.GetMinute();
  message.second = parser.GetSecond();
  message.dialog_type = parser.GetType();

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

void RendererDateTimePicker::OnReplaceDateTime(
    const ViewHostMsg_DateTimeDialogValue_Params& value) {

  DateTimeFormatter formatter(
      static_cast<ui::TextInputType>(value.dialog_type),
      value.year, value.month, value.day,
      value.hour, value.minute, value.second);

  if (chooser_completion_)
    chooser_completion_->didChooseValue(WebString::fromUTF8(
        formatter.GetFormattedValue().c_str()));
}

void RendererDateTimePicker::OnCancel() {
  if (chooser_completion_)
    chooser_completion_->didCancelChooser();
}

}  // namespace content
