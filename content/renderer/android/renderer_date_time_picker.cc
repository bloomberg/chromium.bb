// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/android/renderer_date_time_picker.h"

#include <stddef.h>

#include "base/strings/string_util.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/blink/public/web/web_date_time_chooser_completion.h"
#include "third_party/blink/public/web/web_date_time_chooser_params.h"
#include "third_party/blink/public/web/web_date_time_input_type.h"
#include "third_party/blink/public/web/web_date_time_suggestion.h"
#include "ui/base/ime/text_input_type.h"

using blink::WebString;

namespace mojo {

template <>
struct TypeConverter<::content::mojom::DateTimeSuggestionPtr,
                     ::blink::WebDateTimeSuggestion> {
  static content::mojom::DateTimeSuggestionPtr Convert(
      const blink::WebDateTimeSuggestion& input) {
    content::mojom::DateTimeSuggestionPtr output =
        content::mojom::DateTimeSuggestion::New();
    output->value = input.value;
    output->localized_value = input.localized_value.Utf16();
    output->label = input.label.Utf16();
    return output;
  }
};

}  // namespace mojo

namespace content {

static ui::TextInputType ToTextInputType(int type) {
  switch (type) {
    case blink::kWebDateTimeInputTypeDate:
      return ui::TEXT_INPUT_TYPE_DATE;
      break;
    case blink::kWebDateTimeInputTypeDateTime:
      return ui::TEXT_INPUT_TYPE_DATE_TIME;
      break;
    case blink::kWebDateTimeInputTypeDateTimeLocal:
      return ui::TEXT_INPUT_TYPE_DATE_TIME_LOCAL;
      break;
    case blink::kWebDateTimeInputTypeMonth:
      return ui::TEXT_INPUT_TYPE_MONTH;
      break;
    case blink::kWebDateTimeInputTypeTime:
      return ui::TEXT_INPUT_TYPE_TIME;
      break;
    case blink::kWebDateTimeInputTypeWeek:
      return ui::TEXT_INPUT_TYPE_WEEK;
      break;
    case blink::kWebDateTimeInputTypeNone:
    default:
      return ui::TEXT_INPUT_TYPE_NONE;
  }
}

RendererDateTimePicker::RendererDateTimePicker(
    RenderViewImpl* sender,
    const blink::WebDateTimeChooserParams& params,
    blink::WebDateTimeChooserCompletion* completion)
    : RenderViewObserver(sender),
      chooser_params_(params),
      chooser_completion_(completion) {}

RendererDateTimePicker::~RendererDateTimePicker() {}

void RendererDateTimePicker::Open() {
  mojom::DateTimeDialogValuePtr date_time_dialog_value =
      mojom::DateTimeDialogValue::New();
  date_time_dialog_value->dialog_type = ToTextInputType(chooser_params_.type);
  date_time_dialog_value->dialog_value = chooser_params_.double_value;
  date_time_dialog_value->minimum = chooser_params_.minimum;
  date_time_dialog_value->maximum = chooser_params_.maximum;
  date_time_dialog_value->step = chooser_params_.step;
  for (size_t i = 0; i < chooser_params_.suggestions.size(); i++) {
    date_time_dialog_value->suggestions.push_back(
        mojo::ConvertTo<mojom::DateTimeSuggestionPtr>(
            chooser_params_.suggestions[i]));
  }

  auto response_callback = base::BindOnce(
      &RendererDateTimePicker::ResponseHandler, base::Unretained(this));
  GetDateTimePicker()->OpenDateTimeDialog(std::move(date_time_dialog_value),
                                          std::move(response_callback));
}

void RendererDateTimePicker::ResponseHandler(bool success,
                                             double dialog_value) {
  if (success)
    ReplaceDateTime(dialog_value);
  else
    Cancel();
}

void RendererDateTimePicker::ReplaceDateTime(double value) {
  if (chooser_completion_)
    chooser_completion_->DidChooseValue(value);
  static_cast<RenderViewImpl*>(render_view())->DismissDateTimeDialog();
}

void RendererDateTimePicker::Cancel() {
  if (chooser_completion_)
    chooser_completion_->DidCancelChooser();
  static_cast<RenderViewImpl*>(render_view())->DismissDateTimeDialog();
}

void RendererDateTimePicker::OnDestruct() {
  delete this;
}

mojom::DateTimePicker* RendererDateTimePicker::GetDateTimePicker() {
  if (!date_time_picker_) {
    render_view()->GetMainRenderFrame()->GetRemoteInterfaces()->GetInterface(
        &date_time_picker_);
  }
  return date_time_picker_.get();
}

}  // namespace content
