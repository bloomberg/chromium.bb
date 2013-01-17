// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/renderer_date_time_picker.h"

#include "base/string_util.h"
#include "content/common/view_messages.h"
#include "content/renderer/render_view_impl.h"

#include "third_party/WebKit/Source/WebKit/chromium/public/WebDateTimeChooserCompletion.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDateTimeChooserParams.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDateTimeInputType.h"

namespace content {

using WebKit::WebString;

RendererDateTimePicker::RendererDateTimePicker(
    RenderViewImpl* sender,
    const WebKit::WebDateTimeChooserParams& params,
    WebKit::WebDateTimeChooserCompletion* completion)
    : RenderViewObserver(sender),
      chooser_params_(params),
      chooser_completion_(completion) {
}

RendererDateTimePicker::~RendererDateTimePicker() {
}

static ui::TextInputType ExtractType(
    const WebKit::WebDateTimeChooserParams& source) {

  if (source.type == WebKit::WebDateTimeInputTypeDate)
    return ui::TEXT_INPUT_TYPE_DATE;
  if (source.type == WebKit::WebDateTimeInputTypeDateTime)
    return ui::TEXT_INPUT_TYPE_DATE_TIME;
  if (source.type == WebKit::WebDateTimeInputTypeDateTimeLocal)
    return ui::TEXT_INPUT_TYPE_DATE_TIME_LOCAL;
  if (source.type == WebKit::WebDateTimeInputTypeMonth)
    return ui::TEXT_INPUT_TYPE_MONTH;
  if (source.type == WebKit::WebDateTimeInputTypeTime)
    return ui::TEXT_INPUT_TYPE_TIME;
  if (source.type == WebKit::WebDateTimeInputTypeWeek)
    return ui::TEXT_INPUT_TYPE_WEEK;
  return ui::TEXT_INPUT_TYPE_NONE;
}

bool RendererDateTimePicker::Open() {
  Send(new ViewHostMsg_OpenDateTimeDialog(
      routing_id(), ExtractType(chooser_params_),
      chooser_params_.currentValue.utf8()));
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

void RendererDateTimePicker::OnReplaceDateTime(const string16& new_date) {
  if (chooser_completion_)
    chooser_completion_->didChooseValue(new_date);
}

void RendererDateTimePicker::OnCancel() {
  if (chooser_completion_)
    chooser_completion_->didCancelChooser();
}

}  // namespace content
