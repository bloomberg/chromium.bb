// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ime/mock_ime_engine_handler.h"

namespace chromeos {

MockIMEEngineHandler::MockIMEEngineHandler()
    : focus_in_call_count_(0),
      focus_out_call_count_(0),
      set_surrounding_text_call_count_(0),
      process_key_event_call_count_(0),
      reset_call_count_(0),
      last_text_input_type_(ibus::TEXT_INPUT_TYPE_NONE),
      last_set_surrounding_cursor_pos_(0),
      last_set_surrounding_anchor_pos_(0),
      last_processed_keysym_(0),
      last_processed_keycode_(0),
      last_processed_state_(0) {
}

MockIMEEngineHandler::~MockIMEEngineHandler() {
}

void MockIMEEngineHandler::FocusIn(ibus::TextInputType text_input_type) {
  ++focus_in_call_count_;
  last_text_input_type_ = text_input_type;
}

void MockIMEEngineHandler::FocusOut() {
  ++focus_out_call_count_;
}

void MockIMEEngineHandler::Enable() {
}

void MockIMEEngineHandler::Disable() {
}

void MockIMEEngineHandler::PropertyActivate(const std::string& property_name) {
}

void MockIMEEngineHandler::PropertyShow(const std::string& property_name) {
}

void MockIMEEngineHandler::PropertyHide(const std::string& property_name) {
}

void MockIMEEngineHandler::SetCapability(IBusCapability capability) {
}

void MockIMEEngineHandler::Reset() {
  ++reset_call_count_;
}

void MockIMEEngineHandler::ProcessKeyEvent(
    uint32 keysym,
    uint32 keycode,
    uint32 state,
    const KeyEventDoneCallback& callback) {
  ++process_key_event_call_count_;
  last_processed_keysym_ = keysym;
  last_processed_keycode_ = keycode;
  last_processed_state_ = state;
  last_passed_callback_ = callback;
}

void MockIMEEngineHandler::CandidateClicked(uint32 index,
                                            ibus::IBusMouseButton button,
                                            uint32 state) {
}

void MockIMEEngineHandler::SetSurroundingText(const std::string& text,
                                              uint32 cursor_pos,
                                              uint32 anchor_pos) {
  ++set_surrounding_text_call_count_;
  last_set_surrounding_text_ = text;
  last_set_surrounding_cursor_pos_ = cursor_pos;
  last_set_surrounding_anchor_pos_ = anchor_pos;
}

} // namespace chromeos

