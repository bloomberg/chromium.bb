// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/ibus/mock_ibus_engine_service.h"

#include "chromeos/dbus/ibus/ibus_text.h"

namespace chromeos {

MockIBusEngineService::MockIBusEngineService()
    : register_properties_call_count_(0),
      update_preedit_call_count_(0),
      update_auxiliary_text_call_count_(0),
      update_lookup_table_call_count_(0),
      update_property_call_count_(0),
      forward_key_event_call_count_(0),
      commit_text_call_count_(0),
      delete_surrounding_text_call_count_(0),
      last_update_preedit_arg_(new UpdatePreeditArg()),
      last_update_aux_text_arg_(new UpdateAuxiliaryTextArg()),
      last_update_lookup_table_arg_(new UpdateLookupTableArg()),
      last_registered_properties_(new IBusPropertyList()),
      last_updated_property_(new IBusProperty()),
      last_delete_surrounding_text_arg_(new DeleteSurroundingTextArg()),
      current_engine_(NULL) {
}

MockIBusEngineService::~MockIBusEngineService() {
}

void MockIBusEngineService::SetEngine(IBusEngineHandlerInterface* handler) {
  current_engine_ = handler;
}

void MockIBusEngineService::UnsetEngine(IBusEngineHandlerInterface* handler) {
  current_engine_ = NULL;
}

void MockIBusEngineService::RegisterProperties(
      const IBusPropertyList& property_list) {
  ++register_properties_call_count_;
  last_registered_properties_->resize(property_list.size());
  for (size_t i = 0; i < property_list.size(); ++i) {
    (*last_registered_properties_)[i] = new IBusProperty();
    (*last_registered_properties_)[i]->CopyFrom(*property_list[i]);
  }
}

void MockIBusEngineService::UpdatePreedit(const IBusText& ibus_text,
                                          uint32 cursor_pos,
                                          bool is_visible,
                                          IBusEnginePreeditFocusOutMode mode) {
  ++update_preedit_call_count_;
  last_update_preedit_arg_->ibus_text.CopyFrom(ibus_text);
  last_update_preedit_arg_->cursor_pos = cursor_pos;
  last_update_preedit_arg_->is_visible = is_visible;
}

void MockIBusEngineService::UpdateAuxiliaryText(const IBusText& ibus_text,
                                                bool is_visible) {
  ++update_auxiliary_text_call_count_;
  last_update_aux_text_arg_->ibus_text.CopyFrom(ibus_text);
  last_update_aux_text_arg_->is_visible = is_visible;
}

void MockIBusEngineService::UpdateLookupTable(
    const IBusLookupTable& lookup_table,
    bool is_visible) {
  ++update_lookup_table_call_count_;
  last_update_lookup_table_arg_->lookup_table.CopyFrom(lookup_table);
  last_update_lookup_table_arg_->is_visible = is_visible;
}

void MockIBusEngineService::UpdateProperty(const IBusProperty& property) {
  ++update_property_call_count_;
  last_updated_property_->CopyFrom(property);
}

void MockIBusEngineService::ForwardKeyEvent(uint32 keyval,
                                            uint32 keycode,
                                            uint32 state) {
  ++forward_key_event_call_count_;
}

void MockIBusEngineService::RequireSurroundingText() {
}

void MockIBusEngineService::CommitText(const std::string& text) {
  ++commit_text_call_count_;
  last_commit_text_ = text;
}

void MockIBusEngineService::DeleteSurroundingText(int32 offset,uint32 length) {
  ++delete_surrounding_text_call_count_;
  last_delete_surrounding_text_arg_->offset = offset;
  last_delete_surrounding_text_arg_->length = length;
}

IBusEngineHandlerInterface* MockIBusEngineService::GetEngine() const {
  return current_engine_;
}

void MockIBusEngineService::Clear() {
  register_properties_call_count_ = 0;
  update_preedit_call_count_ = 0;
  update_auxiliary_text_call_count_ = 0;
  update_lookup_table_call_count_ = 0;
  update_property_call_count_ = 0;
  forward_key_event_call_count_ = 0;
  commit_text_call_count_ = 0;
  delete_surrounding_text_call_count_ = 0;
  last_commit_text_.clear();
  last_update_preedit_arg_.reset(new UpdatePreeditArg());
  last_update_aux_text_arg_.reset(new UpdateAuxiliaryTextArg());
  last_update_lookup_table_arg_.reset(new UpdateLookupTableArg());
  last_registered_properties_.reset(new IBusPropertyList());
  last_updated_property_.reset(new IBusProperty());
  last_delete_surrounding_text_arg_.reset(new DeleteSurroundingTextArg());
}

}  // namespace chromeos
