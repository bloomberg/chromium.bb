// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/input_method_engine.h"

#define XK_MISCELLANY
#include <X11/keysymdef.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#undef FocusIn
#undef FocusOut
#include <map>

#include "ash/shell.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ime/candidate_window.h"
#include "chromeos/ime/component_extension_ime_manager.h"
#include "chromeos/ime/extension_ime_util.h"
#include "chromeos/ime/ibus_keymap.h"
#include "chromeos/ime/ibus_text.h"
#include "chromeos/ime/input_method_manager.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_code_conversion_x.h"
#include "ui/keyboard/keyboard_controller.h"

namespace chromeos {
const char* kErrorNotActive = "IME is not active";
const char* kErrorWrongContext = "Context is not active";
const char* kCandidateNotFound = "Candidate not found";
const char* kEngineBusPrefix = "org.freedesktop.IBus.";

namespace {

// Notifies InputContextHandler that the preedit is changed.
void UpdatePreedit(const IBusText& ibus_text,
                   uint32 cursor_pos,
                   bool is_visible) {
  IBusInputContextHandlerInterface* input_context =
      IBusBridge::Get()->GetInputContextHandler();
  if (input_context)
    input_context->UpdatePreeditText(ibus_text, cursor_pos, is_visible);
}

// Notifies CandidateWindowHandler that the auxilary text is changed.
// Auxilary text is usually footer text.
void UpdateAuxiliaryText(const std::string& text, bool is_visible) {
  IBusPanelCandidateWindowHandlerInterface* candidate_window =
      IBusBridge::Get()->GetCandidateWindowHandler();
  if (candidate_window)
    candidate_window->UpdateAuxiliaryText(text, is_visible);
}

}  // namespace

InputMethodEngine::InputMethodEngine()
    : focused_(false),
      active_(false),
      context_id_(0),
      next_context_id_(1),
      aux_text_visible_(false),
      observer_(NULL),
      preedit_text_(new IBusText()),
      preedit_cursor_(0),
      candidate_window_(new input_method::CandidateWindow()),
      window_visible_(false) {}

InputMethodEngine::~InputMethodEngine() {
  input_method::InputMethodManager::Get()->RemoveInputMethodExtension(ibus_id_);
}

void InputMethodEngine::Initialize(
    InputMethodEngineInterface::Observer* observer,
    const char* engine_name,
    const char* extension_id,
    const char* engine_id,
    const std::vector<std::string>& languages,
    const std::vector<std::string>& layouts,
    const GURL& options_page,
    const GURL& input_view) {
  DCHECK(observer) << "Observer must not be null.";

  observer_ = observer;
  engine_id_ = engine_id;

  input_method::InputMethodManager* manager =
      input_method::InputMethodManager::Get();
  ComponentExtensionIMEManager* comp_ext_ime_manager
      = manager->GetComponentExtensionIMEManager();

  if (comp_ext_ime_manager->IsInitialized() &&
      comp_ext_ime_manager->IsWhitelistedExtension(extension_id)) {
    ibus_id_ = comp_ext_ime_manager->GetId(extension_id, engine_id);
  } else {
    ibus_id_ = extension_ime_util::GetInputMethodID(extension_id, engine_id);
  }

  input_view_url_ = input_view;

  manager->AddInputMethodExtension(ibus_id_, engine_name, layouts, languages,
                                   options_page, input_view, this);
  IBusBridge::Get()->SetEngineHandler(ibus_id_, this);
}

void InputMethodEngine::StartIme() {
  input_method::InputMethodManager* manager =
      input_method::InputMethodManager::Get();
  if (manager && ibus_id_ == manager->GetCurrentInputMethod().id())
    Enable();
}

bool InputMethodEngine::SetComposition(
    int context_id,
    const char* text,
    int selection_start,
    int selection_end,
    int cursor,
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

  preedit_cursor_ = cursor;
  preedit_text_.reset(new IBusText());
  preedit_text_->set_text(text);

  preedit_text_->set_selection_start(selection_start);
  preedit_text_->set_selection_end(selection_end);

  // TODO: Add support for displaying selected text in the composition string.
  for (std::vector<SegmentInfo>::const_iterator segment = segments.begin();
       segment != segments.end(); ++segment) {
    IBusText::UnderlineAttribute underline;

    switch (segment->style) {
      case SEGMENT_STYLE_UNDERLINE:
        underline.type = IBusText::IBUS_TEXT_UNDERLINE_SINGLE;
        break;
      case SEGMENT_STYLE_DOUBLE_UNDERLINE:
        underline.type = IBusText::IBUS_TEXT_UNDERLINE_DOUBLE;
        break;
      default:
        continue;
    }

    underline.start_index = segment->start;
    underline.end_index = segment->end;
    preedit_text_->mutable_underline_attributes()->push_back(underline);
  }

  // TODO(nona): Makes focus out mode configuable, if necessary.
  UpdatePreedit(*preedit_text_, preedit_cursor_, true);
  return true;
}

bool InputMethodEngine::ClearComposition(int context_id,
                                         std::string* error)  {
  if (!active_) {
    *error = kErrorNotActive;
    return false;
  }
  if (context_id != context_id_ || context_id_ == -1) {
    *error = kErrorWrongContext;
    return false;
  }

  preedit_cursor_ = 0;
  preedit_text_.reset(new IBusText());
  UpdatePreedit(*preedit_text_, preedit_cursor_, false);
  return true;
}

bool InputMethodEngine::CommitText(int context_id, const char* text,
                                   std::string* error) {
  if (!active_) {
    // TODO: Commit the text anyways.
    *error = kErrorNotActive;
    return false;
  }
  if (context_id != context_id_ || context_id_ == -1) {
    *error = kErrorWrongContext;
    return false;
  }

  IBusBridge::Get()->GetInputContextHandler()->CommitText(text);
  return true;
}

const InputMethodEngine::CandidateWindowProperty&
InputMethodEngine::GetCandidateWindowProperty() const {
  return candidate_window_property_;
}

void InputMethodEngine::SetCandidateWindowProperty(
    const CandidateWindowProperty& property) {
  // Type conversion from InputMethodEngineInterface::CandidateWindowProperty to
  // CandidateWindow::CandidateWindowProperty defined in chromeos/ime/.
  input_method::CandidateWindow::CandidateWindowProperty dest_property;
  dest_property.page_size = property.page_size;
  dest_property.is_cursor_visible = property.is_cursor_visible;
  dest_property.is_vertical = property.is_vertical;
  dest_property.show_window_at_composition =
      property.show_window_at_composition;
  dest_property.cursor_position =
      candidate_window_->GetProperty().cursor_position;
  candidate_window_->SetProperty(dest_property);
  candidate_window_property_ = property;

  if (active_) {
    IBusPanelCandidateWindowHandlerInterface* cw_handler =
        IBusBridge::Get()->GetCandidateWindowHandler();
    if (cw_handler)
      cw_handler->UpdateLookupTable(*candidate_window_, window_visible_);
  }
}

bool InputMethodEngine::SetCandidateWindowVisible(bool visible,
                                                  std::string* error) {
  if (!active_) {
    *error = kErrorNotActive;
    return false;
  }

  window_visible_ = visible;
  IBusPanelCandidateWindowHandlerInterface* cw_handler =
    IBusBridge::Get()->GetCandidateWindowHandler();
  if (cw_handler)
    cw_handler->UpdateLookupTable(*candidate_window_, window_visible_);
  return true;
}

void InputMethodEngine::SetCandidateWindowAuxText(const char* text) {
  aux_text_.assign(text);
  if (active_) {
    // Should not show auxiliary text if the whole window visibility is false.
    UpdateAuxiliaryText(aux_text_, window_visible_ && aux_text_visible_);
  }
}

void InputMethodEngine::SetCandidateWindowAuxTextVisible(bool visible) {
  aux_text_visible_ = visible;
  if (active_) {
    // Should not show auxiliary text if the whole window visibility is false.
    UpdateAuxiliaryText(aux_text_, window_visible_ && aux_text_visible_);
  }
}

bool InputMethodEngine::SetCandidates(
    int context_id,
    const std::vector<Candidate>& candidates,
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
  candidate_ids_.clear();
  candidate_indexes_.clear();
  candidate_window_->mutable_candidates()->clear();
  for (std::vector<Candidate>::const_iterator ix = candidates.begin();
       ix != candidates.end(); ++ix) {
    input_method::CandidateWindow::Entry entry;
    entry.value = ix->value;
    entry.label = ix->label;
    entry.annotation = ix->annotation;
    entry.description_title = ix->usage.title;
    entry.description_body = ix->usage.body;

    // Store a mapping from the user defined ID to the candidate index.
    candidate_indexes_[ix->id] = candidate_ids_.size();
    candidate_ids_.push_back(ix->id);

    candidate_window_->mutable_candidates()->push_back(entry);
  }
  if (active_) {
    IBusPanelCandidateWindowHandlerInterface* cw_handler =
      IBusBridge::Get()->GetCandidateWindowHandler();
    if (cw_handler)
      cw_handler->UpdateLookupTable(*candidate_window_, window_visible_);
  }
  return true;
}

bool InputMethodEngine::SetCursorPosition(int context_id, int candidate_id,
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
      candidate_indexes_.find(candidate_id);
  if (position == candidate_indexes_.end()) {
    *error = kCandidateNotFound;
    return false;
  }

  candidate_window_->set_cursor_position(position->second);
  IBusPanelCandidateWindowHandlerInterface* cw_handler =
    IBusBridge::Get()->GetCandidateWindowHandler();
  if (cw_handler)
    cw_handler->UpdateLookupTable(*candidate_window_, window_visible_);
  return true;
}

bool InputMethodEngine::SetMenuItems(const std::vector<MenuItem>& items) {
  return UpdateMenuItems(items);
}

bool InputMethodEngine::UpdateMenuItems(
    const std::vector<MenuItem>& items) {
  if (!active_)
    return false;

  input_method::InputMethodPropertyList property_list;
  for (std::vector<MenuItem>::const_iterator item = items.begin();
       item != items.end(); ++item) {
    input_method::InputMethodProperty property;
    MenuItemToProperty(*item, &property);
    property_list.push_back(property);
  }

  input_method::InputMethodManager* manager =
      input_method::InputMethodManager::Get();
  if (manager)
    manager->SetCurrentInputMethodProperties(property_list);

  return true;
}

bool InputMethodEngine::IsActive() const {
  return active_;
}

void InputMethodEngine::KeyEventDone(input_method::KeyEventHandle* key_data,
                                     bool handled) {
  KeyEventDoneCallback* callback =
      reinterpret_cast<KeyEventDoneCallback*>(key_data);
  callback->Run(handled);
  delete callback;
}

bool InputMethodEngine::DeleteSurroundingText(int context_id,
                                              int offset,
                                              size_t number_of_chars,
                                              std::string* error) {
  if (!active_) {
    *error = kErrorNotActive;
    return false;
  }
  if (context_id != context_id_ || context_id_ == -1) {
    *error = kErrorWrongContext;
    return false;
  }

  if (offset < 0 && static_cast<size_t>(-1 * offset) != size_t(number_of_chars))
    return false;  // Currently we can only support preceding text.

  // TODO(nona): Return false if there is ongoing composition.

  IBusInputContextHandlerInterface* input_context =
      IBusBridge::Get()->GetInputContextHandler();
  if (input_context)
    input_context->DeleteSurroundingText(offset, number_of_chars);

  return true;
}

void InputMethodEngine::FocusIn(
    const IBusEngineHandlerInterface::InputContext& input_context) {
  focused_ = true;
  if (!active_)
    return;
  context_id_ = next_context_id_;
  ++next_context_id_;

  InputMethodEngineInterface::InputContext context;
  context.id = context_id_;
  switch (input_context.type) {
    case ui::TEXT_INPUT_TYPE_SEARCH:
      context.type = "search";
      break;
    case ui::TEXT_INPUT_TYPE_TELEPHONE:
      context.type = "tel";
      break;
    case ui::TEXT_INPUT_TYPE_URL:
      context.type = "url";
      break;
    case ui::TEXT_INPUT_TYPE_EMAIL:
      context.type = "email";
      break;
    case ui::TEXT_INPUT_TYPE_NUMBER:
      context.type = "number";
      break;
    default:
      context.type = "text";
      break;
  }

  observer_->OnFocus(context);
}

void InputMethodEngine::FocusOut() {
  focused_ = false;
  if (!active_)
    return;
  int context_id = context_id_;
  context_id_ = -1;
  observer_->OnBlur(context_id);
}

void InputMethodEngine::Enable() {
  active_ = true;
  observer_->OnActivate(engine_id_);
  IBusEngineHandlerInterface::InputContext context(ui::TEXT_INPUT_TYPE_TEXT,
                                                   ui::TEXT_INPUT_MODE_DEFAULT);
  FocusIn(context);

  keyboard::KeyboardController* keyboard_controller =
      ash::Shell::GetInstance()->keyboard_controller();
  if (keyboard_controller) {
    keyboard_controller->SetOverrideContentUrl(input_view_url_);
  }
}

void InputMethodEngine::Disable() {
  active_ = false;
  observer_->OnDeactivated(engine_id_);

  keyboard::KeyboardController* keyboard_controller =
      ash::Shell::GetInstance()->keyboard_controller();
  if (keyboard_controller) {
    GURL empty_url;
    keyboard_controller->SetOverrideContentUrl(empty_url);
  }
}

void InputMethodEngine::PropertyActivate(const std::string& property_name) {
  observer_->OnMenuItemActivated(engine_id_, property_name);
}

void InputMethodEngine::Reset() {
  observer_->OnReset(engine_id_);
}

namespace {
void GetExtensionKeyboardEventFromKeyEvent(
    const ui::KeyEvent& event,
    InputMethodEngine::KeyboardEvent* ext_event) {
  DCHECK(event.type() == ui::ET_KEY_RELEASED ||
         event.type() == ui::ET_KEY_PRESSED);
  DCHECK(ext_event);
  ext_event->type = (event.type() == ui::ET_KEY_RELEASED) ? "keyup" : "keydown";

  ext_event->code = event.code();
  ext_event->alt_key = event.IsAltDown();
  ext_event->ctrl_key = event.IsControlDown();
  ext_event->shift_key = event.IsShiftDown();
  ext_event->caps_lock = event.IsCapsLockDown();

  uint32 ibus_keyval = 0;
  if (event.HasNativeEvent()) {
    const base::NativeEvent& native_event = event.native_event();
    DCHECK(native_event);

    XKeyEvent* x_key = &(static_cast<XEvent*>(native_event)->xkey);
    KeySym keysym = NoSymbol;
    ::XLookupString(x_key, NULL, 0, &keysym, NULL);
    ibus_keyval = keysym;
  } else {
    // Convert ui::KeyEvent.key_code to DOM UIEvent key.
    // XKeysymForWindowsKeyCode converts key_code to XKeySym, but it
    // assumes US layout and does not care about CapLock state.
    //
    // TODO(komatsu): Support CapsLock states.
    // TODO(komatsu): Support non-us keyboard layouts.
    ibus_keyval = ui::XKeysymForWindowsKeyCode(event.key_code(),
                                               event.IsShiftDown());
  }
  ext_event->key = input_method::GetIBusKey(ibus_keyval);
}
}  // namespace

void InputMethodEngine::ProcessKeyEvent(
    const ui::KeyEvent& key_event,
    const KeyEventDoneCallback& callback) {

  KeyEventDoneCallback *handler = new KeyEventDoneCallback();
  *handler = callback;

  KeyboardEvent ext_event;
  GetExtensionKeyboardEventFromKeyEvent(key_event, &ext_event);
  observer_->OnKeyEvent(
      engine_id_,
      ext_event,
      reinterpret_cast<input_method::KeyEventHandle*>(handler));
}

void InputMethodEngine::CandidateClicked(uint32 index) {
  if (index > candidate_ids_.size()) {
    return;
  }

  // Only left button click is supported at this moment.
  observer_->OnCandidateClicked(
      engine_id_, candidate_ids_.at(index), MOUSE_BUTTON_LEFT);
}

void InputMethodEngine::SetSurroundingText(const std::string& text,
                                           uint32 cursor_pos,
                                           uint32 anchor_pos) {
  observer_->OnSurroundingTextChanged(engine_id_,
                                      text,
                                      static_cast<int>(cursor_pos),
                                      static_cast<int>(anchor_pos));
}

void InputMethodEngine::MenuItemToProperty(
    const MenuItem& item,
    input_method::InputMethodProperty* property) {
  property->key = item.id;

  if (item.modified & MENU_ITEM_MODIFIED_LABEL) {
    property->label = item.label;
  }
  if (item.modified & MENU_ITEM_MODIFIED_VISIBLE) {
    // TODO(nona): Implement it.
  }
  if (item.modified & MENU_ITEM_MODIFIED_CHECKED) {
    property->is_selection_item_checked = item.checked;
  }
  if (item.modified & MENU_ITEM_MODIFIED_ENABLED) {
    // TODO(nona): implement sensitive entry(crbug.com/140192).
  }
  if (item.modified & MENU_ITEM_MODIFIED_STYLE) {
    if (!item.children.empty()) {
      // TODO(nona): Implement it.
    } else {
      switch (item.style) {
        case MENU_ITEM_STYLE_NONE:
          NOTREACHED();
          break;
        case MENU_ITEM_STYLE_CHECK:
          // TODO(nona): Implement it.
          break;
        case MENU_ITEM_STYLE_RADIO:
          property->is_selection_item = true;
          break;
        case MENU_ITEM_STYLE_SEPARATOR:
          // TODO(nona): Implement it.
          break;
      }
    }
  }

  // TODO(nona): Support item.children.
}

}  // namespace chromeos
