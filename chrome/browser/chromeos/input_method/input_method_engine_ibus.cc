// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/input_method_engine_ibus.h"

#define XK_MISCELLANY
#include <X11/keysymdef.h>
#include <map>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/input_method/ibus_keymap.h"
#include "chrome/browser/chromeos/input_method/input_method_configuration.h"
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/ibus/ibus_client.h"
#include "chromeos/dbus/ibus/ibus_component.h"
#include "chromeos/dbus/ibus/ibus_engine_factory_service.h"
#include "chromeos/dbus/ibus/ibus_engine_service.h"
#include "chromeos/dbus/ibus/ibus_lookup_table.h"
#include "chromeos/dbus/ibus/ibus_property.h"
#include "chromeos/dbus/ibus/ibus_text.h"
#include "dbus/object_path.h"

namespace chromeos {
const char* kExtensionImePrefix = "_ext_ime_";
const char* kErrorNotActive = "IME is not active";
const char* kErrorWrongContext = "Context is not active";
const char* kCandidateNotFound = "Candidate not found";
const char* kEngineBusPrefix = "org.freedesktop.IBus.";
const char* kObjectPathPrefix = "/org/freedesktop/IBus/Engine/";

namespace {
const uint32 kIBusAltKeyMask = 1 << 3;
const uint32 kIBusCtrlKeyMask = 1 << 2;
const uint32 kIBusShiftKeyMask = 1 << 0;
const uint32 kIBusKeyReleaseMask = 1 << 30;
}

InputMethodEngineIBus::InputMethodEngineIBus()
    : focused_(false),
      active_(false),
      context_id_(0),
      next_context_id_(1),
      current_object_path_(0),
      aux_text_(new IBusText()),
      aux_text_visible_(false),
      observer_(NULL),
      preedit_text_(new IBusText()),
      preedit_cursor_(0),
      component_(new IBusComponent()),
      table_(new IBusLookupTable()),
      window_visible_(false),
      weak_ptr_factory_(this) {
}

InputMethodEngineIBus::~InputMethodEngineIBus() {
  if (object_path_.IsValid())
    GetCurrentService()->UnsetEngine();
  input_method::GetInputMethodManager()->RemoveInputMethodExtension(ibus_id_);
}

void InputMethodEngineIBus::Initialize(
    InputMethodEngine::Observer* observer,
    const char* engine_name,
    const char* extension_id,
    const char* engine_id,
    const char* description,
    const char* language,
    const std::vector<std::string>& layouts,
    std::string* error) {
  DCHECK(observer) << "Observer must not be null.";

  observer_ = observer;
  engine_id_ = engine_id;
  ibus_id_ = kExtensionImePrefix;
  ibus_id_ += extension_id;
  ibus_id_ += engine_id;

  input_method::InputMethodManager* manager =
      input_method::GetInputMethodManager();
  std::string layout;
  if (!layouts.empty()) {
    layout = JoinString(layouts, ',');
  } else {
    const std::string fallback_id =
        manager->GetInputMethodUtil()->GetHardwareInputMethodId();
    const input_method::InputMethodDescriptor* fallback_desc =
        manager->GetInputMethodUtil()->GetInputMethodDescriptorFromId(
            fallback_id);
    layout = fallback_desc->keyboard_layout();
  }

  component_.reset(new IBusComponent());
  component_->set_name(std::string(kEngineBusPrefix) + std::string(engine_id));
  component_->set_description(description);
  component_->set_author(engine_name);

  chromeos::IBusComponent::EngineDescription engine_desc;
  engine_desc.engine_id = ibus_id_;
  engine_desc.display_name = description;
  engine_desc.description = description;
  engine_desc.language_code = language;
  engine_desc.author = ibus_id_;
  engine_desc.layout = layout.c_str();

  component_->mutable_engine_description()->push_back(engine_desc);
  manager->AddInputMethodExtension(ibus_id_, engine_name, layouts, language,
                                   this);
  // If connection is avaiable, register component. If there are no connection
  // to ibus-daemon, OnConnected callback will register component instead.
  if (IsConnected())
    RegisterComponent();
}

bool InputMethodEngineIBus::SetComposition(
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
  GetCurrentService()->UpdatePreedit(
      *preedit_text_.get(),
      preedit_cursor_,
      true,
      chromeos::IBusEngineService::IBUS_ENGINE_PREEEDIT_FOCUS_OUT_MODE_COMMIT);
  return true;
}

bool InputMethodEngineIBus::ClearComposition(int context_id,
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
  GetCurrentService()->UpdatePreedit(
      *preedit_text_.get(),
      0,
      true,
      chromeos::IBusEngineService::IBUS_ENGINE_PREEEDIT_FOCUS_OUT_MODE_COMMIT);
  return true;
}

bool InputMethodEngineIBus::CommitText(int context_id, const char* text,
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

  GetCurrentService()->CommitText(text);
  return true;
}

bool InputMethodEngineIBus::SetCandidateWindowVisible(bool visible,
                                                      std::string* error) {
  if (!active_) {
    *error = kErrorNotActive;
    return false;
  }

  window_visible_ = visible;
  GetCurrentService()->UpdateLookupTable(*table_.get(), window_visible_);
  return true;
}

void InputMethodEngineIBus::SetCandidateWindowCursorVisible(bool visible) {
  table_->set_is_cursor_visible(visible);
  if (active_)
    GetCurrentService()->UpdateLookupTable(*table_.get(), window_visible_);
}

void InputMethodEngineIBus::SetCandidateWindowVertical(bool vertical) {
  table_->set_orientation(vertical ? IBusLookupTable::VERTICAL :
                          IBusLookupTable::HORIZONTAL);
  if (active_)
    GetCurrentService()->UpdateLookupTable(*table_.get(), window_visible_);
}

void InputMethodEngineIBus::SetCandidateWindowPageSize(int size) {
  table_->set_page_size(size);
  if (active_)
    GetCurrentService()->UpdateLookupTable(*table_.get(), window_visible_);
}

void InputMethodEngineIBus::SetCandidateWindowAuxText(const char* text) {
  aux_text_->set_text(text);
  if (active_) {
    // Should not show auxiliary text if the whole window visibility is false.
    GetCurrentService()->UpdateAuxiliaryText(
        *aux_text_.get(),
        window_visible_ && aux_text_visible_);
  }
}

void InputMethodEngineIBus::SetCandidateWindowAuxTextVisible(bool visible) {
  aux_text_visible_ = visible;
  if (active_) {
    // Should not show auxiliary text if the whole window visibility is false.
    GetCurrentService()->UpdateAuxiliaryText(
        *aux_text_.get(),
        window_visible_ && aux_text_visible_);
  }
}

bool InputMethodEngineIBus::SetCandidates(
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
  table_->mutable_candidates()->clear();
  for (std::vector<Candidate>::const_iterator ix = candidates.begin();
       ix != candidates.end(); ++ix) {
    IBusLookupTable::Entry entry;
    entry.value = ix->value;
    entry.label = ix->label;
    entry.annotation = ix->annotation;
    entry.description_title = ix->usage.title;
    entry.description_body = ix->usage.body;

    // Store a mapping from the user defined ID to the candidate index.
    candidate_indexes_[ix->id] = candidate_ids_.size();
    candidate_ids_.push_back(ix->id);

    table_->mutable_candidates()->push_back(entry);
  }
  GetCurrentService()->UpdateLookupTable(*table_.get(), window_visible_);
  return true;
}

bool InputMethodEngineIBus::SetCursorPosition(int context_id, int candidate_id,
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

  table_->set_cursor_position(position->second);
  GetCurrentService()->UpdateLookupTable(*table_.get(), window_visible_);
  return true;
}

bool InputMethodEngineIBus::SetMenuItems(const std::vector<MenuItem>& items) {
  if (!active_)
    return false;

  IBusPropertyList properties;
  for (std::vector<MenuItem>::const_iterator item = items.begin();
       item != items.end(); ++item) {
    IBusProperty* property = new IBusProperty();
    if (!MenuItemToProperty(*item, property)) {
      delete property;
      DVLOG(1) << "Bad menu item";
      return false;
    }
    properties.push_back(property);
  }
  GetCurrentService()->RegisterProperties(properties);
  return true;
}

bool InputMethodEngineIBus::UpdateMenuItems(
    const std::vector<MenuItem>& items) {
  if (!active_)
    return false;

  IBusPropertyList properties;
  for (std::vector<MenuItem>::const_iterator item = items.begin();
       item != items.end(); ++item) {
    IBusProperty* property = new IBusProperty();
    if (!MenuItemToProperty(*item, property)) {
      delete property;
      DVLOG(1) << "Bad menu item";
      return false;
    }
    properties.push_back(property);
  }
  GetCurrentService()->RegisterProperties(properties);
  return true;
}

bool InputMethodEngineIBus::IsActive() const {
  return active_;
}

void InputMethodEngineIBus::KeyEventDone(input_method::KeyEventHandle* key_data,
                                         bool handled) {
  KeyEventDoneCallback* callback =
      reinterpret_cast<KeyEventDoneCallback*>(key_data);
  callback->Run(handled);
  delete callback;
}

bool InputMethodEngineIBus::DeleteSurroundingText(int context_id,
                                                  int offset,
                                                  size_t number_of_chars,
                                                  std::string* error) {
  const uint32 kBackSpaceKeyCode = 14;

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
  for (size_t i = 0; i < number_of_chars; ++i)
    GetCurrentService()->ForwardKeyEvent(XK_BackSpace, kBackSpaceKeyCode, 0U);
  return true;
}

void InputMethodEngineIBus::FocusIn() {
  focused_ = true;
  if (!active_)
    return;
  context_id_ = next_context_id_;
  ++next_context_id_;

  InputContext context;
  context.id = context_id_;
  // TODO: Other types
  context.type = "text";

  observer_->OnFocus(context);
}

void InputMethodEngineIBus::FocusOut() {
  focused_ = false;
  if (!active_)
    return;
  int context_id = context_id_;
  context_id_ = -1;
  observer_->OnBlur(context_id);
}

void InputMethodEngineIBus::Enable() {
  active_ = true;
  observer_->OnActivate(engine_id_);
  FocusIn();
}

void InputMethodEngineIBus::Disable() {
  active_ = false;
  observer_->OnDeactivated(engine_id_);
}

void InputMethodEngineIBus::PropertyActivate(
    const std::string& property_name,
    ibus::IBusPropertyState property_state) {
  observer_->OnMenuItemActivated(engine_id_, property_name);
}

void InputMethodEngineIBus::PropertyShow(
    const std::string& property_name) {
}

void InputMethodEngineIBus::PropertyHide(
    const std::string& property_name) {
}

void InputMethodEngineIBus::SetCapability(
    IBusCapability capability) {
}

void InputMethodEngineIBus::Reset() {
}

void InputMethodEngineIBus::ProcessKeyEvent(
    uint32 keysym,
    uint32 keycode,
    uint32 state,
    const KeyEventDoneCallback& callback) {

  KeyEventDoneCallback *handler = new KeyEventDoneCallback();
  *handler = callback;

  KeyboardEvent event;
  event.type = !(state & kIBusKeyReleaseMask) ? "keydown" : "keyup";
  event.key = input_method::GetIBusKey(keysym);
  event.code = input_method::GetIBusKeyCode(keycode);
  event.alt_key = state & kIBusAltKeyMask;
  event.ctrl_key = state & kIBusCtrlKeyMask;
  event.shift_key = state & kIBusShiftKeyMask;
  observer_->OnKeyEvent(
      engine_id_,
      event,
      reinterpret_cast<input_method::KeyEventHandle*>(handler));
}

void InputMethodEngineIBus::CandidateClicked(uint32 index,
                                             ibus::IBusMouseButton button,
                                             uint32 state) {
  if (index > candidate_ids_.size()) {
    return;
  }

  MouseButtonEvent pressed_button;
  switch (button) {
    case ibus::IBUS_MOUSE_BUTTON_LEFT:
      pressed_button = MOUSE_BUTTON_LEFT;
      break;
    case ibus::IBUS_MOUSE_BUTTON_MIDDLE:
      pressed_button = MOUSE_BUTTON_MIDDLE;
      break;
    case ibus::IBUS_MOUSE_BUTTON_RIGHT:
      pressed_button = MOUSE_BUTTON_RIGHT;
      break;
    default:
      DVLOG(1) << "Unknown button: " << button;
      pressed_button = MOUSE_BUTTON_LEFT;
      break;
  }

  observer_->OnCandidateClicked(
      engine_id_, candidate_ids_.at(index), pressed_button);
}

void InputMethodEngineIBus::SetSurroundingText(
    const std::string& text,
    uint32 cursor_pos,
    uint32 anchor_pos) {
}

IBusEngineService* InputMethodEngineIBus::GetCurrentService() {
  return DBusThreadManager::Get()->GetIBusEngineService(object_path_);
}

bool InputMethodEngineIBus::MenuItemToProperty(
    const MenuItem& item,
    IBusProperty* property) {
  property->set_key(item.id);

  if (item.modified & MENU_ITEM_MODIFIED_LABEL) {
    property->set_label(item.label);
  }
  if (item.modified & MENU_ITEM_MODIFIED_VISIBLE) {
    property->set_visible(item.visible);
  }
  if (item.modified & MENU_ITEM_MODIFIED_CHECKED) {
    property->set_checked(item.checked);
  }
  if (item.modified & MENU_ITEM_MODIFIED_ENABLED) {
    // TODO(nona): implement sensitive entry(crbug.com/140192).
  }
  if (item.modified & MENU_ITEM_MODIFIED_STYLE) {
    IBusProperty::IBusPropertyType type =
        IBusProperty::IBUS_PROPERTY_TYPE_NORMAL;
    if (!item.children.empty()) {
      type = IBusProperty::IBUS_PROPERTY_TYPE_MENU;
    } else {
      switch (item.style) {
        case MENU_ITEM_STYLE_NONE:
          type = IBusProperty::IBUS_PROPERTY_TYPE_NORMAL;
          break;
        case MENU_ITEM_STYLE_CHECK:
          type = IBusProperty::IBUS_PROPERTY_TYPE_TOGGLE;
          break;
        case MENU_ITEM_STYLE_RADIO:
          type = IBusProperty::IBUS_PROPERTY_TYPE_RADIO;
          break;
        case MENU_ITEM_STYLE_SEPARATOR:
          type = IBusProperty::IBUS_PROPERTY_TYPE_SEPARATOR;
          break;
      }
    }
    property->set_type(type);
  }

  for (std::vector<MenuItem>::const_iterator child = item.children.begin();
       child != item.children.end(); ++child) {
    IBusProperty* new_property = new IBusProperty();
    if (!MenuItemToProperty(*child, new_property)) {
      delete new_property;
      DVLOG(1) << "Bad menu item child";
      return false;
    }
    property->mutable_sub_properties()->push_back(new_property);
  }

  return true;
}

void InputMethodEngineIBus::OnConnected() {
  RegisterComponent();
}

void InputMethodEngineIBus::OnDisconnected() {
}

bool InputMethodEngineIBus::IsConnected() {
  return DBusThreadManager::Get()->GetIBusClient() != NULL;
}

void InputMethodEngineIBus::RegisterComponent() {
  chromeos::IBusClient* client =
      chromeos::DBusThreadManager::Get()->GetIBusClient();
  client->RegisterComponent(
      *component_.get(),
      base::Bind(&InputMethodEngineIBus::OnComponentRegistered,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&InputMethodEngineIBus::OnComponentRegistrationFailed,
                 weak_ptr_factory_.GetWeakPtr()));
}

void InputMethodEngineIBus::OnComponentRegistered() {
  DBusThreadManager::Get()->GetIBusEngineFactoryService()->
      SetCreateEngineHandler(ibus_id_,
                             base::Bind(
                                 &InputMethodEngineIBus::CreateEngineHandler,
                                 weak_ptr_factory_.GetWeakPtr()));
}

void InputMethodEngineIBus::OnComponentRegistrationFailed() {
  DVLOG(1) << "Failed to register input method components.";
  // TODO(nona): Implement error handling.
}

void InputMethodEngineIBus::CreateEngineHandler(
    const IBusEngineFactoryService::CreateEngineResponseSender& sender) {
  DBusThreadManager::Get()->RemoveIBusEngineService(object_path_);

  current_object_path_++;
  object_path_ = dbus::ObjectPath(kObjectPathPrefix +
                                  base::IntToString(current_object_path_));
  GetCurrentService()->SetEngine(this);
  sender.Run(object_path_);
}

}  // namespace chromeos
