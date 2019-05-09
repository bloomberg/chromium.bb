// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ime_driver/remote_text_input_client.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "ui/events/event_dispatcher.h"

struct RemoteTextInputClient::QueuedEvent {
  QueuedEvent(std::unique_ptr<ui::Event> event,
              DispatchKeyEventPostIMECallback callback)
      : event(std::move(event)), callback(std::move(callback)) {}

  std::unique_ptr<ui::Event> event;
  DispatchKeyEventPostIMECallback callback;
};

RemoteTextInputClient::RemoteTextInputClient(
    ws::mojom::TextInputClientPtr client,
    ws::mojom::SessionDetailsPtr details)
    : remote_client_(std::move(client)), details_(std::move(details)) {}

RemoteTextInputClient::~RemoteTextInputClient() {
  while (!queued_events_.empty()) {
    RunNextPendingCallback(/* handled */ false,
                           /* stopped_propagation */ false);
  }
}

void RemoteTextInputClient::SetTextInputState(
    ws::mojom::TextInputStatePtr text_input_state) {
  details_->state = std::move(text_input_state);
}

void RemoteTextInputClient::SetCaretBounds(const gfx::Rect& caret_bounds) {
  details_->caret_bounds = caret_bounds;
}

void RemoteTextInputClient::SetTextInputClientData(
    ws::mojom::TextInputClientDataPtr data) {
  details_->data = std::move(data);
}

void RemoteTextInputClient::OnDispatchKeyEventPostIMECompleted(
    bool handled,
    bool stopped_propagation) {
  RunNextPendingCallback(handled, stopped_propagation);
  DispatchQueuedEvent();
}

void RemoteTextInputClient::SetCompositionText(
    const ui::CompositionText& composition) {
  remote_client_->SetCompositionText(composition);
}

void RemoteTextInputClient::ConfirmCompositionText() {
  remote_client_->ConfirmCompositionText();
}

void RemoteTextInputClient::ClearCompositionText() {
  remote_client_->ClearCompositionText();
}

void RemoteTextInputClient::InsertText(const base::string16& text) {
  remote_client_->InsertText(text);
}

void RemoteTextInputClient::InsertChar(const ui::KeyEvent& event) {
  remote_client_->InsertChar(ui::Event::Clone(event));
}

ui::TextInputType RemoteTextInputClient::GetTextInputType() const {
  return details_->state->text_input_type;
}

ui::TextInputMode RemoteTextInputClient::GetTextInputMode() const {
  return details_->state->text_input_mode;
}

base::i18n::TextDirection RemoteTextInputClient::GetTextDirection() const {
  return details_->state->text_direction;
}

int RemoteTextInputClient::GetTextInputFlags() const {
  return details_->state->text_input_flags;
}

bool RemoteTextInputClient::CanComposeInline() const {
  // If we return false here, ui::InputMethodChromeOS will try to create a
  // composition window. But here we are at IMEDriver, and composition
  // window shouldn't be created by IMEDriver.
  return true;
}

gfx::Rect RemoteTextInputClient::GetCaretBounds() const {
  return details_->caret_bounds;
}

bool RemoteTextInputClient::GetCompositionCharacterBounds(
    uint32_t index,
    gfx::Rect* rect) const {
  // TODO(moshayedi): crbug.com/631527.
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

bool RemoteTextInputClient::HasCompositionText() const {
  return details_->data->has_composition_text;
}

ui::TextInputClient::FocusReason RemoteTextInputClient::GetFocusReason() const {
  return details_->focus_reason;
}

bool RemoteTextInputClient::GetTextRange(gfx::Range* range) const {
  if (!details_->data->text_range.has_value())
    return false;

  *range = details_->data->text_range.value();
  return true;
}

bool RemoteTextInputClient::GetCompositionTextRange(gfx::Range* range) const {
  if (!details_->data->composition_text_range.has_value())
    return false;

  *range = details_->data->composition_text_range.value();
  return true;
}

bool RemoteTextInputClient::GetEditableSelectionRange(gfx::Range* range) const {
  if (!details_->data->editable_selection_range.has_value())
    return false;

  *range = details_->data->editable_selection_range.value();
  return true;
}

bool RemoteTextInputClient::SetEditableSelectionRange(const gfx::Range& range) {
  remote_client_->SetEditableSelectionRange(range);
  // Note that we assume the client side always succeeds.
  return true;
}

bool RemoteTextInputClient::DeleteRange(const gfx::Range& range) {
  remote_client_->DeleteRange(range);
  // Note that we assume the client side always succeeds.
  return true;
}

bool RemoteTextInputClient::GetTextFromRange(const gfx::Range& range,
                                             base::string16* text) const {
  if (!details_->data->text.has_value() ||
      !details_->data->text_range.has_value() ||
      !details_->data->text_range->Contains(range)) {
    return false;
  }

  *text = details_->data->text->substr(range.GetMin(), range.length());
  return true;
}

void RemoteTextInputClient::OnInputMethodChanged() {
  remote_client_->OnInputMethodChanged();
}

bool RemoteTextInputClient::ChangeTextDirectionAndLayoutAlignment(
    base::i18n::TextDirection direction) {
  remote_client_->ChangeTextDirectionAndLayoutAlignment(direction);
  // Note that we assume the client side always succeeds.
  return true;
}

void RemoteTextInputClient::ExtendSelectionAndDelete(size_t before,
                                                     size_t after) {
  remote_client_->ExtendSelectionAndDelete(before, after);
}

void RemoteTextInputClient::EnsureCaretNotInRect(const gfx::Rect& rect) {
  remote_client_->EnsureCaretNotInRect(rect);
}

bool RemoteTextInputClient::IsTextEditCommandEnabled(
    ui::TextEditCommand command) const {
  if (!details_->data->edit_command_enabled.has_value())
    return false;

  const size_t index = static_cast<size_t>(command);
  if (index >= details_->data->edit_command_enabled->size())
    return false;

  return details_->data->edit_command_enabled->at(index);
}

void RemoteTextInputClient::SetTextEditCommandForNextKeyEvent(
    ui::TextEditCommand command) {
  // TODO(moshayedi): crbug.com/631527.
  NOTIMPLEMENTED_LOG_ONCE();
}

ukm::SourceId RemoteTextInputClient::GetClientSourceForMetrics() const {
  return details_->client_source_for_metrics;
}

bool RemoteTextInputClient::ShouldDoLearning() {
  return details_->should_do_learning;
}

bool RemoteTextInputClient::SetCompositionFromExistingText(
    const gfx::Range& range,
    const std::vector<ui::ImeTextSpan>& ui_ime_text_spans) {
  // TODO(https://crbug.com/952757): Implement this method.
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

ui::EventDispatchDetails RemoteTextInputClient::DispatchKeyEventPostIME(
    ui::KeyEvent* event,
    DispatchKeyEventPostIMECallback callback) {
  const bool is_first_event = queued_events_.empty();
  queued_events_.emplace(ui::Event::Clone(*event), std::move(callback));
  if (is_first_event)
    DispatchQueuedEvent();
  return ui::EventDispatchDetails();
}

void RemoteTextInputClient::DispatchQueuedEvent() {
  if (queued_events_.empty())
    return;

  DCHECK(queued_events_.front().event);
  remote_client_->DispatchKeyEventPostIME(
      std::move(queued_events_.front().event),
      base::BindOnce(&RemoteTextInputClient::OnDispatchKeyEventPostIMECompleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void RemoteTextInputClient::RunNextPendingCallback(bool handled,
                                                   bool stopped_propagation) {
  DCHECK(!queued_events_.empty());
  DispatchKeyEventPostIMECallback callback =
      std::move(queued_events_.front().callback);
  queued_events_.pop();
  if (callback)
    std::move(callback).Run(handled, stopped_propagation);
}
