// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_ENGINE_IBUS_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_ENGINE_IBUS_H_

#include <string>
#include <vector>
#include <map>
#include "chrome/browser/chromeos/input_method/input_method_engine.h"
#include "chromeos/dbus/ibus/ibus_engine_factory_service.h"
#include "chromeos/dbus/ibus/ibus_engine_service.h"
#include "dbus/object_path.h"

namespace chromeos {

namespace ibus {
class IBusComponent;
class IBusLookupTable;
class IBusText;
}

class IBusEngineService;
namespace input_method {
struct KeyEventHandle;
}  // namespace input_method

class InputMethodEngineIBus : public InputMethodEngine,
                              public IBusEngineHandlerInterface {
 public:
  InputMethodEngineIBus();

  virtual ~InputMethodEngineIBus();

  void Initialize(
      InputMethodEngine::Observer* observer,
      const char* engine_name,
      const char* extension_id,
      const char* engine_id,
      const char* description,
      const char* language,
      const std::vector<std::string>& layouts,
      std::string* error);

  // InputMethodEngine overrides.
  virtual bool SetComposition(int context_id,
                              const char* text,
                              int selection_start,
                              int selection_end,
                              int cursor,
                              const std::vector<SegmentInfo>& segments,
                              std::string* error) OVERRIDE;
  virtual bool ClearComposition(int context_id, std::string* error) OVERRIDE;
  virtual bool CommitText(int context_id, const char* text,
                          std::string* error) OVERRIDE;
  virtual bool SetCandidateWindowVisible(bool visible,
                                         std::string* error) OVERRIDE;
  virtual void SetCandidateWindowCursorVisible(bool visible) OVERRIDE;
  virtual void SetCandidateWindowVertical(bool vertical) OVERRIDE;
  virtual void SetCandidateWindowPageSize(int size) OVERRIDE;
  virtual void SetCandidateWindowAuxText(const char* text) OVERRIDE;
  virtual void SetCandidateWindowAuxTextVisible(bool visible) OVERRIDE;
  virtual bool SetCandidates(int context_id,
                             const std::vector<Candidate>& candidates,
                             std::string* error) OVERRIDE;
  virtual bool SetCursorPosition(int context_id, int candidate_id,
                                 std::string* error) OVERRIDE;
  virtual bool SetMenuItems(const std::vector<MenuItem>& items) OVERRIDE;
  virtual bool UpdateMenuItems(const std::vector<MenuItem>& items) OVERRIDE;
  virtual bool IsActive() const OVERRIDE;
  virtual void KeyEventDone(input_method::KeyEventHandle* key_data,
                            bool handled) OVERRIDE;

  // IBusEngineHandlerInterface overrides.
  virtual void FocusIn() OVERRIDE;
  virtual void FocusOut() OVERRIDE;
  virtual void Enable() OVERRIDE;
  virtual void Disable() OVERRIDE;
  virtual void PropertyActivate(const std::string& property_name,
                                IBusPropertyState property_state) OVERRIDE;
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

  // Called when the connection with ibus-daemon is connected.
  void OnConnected();

  // Called whtn the connection with ibus-daemon is disconnected.
  void OnDisconnected();

 private:
  // Returns true if the connection to ibus-daemon is avaiable.
  bool IsConnected();

  // Converts MenuItem to IBusProperty.
  bool MenuItemToProperty(const MenuItem& item, ibus::IBusProperty* property);

  // Registers the engine component.
  void RegisterComponent();

  // Called when the RegisterComponent is failed.
  void OnComponentRegistrationFailed();

  // Called when the RegisterComponent is succeeded.
  void OnComponentRegistered();

  // Called when the ibus-daemon sends CreateEngine message with corresponding
  // engine id.
  void CreateEngineHandler(
      const IBusEngineFactoryService::CreateEngineResponseSender& sender);

  // Returns current IBusEngineService, if there is no available service, this
  // function returns NULL.
  IBusEngineService* GetCurrentService();

  // True if the current context has focus.
  bool focused_;

  // True if this engine is active.
  bool active_;

  // ID that is used for the current input context.  False if there is no focus.
  int context_id_;

  // Next id that will be assigned to a context.
  int next_context_id_;

  // This IME ID in Chrome Extension.
  std::string engine_id_;

  // This IME ID in ibus.
  std::string ibus_id_;

  // The current object path and it's numerical id.
  dbus::ObjectPath object_path_;
  int current_object_path_;

  // The current auxialy text and it's visiblity.
  scoped_ptr<ibus::IBusText> aux_text_;
  bool aux_text_visible_;

  // Pointer to the object recieving events for this IME.
  InputMethodEngine::Observer* observer_;

  // The current preedit text, and it's cursor position.
  scoped_ptr<ibus::IBusText> preedit_text_;
  int preedit_cursor_;

  // The current engine component.
  scoped_ptr<ibus::IBusComponent> component_;

  // The current lookup table.
  scoped_ptr<ibus::IBusLookupTable> table_;

  // Indicates whether the candidate window is visible.
  bool window_visible_;

  // Mapping of candidate index to candidate id.
  std::vector<int> candidate_ids_;

  // Mapping of candidate id to index.
  std::map<int, int> candidate_indexes_;

  // Used for making callbacks.
  base::WeakPtrFactory<InputMethodEngineIBus> weak_ptr_factory_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_ENGINE_IBUS_H_
