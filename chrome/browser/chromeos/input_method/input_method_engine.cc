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
#undef RootWindow
#include <map>

#include "ash/ime/input_method_menu_item.h"
#include "ash/ime/input_method_menu_manager.h"
#include "ash/shell.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ime/component_extension_ime_manager.h"
#include "chromeos/ime/composition_text.h"
#include "chromeos/ime/extension_ime_util.h"
#include "chromeos/ime/input_method_manager.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/candidate_window.h"
#include "ui/base/ime/chromeos/ime_keymap.h"
#include "ui/events/event.h"
#include "ui/events/event_processor.h"
#include "ui/events/keycodes/dom4/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion_x.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_util.h"

namespace chromeos {
const char* kErrorNotActive = "IME is not active";
const char* kErrorWrongContext = "Context is not active";
const char* kCandidateNotFound = "Candidate not found";

namespace {

// Notifies InputContextHandler that the composition is changed.
void UpdateComposition(const CompositionText& composition_text,
                       uint32 cursor_pos,
                       bool is_visible) {
  IMEInputContextHandlerInterface* input_context =
      IMEBridge::Get()->GetInputContextHandler();
  if (input_context)
    input_context->UpdateCompositionText(
        composition_text, cursor_pos, is_visible);
}

}  // namespace

InputMethodEngine::InputMethodEngine()
    : current_input_type_(ui::TEXT_INPUT_TYPE_NONE),
      active_(false),
      context_id_(0),
      next_context_id_(1),
      composition_text_(new CompositionText()),
      composition_cursor_(0),
      candidate_window_(new ui::CandidateWindow()),
      window_visible_(false),
      sent_key_event_(NULL) {}

InputMethodEngine::~InputMethodEngine() {
  input_method::InputMethodManager::Get()->RemoveInputMethodExtension(imm_id_);
}

void InputMethodEngine::Initialize(
    scoped_ptr<InputMethodEngineInterface::Observer> observer,
    const char* engine_name,
    const char* extension_id,
    const char* engine_id,
    const std::vector<std::string>& languages,
    const std::vector<std::string>& layouts,
    const GURL& options_page,
    const GURL& input_view) {
  DCHECK(observer) << "Observer must not be null.";

  // TODO(komatsu): It is probably better to set observer out of Initialize.
  observer_ = observer.Pass();
  engine_id_ = engine_id;
  extension_id_ = extension_id;

  input_method::InputMethodManager* manager =
      input_method::InputMethodManager::Get();
  ComponentExtensionIMEManager* comp_ext_ime_manager =
      manager->GetComponentExtensionIMEManager();

  if (comp_ext_ime_manager && comp_ext_ime_manager->IsInitialized() &&
      comp_ext_ime_manager->IsWhitelistedExtension(extension_id)) {
    imm_id_ = comp_ext_ime_manager->GetId(extension_id, engine_id);
  } else {
    imm_id_ = extension_ime_util::GetInputMethodID(extension_id, engine_id);
  }

  input_view_url_ = input_view;
  descriptor_ = input_method::InputMethodDescriptor(
      imm_id_,
      engine_name,
      std::string(), // TODO(uekawa): Set short name.
      layouts,
      languages,
      extension_ime_util::IsKeyboardLayoutExtension(
          imm_id_), // is_login_keyboard
      options_page,
      input_view);

  // TODO(komatsu): It is probably better to call AddInputMethodExtension
  // out of Initialize.
  manager->AddInputMethodExtension(imm_id_, this);
}

const input_method::InputMethodDescriptor& InputMethodEngine::GetDescriptor()
    const {
  return descriptor_;
}

void InputMethodEngine::NotifyImeReady() {
  input_method::InputMethodManager* manager =
      input_method::InputMethodManager::Get();
  if (manager && imm_id_ == manager->GetCurrentInputMethod().id())
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

  composition_cursor_ = cursor;
  composition_text_.reset(new CompositionText());
  composition_text_->set_text(base::UTF8ToUTF16(text));

  composition_text_->set_selection_start(selection_start);
  composition_text_->set_selection_end(selection_end);

  // TODO: Add support for displaying selected text in the composition string.
  for (std::vector<SegmentInfo>::const_iterator segment = segments.begin();
       segment != segments.end(); ++segment) {
    CompositionText::UnderlineAttribute underline;

    switch (segment->style) {
      case SEGMENT_STYLE_UNDERLINE:
        underline.type = CompositionText::COMPOSITION_TEXT_UNDERLINE_SINGLE;
        break;
      case SEGMENT_STYLE_DOUBLE_UNDERLINE:
        underline.type = CompositionText::COMPOSITION_TEXT_UNDERLINE_DOUBLE;
        break;
      default:
        continue;
    }

    underline.start_index = segment->start;
    underline.end_index = segment->end;
    composition_text_->mutable_underline_attributes()->push_back(underline);
  }

  // TODO(nona): Makes focus out mode configuable, if necessary.
  UpdateComposition(*composition_text_, composition_cursor_, true);
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

  composition_cursor_ = 0;
  composition_text_.reset(new CompositionText());
  UpdateComposition(*composition_text_, composition_cursor_, false);
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

  IMEBridge::Get()->GetInputContextHandler()->CommitText(text);
  return true;
}

bool InputMethodEngine::SendKeyEvents(
    int context_id,
    const std::vector<KeyboardEvent>& events) {
  if (!active_) {
    return false;
  }
  // context_id  ==  0, means sending key events to non-input field.
  // context_id_ == -1, means the focus is not in an input field.
  if (context_id != 0 && (context_id != context_id_ || context_id_ == -1)) {
    return false;
  }

  ui::EventProcessor* dispatcher =
      ash::Shell::GetPrimaryRootWindow()->GetHost()->event_processor();

  for (size_t i = 0; i < events.size(); ++i) {
    const KeyboardEvent& event = events[i];
    const ui::EventType type =
        (event.type == "keyup") ? ui::ET_KEY_RELEASED : ui::ET_KEY_PRESSED;

    // KeyboardCodeFromXKyeSym assumes US keyboard layout.
    ui::KeycodeConverter* conv = ui::KeycodeConverter::GetInstance();
    DCHECK(conv);

     // DOM code (KeyA) -> XKB -> XKeySym (XK_A) -> KeyboardCode (VKEY_A)
    const uint16 native_keycode =
        conv->CodeToNativeKeycode(event.code.c_str());
    const uint xkeysym = ui::DefaultXKeysymFromHardwareKeycode(native_keycode);
    const ui::KeyboardCode key_code = ui::KeyboardCodeFromXKeysym(xkeysym);

    const std::string code = event.code;
    int flags = ui::EF_NONE;
    flags |= event.alt_key   ? ui::EF_ALT_DOWN       : ui::EF_NONE;
    flags |= event.ctrl_key  ? ui::EF_CONTROL_DOWN   : ui::EF_NONE;
    flags |= event.shift_key ? ui::EF_SHIFT_DOWN     : ui::EF_NONE;
    flags |= event.caps_lock ? ui::EF_CAPS_LOCK_DOWN : ui::EF_NONE;

    ui::KeyEvent ui_event(type, key_code, code, flags, false /* is_char */);
    base::AutoReset<const ui::KeyEvent*> reset_sent_key(&sent_key_event_,
                                                        &ui_event);
    ui::EventDispatchDetails details = dispatcher->OnEventFromSource(&ui_event);
    if (details.dispatcher_destroyed)
      break;
  }
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
  ui::CandidateWindow::CandidateWindowProperty dest_property;
  dest_property.page_size = property.page_size;
  dest_property.is_cursor_visible = property.is_cursor_visible;
  dest_property.is_vertical = property.is_vertical;
  dest_property.show_window_at_composition =
      property.show_window_at_composition;
  dest_property.cursor_position =
      candidate_window_->GetProperty().cursor_position;
  dest_property.auxiliary_text = property.auxiliary_text;
  dest_property.is_auxiliary_text_visible = property.is_auxiliary_text_visible;

  candidate_window_->SetProperty(dest_property);
  candidate_window_property_ = property;

  if (active_) {
    IMECandidateWindowHandlerInterface* cw_handler =
        IMEBridge::Get()->GetCandidateWindowHandler();
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
  IMECandidateWindowHandlerInterface* cw_handler =
      IMEBridge::Get()->GetCandidateWindowHandler();
  if (cw_handler)
    cw_handler->UpdateLookupTable(*candidate_window_, window_visible_);
  return true;
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
    ui::CandidateWindow::Entry entry;
    entry.value = base::UTF8ToUTF16(ix->value);
    entry.label = base::UTF8ToUTF16(ix->label);
    entry.annotation = base::UTF8ToUTF16(ix->annotation);
    entry.description_title = base::UTF8ToUTF16(ix->usage.title);
    entry.description_body = base::UTF8ToUTF16(ix->usage.body);

    // Store a mapping from the user defined ID to the candidate index.
    candidate_indexes_[ix->id] = candidate_ids_.size();
    candidate_ids_.push_back(ix->id);

    candidate_window_->mutable_candidates()->push_back(entry);
  }
  if (active_) {
    IMECandidateWindowHandlerInterface* cw_handler =
        IMEBridge::Get()->GetCandidateWindowHandler();
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
  IMECandidateWindowHandlerInterface* cw_handler =
      IMEBridge::Get()->GetCandidateWindowHandler();
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

  ash::ime::InputMethodMenuItemList menu_item_list;
  for (std::vector<MenuItem>::const_iterator item = items.begin();
       item != items.end(); ++item) {
    ash::ime::InputMethodMenuItem property;
    MenuItemToProperty(*item, &property);
    menu_item_list.push_back(property);
  }

  ash::ime::InputMethodMenuManager::GetInstance()->
      SetCurrentInputMethodMenuItemList(
          menu_item_list);
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

  IMEInputContextHandlerInterface* input_context =
      IMEBridge::Get()->GetInputContextHandler();
  if (input_context)
    input_context->DeleteSurroundingText(offset, number_of_chars);

  return true;
}

void InputMethodEngine::HideInputView() {
  keyboard::KeyboardController* keyboard_controller =
    keyboard::KeyboardController::GetInstance();
  if (keyboard_controller) {
    keyboard_controller->HideKeyboard(
        keyboard::KeyboardController::HIDE_REASON_MANUAL);
  }
}

void InputMethodEngine::EnableInputView(bool enabled) {
  const GURL& url = enabled ? input_view_url_ : GURL();
  keyboard::SetOverrideContentUrl(url);
  keyboard::KeyboardController* keyboard_controller =
      keyboard::KeyboardController::GetInstance();
  if (keyboard_controller)
    keyboard_controller->Reload();
}

void InputMethodEngine::FocusIn(
    const IMEEngineHandlerInterface::InputContext& input_context) {
  current_input_type_ = input_context.type;
  if (!active_)
    return;

  // Prevent sending events on password field to 3rd-party IME extensions.
  // And also make sure the VK fallback to system VK.
  // TODO(shuchen): for password field, forcibly switch/lock the IME to the XKB
  // keyboard related to the current IME.
  if (current_input_type_ == ui::TEXT_INPUT_TYPE_PASSWORD &&
      !extension_ime_util::IsComponentExtensionIME(GetDescriptor().id())) {
    EnableInputView(false);
    return;
  }

  context_id_ = next_context_id_;
  ++next_context_id_;

  InputMethodEngineInterface::InputContext context;
  context.id = context_id_;
  switch (current_input_type_) {
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
    case ui::TEXT_INPUT_TYPE_PASSWORD:
      context.type = "password";
      break;
    default:
      context.type = "text";
      break;
  }

  observer_->OnFocus(context);
}

void InputMethodEngine::FocusOut() {
  ui::TextInputType input_type = current_input_type_;
  current_input_type_ = ui::TEXT_INPUT_TYPE_NONE;
  if (!active_)
    return;

  // Prevent sending events on password field to 3rd-party IME extensions.
  // And also make sure the VK restore to IME input view.
  if (input_type == ui::TEXT_INPUT_TYPE_PASSWORD &&
      !extension_ime_util::IsComponentExtensionIME(GetDescriptor().id())) {
    EnableInputView(true);
    return;
  }

  int context_id = context_id_;
  context_id_ = -1;
  observer_->OnBlur(context_id);
}

void InputMethodEngine::Enable() {
  active_ = true;
  observer_->OnActivate(engine_id_);
  if (current_input_type_ == ui::TEXT_INPUT_TYPE_NONE)
    current_input_type_ = ui::TEXT_INPUT_TYPE_TEXT;
  FocusIn(IMEEngineHandlerInterface::InputContext(
      current_input_type_, ui::TEXT_INPUT_MODE_DEFAULT));
  EnableInputView(true);
}

void InputMethodEngine::Disable() {
  active_ = false;
  observer_->OnDeactivated(engine_id_);
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

  uint32 x11_keysym = 0;
  if (event.HasNativeEvent()) {
    const base::NativeEvent& native_event = event.native_event();
    DCHECK(native_event);

    XKeyEvent* x_key = &(static_cast<XEvent*>(native_event)->xkey);
    KeySym keysym = NoSymbol;
    ::XLookupString(x_key, NULL, 0, &keysym, NULL);
    x11_keysym = keysym;
  } else {
    // Convert ui::KeyEvent.key_code to DOM UIEvent key.
    // XKeysymForWindowsKeyCode converts key_code to XKeySym, but it
    // assumes US layout and does not care about CapLock state.
    //
    // TODO(komatsu): Support CapsLock states.
    // TODO(komatsu): Support non-us keyboard layouts.
    x11_keysym = ui::XKeysymForWindowsKeyCode(event.key_code(),
                                              event.IsShiftDown());
  }
  ext_event->key = ui::FromXKeycodeToKeyValue(x11_keysym);
}
}  // namespace

void InputMethodEngine::ProcessKeyEvent(
    const ui::KeyEvent& key_event,
    const KeyEventDoneCallback& callback) {

  KeyEventDoneCallback *handler = new KeyEventDoneCallback();
  *handler = callback;

  KeyboardEvent ext_event;
  GetExtensionKeyboardEventFromKeyEvent(key_event, &ext_event);

  // If the given key event is equal to the key event sent by
  // SendKeyEvents, this engine ID is propagated to the extension IME.
  // Note, this check relies on that ui::KeyEvent is propagated as
  // reference without copying.
  if (&key_event == sent_key_event_)
    ext_event.extension_id = extension_id_;

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

// TODO(uekawa): rename this method to a more reasonable name.
void InputMethodEngine::MenuItemToProperty(
    const MenuItem& item,
    ash::ime::InputMethodMenuItem* property) {
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
