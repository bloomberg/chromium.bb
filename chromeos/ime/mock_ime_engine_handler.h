// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_IME_MOCK_IME_ENGINE_HANDLER_H_
#define CHROMEOS_IME_MOCK_IME_ENGINE_HANDLER_H_

#include "chromeos/ime/ibus_bridge.h"

namespace chromeos {

class MockIMEEngineHandler : public IBusEngineHandlerInterface {
 public:
  MockIMEEngineHandler();
  virtual ~MockIMEEngineHandler();

  virtual void FocusIn(ibus::TextInputType text_input_type) OVERRIDE;
  virtual void FocusOut() OVERRIDE;
  virtual void Enable() OVERRIDE;
  virtual void Disable() OVERRIDE;
  virtual void PropertyActivate(const std::string& property_name) OVERRIDE;
  virtual void PropertyShow(const std::string& property_name) OVERRIDE;
  virtual void PropertyHide(const std::string& property_name) OVERRIDE;
  virtual void SetCapability(IBusCapability capability) OVERRIDE;
  virtual void Reset() OVERRIDE;
  virtual void ProcessKeyEvent(uint32 keysym, uint32 keycode, uint32 state,
                               const KeyEventDoneCallback& callback) OVERRIDE;
  virtual void CandidateClicked(uint32 index, ibus::IBusMouseButton button,
                                uint32 state) OVERRIDE;
  virtual void SetSurroundingText(const std::string& text, uint32 cursor_pos,
                                  uint32 anchor_pos) OVERRIDE;

  int focus_in_call_count() const { return focus_in_call_count_; }
  int focus_out_call_count() const { return focus_out_call_count_; }
  int reset_call_count() const { return reset_call_count_; }
  int set_surrounding_text_call_count() const {
    return set_surrounding_text_call_count_;
  }
  int process_key_event_call_count() const {
    return process_key_event_call_count_;
  }

  ibus::TextInputType last_text_input_type() const {
    return last_text_input_type_;
  }

  std::string last_set_surrounding_text() const {
    return last_set_surrounding_text_;
  }

  uint32 last_set_surrounding_cursor_pos() const {
    return last_set_surrounding_cursor_pos_;
  }

  uint32 last_set_surrounding_anchor_pos() const {
    return last_set_surrounding_anchor_pos_;
  }

  uint32 last_processed_keysym() const {
    return last_processed_keysym_;
  }

  uint32 last_processed_keycode() const {
    return last_processed_keycode_;
  }

  uint32 last_processed_state() const {
    return last_processed_state_;
  }

  const KeyEventDoneCallback& last_passed_callback() const {
    return last_passed_callback_;
  }

 private:
  int focus_in_call_count_;
  int focus_out_call_count_;
  int set_surrounding_text_call_count_;
  int process_key_event_call_count_;
  int reset_call_count_;
  ibus::TextInputType last_text_input_type_;
  std::string last_set_surrounding_text_;
  uint32 last_set_surrounding_cursor_pos_;
  uint32 last_set_surrounding_anchor_pos_;
  uint32 last_processed_keysym_;
  uint32 last_processed_keycode_;
  uint32 last_processed_state_;
  KeyEventDoneCallback last_passed_callback_;
};

}  // namespace chromeos

#endif  // CHROMEOS_IME_MOCK_IME_ENGINE_HANDLER_H_
