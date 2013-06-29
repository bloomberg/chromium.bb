// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_IBUS_MOCK_IBUS_ENGINE_SERVICE_H_
#define CHROMEOS_DBUS_IBUS_MOCK_IBUS_ENGINE_SERVICE_H_

#include <string>
#include "chromeos/dbus/ibus/ibus_engine_service.h"

#include "chromeos/dbus/ibus/ibus_lookup_table.h"
#include "chromeos/dbus/ibus/ibus_property.h"
#include "chromeos/dbus/ibus/ibus_text.h"

namespace chromeos {

class IBusText;

class MockIBusEngineService : public IBusEngineService {
 public:

  struct UpdatePreeditArg {
    UpdatePreeditArg() : is_visible(false) {}
    IBusText ibus_text;
    uint32 cursor_pos;
    bool is_visible;
  };

  struct UpdateAuxiliaryTextArg {
    UpdateAuxiliaryTextArg() : is_visible(false) {}
    IBusText ibus_text;
    bool is_visible;
  };

  struct UpdateLookupTableArg {
    UpdateLookupTableArg() : is_visible(false) {}
    IBusLookupTable lookup_table;
    bool is_visible;
  };

  struct DeleteSurroundingTextArg {
    int32 offset;
    uint32 length;
  };

  MockIBusEngineService();
  virtual ~MockIBusEngineService();

  // IBusEngineService overrides.
  virtual void SetEngine(IBusEngineHandlerInterface* handler) OVERRIDE;
  virtual void UnsetEngine(IBusEngineHandlerInterface* handler) OVERRIDE;
  virtual void RegisterProperties(
      const IBusPropertyList& property_list) OVERRIDE;
  virtual void UpdatePreedit(const IBusText& ibus_text,
                             uint32 cursor_pos,
                             bool is_visible,
                             IBusEnginePreeditFocusOutMode mode) OVERRIDE;
  virtual void UpdateAuxiliaryText(const IBusText& ibus_text,
                                   bool is_visible) OVERRIDE;
  virtual void UpdateLookupTable(const IBusLookupTable& lookup_table,
                                 bool is_visible) OVERRIDE;
  virtual void UpdateProperty(const IBusProperty& property) OVERRIDE;
  virtual void ForwardKeyEvent(uint32 keyval, uint32 keycode,
                               uint32 state) OVERRIDE;
  virtual void RequireSurroundingText() OVERRIDE;
  virtual void CommitText(const std::string& text) OVERRIDE;
  virtual void DeleteSurroundingText(int32 offset, uint32 length) OVERRIDE;

  IBusEngineHandlerInterface* GetEngine() const;

  void Clear();

  int commit_text_call_count() const { return commit_text_call_count_; }
  const std::string& last_commit_text() const { return last_commit_text_; }

  int update_preedit_call_count() const { return update_preedit_call_count_; }
  const UpdatePreeditArg& last_update_preedit_arg() const {
    return *last_update_preedit_arg_.get();
  }

  int update_auxiliary_text_call_count() const {
    return update_auxiliary_text_call_count_;
  }
  const UpdateAuxiliaryTextArg& last_update_aux_text_arg() const {
    return *last_update_aux_text_arg_.get();
  }

  int update_lookup_table_call_count() const {
    return update_lookup_table_call_count_;
  }
  const UpdateLookupTableArg& last_update_lookup_table_arg() const {
    return *last_update_lookup_table_arg_.get();
  }

  int register_properties_call_count() const {
    return register_properties_call_count_;
  }
  const IBusPropertyList& last_registered_properties() const {
    return *last_registered_properties_.get();
  }

  int update_property_call_count() const {
    return update_property_call_count_;
  }
  const IBusProperty& last_updated_property() const {
    return *last_updated_property_.get();
  }

  int delete_surrounding_text_call_count() const {
    return delete_surrounding_text_call_count_;
  }
  const DeleteSurroundingTextArg& last_delete_surrounding_text_arg() const {
    return *last_delete_surrounding_text_arg_.get();
  }

 private:
  int register_properties_call_count_;
  int update_preedit_call_count_;
  int update_auxiliary_text_call_count_;
  int update_lookup_table_call_count_;
  int update_property_call_count_;
  int forward_key_event_call_count_;
  int commit_text_call_count_;
  int delete_surrounding_text_call_count_;

  std::string last_commit_text_;
  scoped_ptr<UpdatePreeditArg> last_update_preedit_arg_;
  scoped_ptr<UpdateAuxiliaryTextArg> last_update_aux_text_arg_;
  scoped_ptr<UpdateLookupTableArg> last_update_lookup_table_arg_;
  scoped_ptr<IBusPropertyList> last_registered_properties_;
  scoped_ptr<IBusProperty> last_updated_property_;
  scoped_ptr<DeleteSurroundingTextArg> last_delete_surrounding_text_arg_;

  IBusEngineHandlerInterface* current_engine_;

  DISALLOW_COPY_AND_ASSIGN(MockIBusEngineService);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_IBUS_MOCK_IBUS_ENGINE_SERVICE_H_
