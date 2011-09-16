// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/input_method_engine.h"

#include <map>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "chrome/browser/chromeos/input_method/ibus_engine_controller.h"
#include "chrome/browser/chromeos/input_method/ibus_keymap.h"
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"

namespace chromeos {

const char* kExtensionImePrefix = "_ext_ime_";
const char* kErrorNotActive = "IME is not active";
const char* kErrorWrongContext = "Context is not active";
const char* kCandidateNotFound = "Candidate not found";

InputMethodEngine::KeyboardEvent::KeyboardEvent() {
}

InputMethodEngine::KeyboardEvent::~KeyboardEvent() {
}

InputMethodEngine::MenuItem::MenuItem() {
}

InputMethodEngine::MenuItem::~MenuItem() {
}

InputMethodEngine::Candidate::Candidate() {
}

InputMethodEngine::Candidate::~Candidate() {
}

InputMethodEngine::Observer::~Observer() {
}

class InputMethodEngineImpl
    : public InputMethodEngine,
      public input_method::IBusEngineController::Observer {
 public:
  InputMethodEngineImpl()
      : observer_(NULL), active_(false), next_context_id_(1),
        context_id_(-1) {}

  ~InputMethodEngineImpl() {
    input_method::InputMethodManager::GetInstance()->RemoveActiveIme(ibus_id_);
  }

  bool Init(InputMethodEngine::Observer* observer,
            const char* engine_name,
            const char* extension_id,
            const char* engine_id,
            const char* description,
            const char* language,
            const std::vector<std::string>& layouts,
            KeyboardEvent& shortcut_key,
            std::string* error);

  virtual bool SetComposition(int context_id,
                              const char* text, int selection_start,
                              int selection_end,
                              const std::vector<SegmentInfo>& segments,
                              std::string* error);
  virtual bool ClearComposition(int context_id,
                                std::string* error);
  virtual bool CommitText(int context_id,
                          const char* text, std::string* error);
  virtual bool SetCandidateWindowVisible(bool visible, std::string* error);
  virtual void SetCandidateWindowCursorVisible(bool visible);
  virtual void SetCandidateWindowVertical(bool vertical);
  virtual void SetCandidateWindowPageSize(int size);
  virtual void SetCandidateWindowAuxText(const char* text);
  virtual void SetCandidateWindowAuxTextVisible(bool visible);
  virtual bool SetCandidates(int context_id,
                             const std::vector<Candidate>& candidates,
                             std::string* error);
  virtual bool SetCursorPosition(int context_id, int candidate_id,
                                 std::string* error);
  virtual void SetMenuItems(const std::vector<MenuItem>& items);
  virtual void UpdateMenuItems(const std::vector<MenuItem>& items);
  virtual bool IsActive() const {
    return active_;
  }
  virtual void KeyEventDone(input_method::KeyEventHandle* key_data,
                            bool handled);

  virtual void OnReset();
  virtual void OnEnable();
  virtual void OnDisable();
  virtual void OnFocusIn();
  virtual void OnFocusOut();
  virtual void OnKeyEvent(bool key_press, unsigned int keyval,
                          unsigned int keycode, bool alt_key, bool ctrl_key,
                          bool shift_key,
                          input_method::KeyEventHandle* key_data);
  virtual void OnPropertyActivate(const char* name, unsigned int state);
  virtual void OnCandidateClicked(unsigned int index, unsigned int button,
                                  unsigned int state);
 private:
  // Pointer to the object recieving events for this IME.
  InputMethodEngine::Observer* observer_;

  // Connection to IBus.
  scoped_ptr<input_method::IBusEngineController> connection_;

  // True when this IME is active, false if deactive.
  bool active_;

  // Next id that will be assigned to a context.
  int next_context_id_;

  // ID that is used for the current input context.  False if there is no focus.
  int context_id_;

  // User specified id of this IME.
  std::string engine_id_;

  // ID used by ibus to reference this IME.
  std::string ibus_id_;

  // Mapping of candidate index to candidate id.
  std::vector<int> candidate_ids_;

  // Mapping of candidate id to index.
  std::map<int, int> candidate_indexs_;
};

InputMethodEngine* InputMethodEngine::CreateEngine(
    InputMethodEngine::Observer* observer,
    const char* engine_name,
    const char* extension_id,
    const char* engine_id,
    const char* description,
    const char* language,
    const std::vector<std::string>& layouts,
    KeyboardEvent& shortcut_key,
    std::string* error) {
  InputMethodEngineImpl* new_engine = new InputMethodEngineImpl();
  if (!new_engine->Init(observer, engine_name, extension_id, engine_id,
                        description, language, layouts, shortcut_key, error)) {
    LOG(ERROR) << "Init() failed.";
    delete new_engine;
    new_engine = NULL;
  }

  return new_engine;
}

bool InputMethodEngineImpl::Init(InputMethodEngine::Observer* observer,
                                 const char* engine_name,
                                 const char* extension_id,
                                 const char* engine_id,
                                 const char* description,
                                 const char* language,
                                 const std::vector<std::string>& layouts,
                                 KeyboardEvent& shortcut_key,
                                 std::string* error) {
  ibus_id_ = kExtensionImePrefix;
  ibus_id_ += extension_id;
  ibus_id_ += engine_id;

  std::string layout;

  if (!layouts.empty()) {
    layout = JoinString(layouts, ',');
  } else {
    const std::string fallback_id = input_method::GetHardwareInputMethodId();
    const input_method::InputMethodDescriptor* fallback_desc =
        input_method::GetInputMethodDescriptorFromId(fallback_id);
    layout = fallback_desc->keyboard_layout();
  }

  connection_.reset(input_method::IBusEngineController::Create(this,
                                                               ibus_id_.c_str(),
                                                               engine_name,
                                                               description,
                                                               language,
                                                               layout.c_str()));
  if (!connection_.get()) {
    *error = "ConnectInputMethodExtension() failed.";
    return false;
  }

  observer_ = observer;
  engine_id_ = engine_id;
  input_method::InputMethodManager::GetInstance()->AddActiveIme(ibus_id_,
                                                                engine_name,
                                                                layouts,
                                                                language);

  return true;
}

bool InputMethodEngineImpl::SetComposition(
    int context_id,
    const char* text,
    int selection_start,
    int selection_end,
    const std::vector<SegmentInfo>& segments,
    std::string* error) {
  if (!active_) {
    *error = kErrorNotActive;
    return false;
  }
  if (context_id != context_id_ || context_id_ == -1) {
    *error = kErrorWrongContext;
    return false;
  }

  connection_->SetPreeditText(text, selection_end);
  // TODO: Add support for displaying selected text in the composition string.
  for (std::vector<SegmentInfo>::const_iterator segment = segments.begin();
       segment != segments.end(); ++segment) {
    int style;
    switch (segment->style) {
      case SEGMENT_STYLE_UNDERLINE:
        style = input_method::IBusEngineController::UNDERLINE_SINGLE;
        break;
      case SEGMENT_STYLE_DOUBLE_UNDERLINE:
        style = input_method::IBusEngineController::UNDERLINE_DOUBLE;
        break;

      default:
        continue;
    }

    connection_->SetPreeditUnderline(segment->start, segment->end, style);
  }
  return true;
}

bool InputMethodEngineImpl::ClearComposition(int context_id,
                                             std::string* error) {
  if (!active_) {
    *error = kErrorNotActive;
    return false;
  }
  if (context_id != context_id_ || context_id_ == -1) {
    *error = kErrorWrongContext;
    return false;
  }

  connection_->SetPreeditText("", 0);
  return true;
}

bool InputMethodEngineImpl::CommitText(int context_id,
                                       const char* text, std::string* error) {
  if (context_id != context_id_ || context_id_ == -1) {
    *error = kErrorWrongContext;
    return false;
  }
  if (!active_) {
    // TODO: Commit the text anyways.
    *error = kErrorNotActive;
    return false;
  }

  connection_->CommitText(text);
  return true;
}

bool InputMethodEngineImpl::SetCandidateWindowVisible(bool visible,
                                                      std::string* error) {
  if (!active_) {
    *error = kErrorNotActive;
    return false;
  }

  connection_->SetTableVisible(visible);
  return true;
}

void InputMethodEngineImpl::SetCandidateWindowCursorVisible(bool visible) {
  connection_->SetCursorVisible(visible);
}

void InputMethodEngineImpl::SetCandidateWindowVertical(bool vertical) {
  connection_->SetOrientationVertical(vertical);
}

void InputMethodEngineImpl::SetCandidateWindowPageSize(int size) {
  connection_->SetPageSize(size);
}

void InputMethodEngineImpl::SetCandidateWindowAuxText(const char* text) {
  connection_->SetCandidateAuxText(text);
}

void InputMethodEngineImpl::SetCandidateWindowAuxTextVisible(bool visible) {
  connection_->SetCandidateAuxTextVisible(visible);
}

bool InputMethodEngineImpl::SetCandidates(
    int context_id, const std::vector<Candidate>& candidates,
    std::string* error) {
  if (!active_) {
    *error = kErrorNotActive;
    return false;
  }
  if (context_id != context_id_ || context_id_ == -1) {
    *error = kErrorWrongContext;
    return false;
  }

  // TODO: Nested candidates
  std::vector<input_method::IBusEngineController::Candidate> ibus_candidates;

  candidate_ids_.clear();
  for (std::vector<Candidate>::const_iterator ix = candidates.begin();
       ix != candidates.end(); ++ix) {
    ibus_candidates.push_back(input_method::IBusEngineController::Candidate());
    ibus_candidates.back().value = ix->value;
    ibus_candidates.back().label = ix->label;
    ibus_candidates.back().annotation = ix->annotation;

    // Store a mapping from the user defined ID to the candidate index.
    candidate_indexs_[ix->id] = candidate_ids_.size();
    candidate_ids_.push_back(ix->id);
  }
  connection_->SetCandidates(ibus_candidates);
  return true;
}

bool InputMethodEngineImpl::SetCursorPosition(int context_id, int candidate_id,
                                              std::string* error) {
  if (!active_) {
    *error = kErrorNotActive;
    return false;
  }
  if (context_id != context_id_ || context_id_ == -1) {
    *error = kErrorWrongContext;
    return false;
  }

  std::map<int, int>::const_iterator position =
      candidate_indexs_.find(candidate_id);
  if (position == candidate_indexs_.end()) {
    *error = kCandidateNotFound;
    return false;
  }

  connection_->SetCursorPosition(position->second);
  return true;
}

void InputMethodEngineImpl::SetMenuItems(const std::vector<MenuItem>& items) {
  // TODO(zork): Implement this function
}

void InputMethodEngineImpl::UpdateMenuItems(
    const std::vector<MenuItem>& items) {
  // TODO(zork): Implement this function
}

void InputMethodEngineImpl::KeyEventDone(input_method::KeyEventHandle* key_data,
                                         bool handled) {
  connection_->KeyEventDone(key_data, handled);
}

void InputMethodEngineImpl::OnReset() {
  // Ignored
}

void InputMethodEngineImpl::OnEnable() {
  active_ = true;
  observer_->OnActivate(engine_id_);
}

void InputMethodEngineImpl::OnDisable() {
  active_ = false;
  observer_->OnDeactivated(engine_id_);
}

void InputMethodEngineImpl::OnFocusIn() {
  context_id_ = next_context_id_;
  ++next_context_id_;

  InputContext context;
  context.id = context_id_;
  // TODO: Other types
  context.type = "text";

  observer_->OnFocus(context);
}

void InputMethodEngineImpl::OnFocusOut() {
  int context_id = context_id_;
  context_id_ = -1;
  observer_->OnBlur(context_id);
}

void InputMethodEngineImpl::OnKeyEvent(bool key_press, unsigned int keyval,
                                       unsigned int keycode, bool alt_key,
                                       bool ctrl_key, bool shift_key,
                                       input_method::KeyEventHandle* key_data) {
  KeyboardEvent event;
  event.type = key_press ? "keydown" : "keyup";
  event.key = input_method::GetIBusKey(keyval);
  event.key_code = input_method::GetIBusKeyCode(keycode);
  event.alt_key = alt_key;
  event.ctrl_key = ctrl_key;
  event.shift_key = shift_key;
  observer_->OnKeyEvent(engine_id_, event, key_data);
}

void InputMethodEngineImpl::OnPropertyActivate(const char* name,
                                               unsigned int state) {
  observer_->OnMenuItemActivated(engine_id_, name);
}

void InputMethodEngineImpl::OnCandidateClicked(unsigned int index,
                                               unsigned int button,
                                               unsigned int state) {
  if (index > candidate_ids_.size()) {
    return;
  }

  int pressed_button = 0;
  if (button & input_method::IBusEngineController::MOUSE_BUTTON_1_MASK) {
    pressed_button = MOUSE_BUTTON_LEFT;
  } else if (button & input_method::IBusEngineController::MOUSE_BUTTON_2_MASK) {
    pressed_button = MOUSE_BUTTON_MIDDLE;
  } else if (button & input_method::IBusEngineController::MOUSE_BUTTON_3_MASK) {
    pressed_button = MOUSE_BUTTON_RIGHT;
  }

  observer_->OnCandidateClicked(engine_id_, candidate_ids_.at(index), button);
}

}  // namespace chromeos
