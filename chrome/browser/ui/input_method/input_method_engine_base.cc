// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/input_method/input_method_engine_base.h"

#include <memory>

#undef FocusIn
#undef FocusOut
#undef RootWindow
#include <algorithm>
#include <map>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/ime_bridge.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/events/event.h"
#include "ui/events/event_processor.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_util.h"

#if defined(OS_CHROMEOS)
#include "ui/base/ime/chromeos/ime_keymap.h"
#elif defined(OS_WIN)
#include "ui/events/keycodes/keyboard_codes_win.h"
#elif defined(OS_LINUX)
#include "ui/events/keycodes/keyboard_codes_posix.h"
#endif

namespace input_method {

namespace {

const char kErrorNotActive[] = "IME is not active";
const char kErrorWrongContext[] = "Context is not active";

#if defined(OS_CHROMEOS)
std::string GetKeyFromEvent(const ui::KeyEvent& event) {
  const std::string code = event.GetCodeString();
  if (base::StartsWith(code, "Control", base::CompareCase::SENSITIVE))
    return "Ctrl";
  if (base::StartsWith(code, "Shift", base::CompareCase::SENSITIVE))
    return "Shift";
  if (base::StartsWith(code, "Alt", base::CompareCase::SENSITIVE))
    return "Alt";
  if (base::StartsWith(code, "Arrow", base::CompareCase::SENSITIVE))
    return code.substr(5);
  if (code == "Escape")
    return "Esc";
  if (code == "Backspace" || code == "Tab" || code == "Enter" ||
      code == "CapsLock" || code == "Power")
    return code;
  // Cases for media keys.
  switch (event.key_code()) {
    case ui::VKEY_BROWSER_BACK:
    case ui::VKEY_F1:
      return "HistoryBack";
    case ui::VKEY_BROWSER_FORWARD:
    case ui::VKEY_F2:
      return "HistoryForward";
    case ui::VKEY_BROWSER_REFRESH:
    case ui::VKEY_F3:
      return "BrowserRefresh";
    case ui::VKEY_MEDIA_LAUNCH_APP2:
    case ui::VKEY_F4:
      return "ChromeOSFullscreen";
    case ui::VKEY_MEDIA_LAUNCH_APP1:
    case ui::VKEY_F5:
      return "ChromeOSSwitchWindow";
    case ui::VKEY_BRIGHTNESS_DOWN:
    case ui::VKEY_F6:
      return "BrightnessDown";
    case ui::VKEY_BRIGHTNESS_UP:
    case ui::VKEY_F7:
      return "BrightnessUp";
    case ui::VKEY_VOLUME_MUTE:
    case ui::VKEY_F8:
      return "AudioVolumeMute";
    case ui::VKEY_VOLUME_DOWN:
    case ui::VKEY_F9:
      return "AudioVolumeDown";
    case ui::VKEY_VOLUME_UP:
    case ui::VKEY_F10:
      return "AudioVolumeUp";
    default:
      break;
  }
  uint16_t ch = 0;
  // Ctrl+? cases, gets key value for Ctrl is not down.
  if (event.flags() & ui::EF_CONTROL_DOWN) {
    ui::KeyEvent event_no_ctrl(event.type(), event.key_code(),
                               event.flags() ^ ui::EF_CONTROL_DOWN);
    ch = event_no_ctrl.GetCharacter();
  } else {
    ch = event.GetCharacter();
  }
  return base::UTF16ToUTF8(base::string16(1, ch));
}
#endif  // defined(OS_CHROMEOS)

void GetExtensionKeyboardEventFromKeyEvent(
    const ui::KeyEvent& event,
    InputMethodEngineBase::KeyboardEvent* ext_event) {
  DCHECK(event.type() == ui::ET_KEY_RELEASED ||
         event.type() == ui::ET_KEY_PRESSED);
  DCHECK(ext_event);
  ext_event->type = (event.type() == ui::ET_KEY_RELEASED) ? "keyup" : "keydown";

  if (event.code() == ui::DomCode::NONE) {
// TODO(azurewei): Use KeycodeConverter::DomCodeToCodeString on all platforms
#if defined(OS_CHROMEOS)
    ext_event->code = ui::KeyboardCodeToDomKeycode(event.key_code());
#else
    ext_event->code =
        std::string(ui::KeycodeConverter::DomCodeToCodeString(event.code()));
#endif
  } else {
    ext_event->code = event.GetCodeString();
  }
  ext_event->key_code = static_cast<int>(event.key_code());
  ext_event->alt_key = event.IsAltDown();
  ext_event->ctrl_key = event.IsControlDown();
  ext_event->shift_key = event.IsShiftDown();
  ext_event->caps_lock = event.IsCapsLockOn();
#if defined(OS_CHROMEOS)
  ext_event->key = GetKeyFromEvent(event);
#else
  ext_event->key = ui::KeycodeConverter::DomKeyToKeyString(event.GetDomKey());
#endif  // defined(OS_CHROMEOS)
}

}  // namespace

InputMethodEngineBase::KeyboardEvent::KeyboardEvent()
    : alt_key(false), ctrl_key(false), shift_key(false), caps_lock(false) {}

InputMethodEngineBase::KeyboardEvent::KeyboardEvent(
    const KeyboardEvent& other) = default;

InputMethodEngineBase::KeyboardEvent::~KeyboardEvent() {}

InputMethodEngineBase::InputMethodEngineBase()
    : current_input_type_(ui::TEXT_INPUT_TYPE_NONE),
      context_id_(0),
      next_context_id_(1),
      composition_text_(new ui::CompositionText()),
      composition_cursor_(0),
      sent_key_event_(nullptr),
      profile_(nullptr),
      next_request_id_(1),
      composition_changed_(false),
      text_(""),
      commit_text_changed_(false),
      handling_key_event_(false) {}

InputMethodEngineBase::~InputMethodEngineBase() {}

void InputMethodEngineBase::Initialize(
    std::unique_ptr<InputMethodEngineBase::Observer> observer,
    const char* extension_id,
    Profile* profile) {
  DCHECK(observer) << "Observer must not be null.";

  // TODO(komatsu): It is probably better to set observer out of Initialize.
  observer_ = std::move(observer);
  extension_id_ = extension_id;
  profile_ = profile;
}

const std::string& InputMethodEngineBase::GetActiveComponentId() const {
  return active_component_id_;
}

bool InputMethodEngineBase::SetComposition(
    int context_id,
    const char* text,
    int selection_start,
    int selection_end,
    int cursor,
    const std::vector<SegmentInfo>& segments,
    std::string* error) {
  if (!IsActive()) {
    *error = kErrorNotActive;
    return false;
  }
  if (context_id != context_id_ || context_id_ == -1) {
    *error = kErrorWrongContext;
    return false;
  }

  composition_cursor_ = cursor;
  composition_text_.reset(new ui::CompositionText());
  composition_text_->text = base::UTF8ToUTF16(text);

  composition_text_->selection.set_start(selection_start);
  composition_text_->selection.set_end(selection_end);

  // TODO: Add support for displaying selected text in the composition string.
  for (std::vector<SegmentInfo>::const_iterator segment = segments.begin();
       segment != segments.end(); ++segment) {
    ui::ImeTextSpan ime_text_span;

    switch (segment->style) {
      case SEGMENT_STYLE_UNDERLINE:
        ime_text_span.underline_color = SK_ColorBLACK;
        break;
      case SEGMENT_STYLE_DOUBLE_UNDERLINE:
        ime_text_span.underline_color = SK_ColorBLACK;
        ime_text_span.thick = true;
        break;
      case SEGMENT_STYLE_NO_UNDERLINE:
        ime_text_span.underline_color = SK_ColorTRANSPARENT;
        break;
      default:
        continue;
    }

    ime_text_span.start_offset = segment->start;
    ime_text_span.end_offset = segment->end;
    composition_text_->ime_text_spans.push_back(ime_text_span);
  }

  // TODO(nona): Makes focus out mode configuable, if necessary.
  UpdateComposition(*composition_text_, composition_cursor_, true);
  return true;
}

bool InputMethodEngineBase::ClearComposition(int context_id,
                                             std::string* error) {
  if (!IsActive()) {
    *error = kErrorNotActive;
    return false;
  }
  if (context_id != context_id_ || context_id_ == -1) {
    *error = kErrorWrongContext;
    return false;
  }

  composition_cursor_ = 0;
  composition_text_.reset(new ui::CompositionText());
  UpdateComposition(*composition_text_, composition_cursor_, false);
  return true;
}

bool InputMethodEngineBase::CommitText(int context_id,
                                       const char* text,
                                       std::string* error) {
  if (!IsActive()) {
    // TODO: Commit the text anyways.
    *error = kErrorNotActive;
    return false;
  }
  if (context_id != context_id_ || context_id_ == -1) {
    *error = kErrorWrongContext;
    return false;
  }

  CommitTextToInputContext(context_id, std::string(text));
  return true;
}

bool InputMethodEngineBase::DeleteSurroundingText(int context_id,
                                                  int offset,
                                                  size_t number_of_chars,
                                                  std::string* error) {
  if (!IsActive()) {
    *error = kErrorNotActive;
    return false;
  }
  if (context_id != context_id_ || context_id_ == -1) {
    *error = kErrorWrongContext;
    return false;
  }

  // TODO(nona): Return false if there is ongoing composition.

  ui::IMEInputContextHandlerInterface* input_context =
      ui::IMEBridge::Get()->GetInputContextHandler();
  if (input_context)
    input_context->DeleteSurroundingText(offset, number_of_chars);

  return true;
}

void InputMethodEngineBase::SetCompositionBounds(
    const std::vector<gfx::Rect>& bounds) {
  observer_->OnCompositionBoundsChanged(bounds);
}

void InputMethodEngineBase::FocusIn(
    const ui::IMEEngineHandlerInterface::InputContext& input_context) {
  current_input_type_ = input_context.type;

  if (!IsActive() || current_input_type_ == ui::TEXT_INPUT_TYPE_NONE)
    return;

  context_id_ = next_context_id_;
  ++next_context_id_;

  observer_->OnFocus(ui::IMEEngineHandlerInterface::InputContext(
      context_id_, input_context.type, input_context.mode,
      input_context.flags));
}

void InputMethodEngineBase::FocusOut() {
  if (!IsActive() || current_input_type_ == ui::TEXT_INPUT_TYPE_NONE)
    return;

  current_input_type_ = ui::TEXT_INPUT_TYPE_NONE;

  int context_id = context_id_;
  context_id_ = -1;
  observer_->OnBlur(context_id);
}

void InputMethodEngineBase::Enable(const std::string& component_id) {
  active_component_id_ = component_id;
  observer_->OnActivate(component_id);
  const ui::IMEEngineHandlerInterface::InputContext& input_context =
      ui::IMEBridge::Get()->GetCurrentInputContext();
  current_input_type_ = input_context.type;
  FocusIn(input_context);
}

void InputMethodEngineBase::Disable() {
  active_component_id_.clear();
  if (ui::IMEBridge::Get()->GetInputContextHandler())
    ui::IMEBridge::Get()->GetInputContextHandler()->CommitText(
        base::UTF16ToUTF8(composition_text_->text));
  composition_text_.reset(new ui::CompositionText());
  observer_->OnDeactivated(active_component_id_);
}

void InputMethodEngineBase::Reset() {
  composition_text_.reset(new ui::CompositionText());
  observer_->OnReset(active_component_id_);
}

void InputMethodEngineBase::MaybeSwitchEngine() {
  observer_->OnRequestEngineSwitch();
}

bool InputMethodEngineBase::IsInterestedInKeyEvent() const {
  return observer_->IsInterestedInKeyEvent();
}

void InputMethodEngineBase::ProcessKeyEvent(const ui::KeyEvent& key_event,
                                            KeyEventDoneCallback& callback) {
  // Make true that we don't handle IME API calling of setComposition and
  // commitText while the extension is handling key event.
  handling_key_event_ = true;

  if (key_event.IsCommandDown()) {
    callback.Run(false);
    return;
  }

  KeyboardEvent ext_event;
  GetExtensionKeyboardEventFromKeyEvent(key_event, &ext_event);

  // If the given key event is equal to the key event sent by
  // SendKeyEvents, this engine ID is propagated to the extension IME.
  // Note, this check relies on that ui::KeyEvent is propagated as
  // reference without copying.
  if (&key_event == sent_key_event_)
    ext_event.extension_id = extension_id_;

  // Should not pass key event in password field.
  if (current_input_type_ != ui::TEXT_INPUT_TYPE_PASSWORD)
    observer_->OnKeyEvent(active_component_id_, ext_event, callback);
}

void InputMethodEngineBase::SetSurroundingText(const std::string& text,
                                               uint32_t cursor_pos,
                                               uint32_t anchor_pos,
                                               uint32_t offset_pos) {
  observer_->OnSurroundingTextChanged(
      active_component_id_, text, static_cast<int>(cursor_pos),
      static_cast<int>(anchor_pos), static_cast<int>(offset_pos));
}

void InputMethodEngineBase::KeyEventHandled(const std::string& extension_id,
                                            const std::string& request_id,
                                            bool handled) {
  handling_key_event_ = false;
  // When finish handling key event, take care of the unprocessed commitText
  // and setComposition calls.
  ui::IMEInputContextHandlerInterface* input_context =
      ui::IMEBridge::Get()->GetInputContextHandler();
  if (commit_text_changed_) {
    if (input_context) {
      input_context->CommitText(text_);
    }
    text_ = "";
    commit_text_changed_ = false;
  }

  if (composition_changed_) {
    if (input_context) {
      input_context->UpdateCompositionText(
          composition_, composition_.selection.start(), true);
    }
    composition_ = ui::CompositionText();
    composition_changed_ = false;
  }

  RequestMap::iterator request = request_map_.find(request_id);
  if (request == request_map_.end()) {
    LOG(ERROR) << "Request ID not found: " << request_id;
    return;
  }

  request->second.second.Run(handled);
  request_map_.erase(request);
}

std::string InputMethodEngineBase::AddRequest(
    const std::string& component_id,
    ui::IMEEngineHandlerInterface::KeyEventDoneCallback& key_data) {
  std::string request_id = base::IntToString(next_request_id_);
  ++next_request_id_;

  request_map_[request_id] = std::make_pair(component_id, key_data);

  return request_id;
}

bool InputMethodEngineBase::SendKeyEvents(
    int context_id,
    const std::vector<KeyboardEvent>& events) {
  // context_id  ==  0, means sending key events to non-input field.
  // context_id_ == -1, means the focus is not in an input field.
  if (!IsActive() ||
      (context_id != 0 && (context_id != context_id_ || context_id_ == -1)))
    return false;

  for (size_t i = 0; i < events.size(); ++i) {
    const KeyboardEvent& event = events[i];
    const ui::EventType type =
        (event.type == "keyup") ? ui::ET_KEY_RELEASED : ui::ET_KEY_PRESSED;
    ui::KeyboardCode key_code = static_cast<ui::KeyboardCode>(event.key_code);

    int flags = ui::EF_NONE;
    flags |= event.alt_key ? ui::EF_ALT_DOWN : ui::EF_NONE;
    flags |= event.ctrl_key ? ui::EF_CONTROL_DOWN : ui::EF_NONE;
    flags |= event.shift_key ? ui::EF_SHIFT_DOWN : ui::EF_NONE;
    flags |= event.caps_lock ? ui::EF_CAPS_LOCK_ON : ui::EF_NONE;

    ui::KeyEvent ui_event(
        type, key_code, ui::KeycodeConverter::CodeStringToDomCode(event.code),
        flags, ui::KeycodeConverter::KeyStringToDomKey(event.key),
        ui::EventTimeForNow());
    base::AutoReset<const ui::KeyEvent*> reset_sent_key(&sent_key_event_,
                                                        &ui_event);
    if (!SendKeyEvent(&ui_event, event.code))
      return false;
  }
  return true;
}

}  // namespace input_method
