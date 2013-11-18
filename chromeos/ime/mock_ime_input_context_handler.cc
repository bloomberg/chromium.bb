// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ime/mock_ime_input_context_handler.h"

#include "chromeos/ime/ibus_text.h"

namespace chromeos {

MockIMEInputContextHandler::MockIMEInputContextHandler()
    : commit_text_call_count_(0),
      forward_key_event_call_count_(0),
      update_preedit_text_call_count_(0),
      show_preedit_text_call_count_(0),
      hide_preedit_text_call_count_(0),
      delete_surrounding_text_call_count_(0) {
}

MockIMEInputContextHandler::~MockIMEInputContextHandler() {
}

void MockIMEInputContextHandler::CommitText(const std::string& text) {
  ++commit_text_call_count_;
  last_commit_text_ = text;
}

void MockIMEInputContextHandler::ForwardKeyEvent(uint32 keyval,
                                                 uint32 keycode,
                                                 uint32 state) {
  ++forward_key_event_call_count_;
}

void MockIMEInputContextHandler::UpdatePreeditText(const IBusText& text,
                                                   uint32 cursor_pos,
                                                   bool visible) {
  ++update_preedit_text_call_count_;
  last_update_preedit_arg_.ibus_text.CopyFrom(text);
  last_update_preedit_arg_.cursor_pos = cursor_pos;
  last_update_preedit_arg_.is_visible = visible;
}

void MockIMEInputContextHandler::ShowPreeditText() {
  ++show_preedit_text_call_count_;
}

void MockIMEInputContextHandler::HidePreeditText() {
  ++hide_preedit_text_call_count_;
}

void MockIMEInputContextHandler::DeleteSurroundingText(int32 offset,
                                                       uint32 length) {
  ++delete_surrounding_text_call_count_;
  last_delete_surrounding_text_arg_.offset = offset;
  last_delete_surrounding_text_arg_.length = length;
}

void MockIMEInputContextHandler::Reset() {
  commit_text_call_count_ = 0;
  forward_key_event_call_count_ = 0;
  update_preedit_text_call_count_ = 0;
  show_preedit_text_call_count_ = 0;
  hide_preedit_text_call_count_ = 0;
  delete_surrounding_text_call_count_ = 0;
  last_commit_text_.clear();
}

}  // namespace chromeos
