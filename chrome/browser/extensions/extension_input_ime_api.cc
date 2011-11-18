// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_input_ime_api.h"

#include "base/json/json_writer.h"
#include "base/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/input_method/input_method_engine.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_input_module_constants.h"
#include "chrome/browser/profiles/profile.h"

namespace keys = extension_input_module_constants;

namespace {

const char kStyleNone[] = "none";
const char kStyleCheck[] = "check";
const char kStyleRadio[] = "radio";
const char kStyleSeparator[] = "separator";

const char kErrorEngineNotAvailable[] = "Engine is not available";
const char kErrorBadCandidateList[] = "Invalid candidate list provided";
const char kErrorSetMenuItemsFail[] = "Could not create menu Items";
const char kErrorUpdateMenuItemsFail[] = "Could not update menu Items";

bool ReadMenuItems(
    ListValue* menu_items,
    std::vector<chromeos::InputMethodEngine::MenuItem>* output) {
  for (size_t i = 0; i < menu_items->GetSize(); ++i) {
    DictionaryValue* item_dict;
    if (!menu_items->GetDictionary(i, &item_dict)) {
      return false;
    }

    std::string id;
    std::string label;
    chromeos::InputMethodEngine::MenuItemStyle style =
        chromeos::InputMethodEngine::MENU_ITEM_STYLE_NONE;
    bool visible = true;
    bool enabled = true;
    bool checked = false;
    std::string icon;
    chromeos::InputMethodEngine::KeyboardEvent shortcut_key;

    unsigned int modified = 0;

    if (!item_dict->GetString(keys::kIdKey, &id)) {
      return false;
    }

    if (item_dict->HasKey(keys::kLabelKey)) {
      if (!item_dict->GetString(keys::kLabelKey, &label)) {
        return false;
      }
      modified |= chromeos::InputMethodEngine::MENU_ITEM_MODIFIED_LABEL;
    }
    if (item_dict->HasKey(keys::kStyleKey)) {
      std::string style_string;
      if (!item_dict->GetString(keys::kStyleKey, &style_string)) {
        return false;
      }

      if (style_string == kStyleNone) {
        style = chromeos::InputMethodEngine::MENU_ITEM_STYLE_NONE;
      } else if (style_string == kStyleCheck) {
        style = chromeos::InputMethodEngine::MENU_ITEM_STYLE_CHECK;
      } else if (style_string == kStyleRadio) {
        style = chromeos::InputMethodEngine::MENU_ITEM_STYLE_RADIO;
      } else if (style_string == kStyleSeparator) {
        style = chromeos::InputMethodEngine::MENU_ITEM_STYLE_SEPARATOR;
      } else {
        return false;
      }
      modified |= chromeos::InputMethodEngine::MENU_ITEM_MODIFIED_STYLE;
    }

    if (item_dict->HasKey(keys::kVisibleKey)) {
      if (!item_dict->GetBoolean(keys::kVisibleKey, &visible)) {
        return false;
      }
      modified |= chromeos::InputMethodEngine::MENU_ITEM_MODIFIED_VISIBLE;
    }

    if (item_dict->HasKey(keys::kCheckedKey)) {
      if (!item_dict->GetBoolean(keys::kCheckedKey, &checked)) {
        return false;
      }
      modified |= chromeos::InputMethodEngine::MENU_ITEM_MODIFIED_CHECKED;
    }

    if (item_dict->HasKey(keys::kEnabledKey)) {
      if (!item_dict->GetBoolean(keys::kEnabledKey, &enabled)) {
        return false;
      }
      modified |= chromeos::InputMethodEngine::MENU_ITEM_MODIFIED_ENABLED;
    }

    if (item_dict->HasKey(keys::kIconKey)) {
      if (!item_dict->GetString(keys::kIconKey, &icon)) {
        return false;
      }
      modified |= chromeos::InputMethodEngine::MENU_ITEM_MODIFIED_ICON;
    }

    if (item_dict->HasKey(keys::kShortcutKey)) {
      DictionaryValue* shortcut_dict;
      if (!item_dict->GetDictionary(keys::kShortcutKey, &shortcut_dict)) {
        return false;
      }

      if (!shortcut_dict->GetString(keys::kKeyKey, &(shortcut_key.key))) {
        return false;
      }

      if (shortcut_dict->HasKey(keys::kAltKeyKey)) {
        if (!item_dict->GetBoolean(keys::kAltKeyKey, &(shortcut_key.alt_key))) {
          return false;
        }
      }

      if (shortcut_dict->HasKey(keys::kCtrlKeyKey)) {
        if (!shortcut_dict->GetBoolean(keys::kCtrlKeyKey,
                                       &(shortcut_key.ctrl_key))) {
          return false;
        }
      }

      if (shortcut_dict->HasKey(keys::kShiftKeyKey)) {
        if (!shortcut_dict->GetBoolean(keys::kShiftKeyKey,
                                       &(shortcut_key.shift_key))) {
          return false;
        }
      }
      modified |= chromeos::InputMethodEngine::MENU_ITEM_MODIFIED_SHORTCUT_KEY;
    }

    output->push_back(chromeos::InputMethodEngine::MenuItem());
    output->back().id = id;
    output->back().label = label;
    output->back().style = style;
    output->back().visible = visible;
    output->back().enabled = enabled;
    output->back().checked = checked;
    output->back().icon = icon;
    output->back().shortcut_key = shortcut_key;

    output->back().modified = modified;

    if (item_dict->HasKey(keys::kItemsKey)) {
      ListValue* sub_list;
      if (!item_dict->GetList(keys::kItemsKey, &sub_list)) {
        return false;
      }

      if (!ReadMenuItems(sub_list, &(output->back().children))) {
        return false;
      }
    }
  }

  return true;
}

}

namespace events {

const char kOnActivate[] = "experimental.input.ime.onActivate";
const char kOnDeactivated[] = "experimental.input.ime.onDeactivated";
const char kOnFocus[] = "experimental.input.ime.onFocus";
const char kOnBlur[] = "experimental.input.ime.onBlur";
const char kOnInputContextUpdate[] =
    "experimental.input.ime.onInputContextUpdate";
const char kOnKeyEvent[] = "experimental.input.ime.onKeyEvent";
const char kOnCandidateClicked[] = "experimental.input.ime.onCandidateClicked";
const char kOnMenuItemActivated[] =
    "experimental.input.ime.onMenuItemActivated";

}  // namespace events

namespace chromeos {
class ImeObserver : public chromeos::InputMethodEngine::Observer {
 public:
  ImeObserver(Profile* profile, const std::string& extension_id,
              const std::string& engine_id) :
    profile_(profile),
    extension_id_(extension_id),
    engine_id_(engine_id) {
  }

  virtual ~ImeObserver() {
  }

  virtual void OnActivate(const std::string& engine_id) {
    if (profile_ == NULL || extension_id_.empty())
      return;

    ListValue args;
    args.Append(Value::CreateStringValue(engine_id));

    std::string json_args;
    base::JSONWriter::Write(&args, false, &json_args);
    profile_->GetExtensionEventRouter()->DispatchEventToExtension(
        extension_id_, events::kOnActivate, json_args, profile_, GURL());
  }

  virtual void OnDeactivated(const std::string& engine_id) {
    if (profile_ == NULL || extension_id_.empty())
      return;

    ListValue args;
    args.Append(Value::CreateStringValue(engine_id));

    std::string json_args;
    base::JSONWriter::Write(&args, false, &json_args);
    profile_->GetExtensionEventRouter()->DispatchEventToExtension(
        extension_id_, events::kOnDeactivated, json_args, profile_, GURL());
  }

  virtual void OnFocus(const InputMethodEngine::InputContext& context) {
    if (profile_ == NULL || extension_id_.empty())
      return;

    DictionaryValue* dict = new DictionaryValue();
    dict->SetInteger("contextID", context.id);
    dict->SetString("type", context.type);

    ListValue args;
    args.Append(dict);

    std::string json_args;
    base::JSONWriter::Write(&args, false, &json_args);
    profile_->GetExtensionEventRouter()->DispatchEventToExtension(
        extension_id_, events::kOnFocus, json_args, profile_, GURL());
  }

  virtual void OnBlur(int context_id) {
    if (profile_ == NULL || extension_id_.empty())
      return;

    ListValue args;
    args.Append(Value::CreateIntegerValue(context_id));

    std::string json_args;
    base::JSONWriter::Write(&args, false, &json_args);
    profile_->GetExtensionEventRouter()->DispatchEventToExtension(
        extension_id_, events::kOnBlur, json_args, profile_, GURL());
  }

  virtual void OnInputContextUpdate(
      const InputMethodEngine::InputContext& context) {
    if (profile_ == NULL || extension_id_.empty())
      return;

    DictionaryValue* dict = new DictionaryValue();
    dict->SetInteger("contextID", context.id);
    dict->SetString("type", context.type);

    ListValue args;
    args.Append(dict);

    std::string json_args;
    base::JSONWriter::Write(&args, false, &json_args);
    profile_->GetExtensionEventRouter()->DispatchEventToExtension(
        extension_id_, events::kOnInputContextUpdate, json_args, profile_,
        GURL());
  }

  virtual void OnKeyEvent(const std::string& engine_id,
                          const InputMethodEngine::KeyboardEvent& event,
                          chromeos::input_method::KeyEventHandle* key_data) {
    if (profile_ == NULL || extension_id_.empty())
      return;

    std::string request_id =
        ExtensionInputImeEventRouter::GetInstance()->AddRequest(engine_id,
                                                                key_data);

    DictionaryValue* dict = new DictionaryValue();
    dict->SetString("type", event.type);
    dict->SetString("requestId", request_id);
    dict->SetString("key", event.key);
    dict->SetString("keyCode", event.key_code);
    dict->SetBoolean("altKey", event.alt_key);
    dict->SetBoolean("ctrlKey", event.ctrl_key);
    dict->SetBoolean("shiftKey", event.shift_key);

    ListValue args;
    args.Append(Value::CreateStringValue(engine_id));
    args.Append(dict);

    std::string json_args;
    base::JSONWriter::Write(&args, false, &json_args);
    profile_->GetExtensionEventRouter()->DispatchEventToExtension(
        extension_id_, events::kOnKeyEvent, json_args, profile_, GURL());
  }

  virtual void OnCandidateClicked(
      const std::string& engine_id,
      int candidate_id,
      chromeos::InputMethodEngine::MouseButtonEvent button) {
    if (profile_ == NULL || extension_id_.empty())
      return;

    ListValue args;
    args.Append(Value::CreateStringValue(engine_id));
    args.Append(Value::CreateIntegerValue(candidate_id));
    switch (button) {
      case chromeos::InputMethodEngine::MOUSE_BUTTON_MIDDLE:
        args.Append(Value::CreateStringValue("middle"));
        break;

      case chromeos::InputMethodEngine::MOUSE_BUTTON_RIGHT:
        args.Append(Value::CreateStringValue("right"));
        break;

      case chromeos::InputMethodEngine::MOUSE_BUTTON_LEFT:
      // Default to left.
      default:
        args.Append(Value::CreateStringValue("left"));
        break;
    }

    std::string json_args;
    base::JSONWriter::Write(&args, false, &json_args);
    profile_->GetExtensionEventRouter()->DispatchEventToExtension(
        extension_id_, events::kOnCandidateClicked, json_args, profile_,
        GURL());
  }

  virtual void OnMenuItemActivated(const std::string& engine_id,
                                   const std::string& menu_id) {
    if (profile_ == NULL || extension_id_.empty())
      return;

    ListValue args;
    args.Append(Value::CreateStringValue(engine_id));
    args.Append(Value::CreateStringValue(menu_id));

    std::string json_args;
    base::JSONWriter::Write(&args, false, &json_args);
    profile_->GetExtensionEventRouter()->DispatchEventToExtension(
        extension_id_, events::kOnMenuItemActivated, json_args, profile_,
        GURL());
  }

 private:
  Profile* profile_;
  std::string extension_id_;
  std::string engine_id_;

  DISALLOW_COPY_AND_ASSIGN(ImeObserver);
};
}  // namespace chromeos


ExtensionInputImeEventRouter*
ExtensionInputImeEventRouter::GetInstance() {
  return Singleton<ExtensionInputImeEventRouter>::get();
}

ExtensionInputImeEventRouter::ExtensionInputImeEventRouter()
  : next_request_id_(1) {
}

ExtensionInputImeEventRouter::~ExtensionInputImeEventRouter() {
}

void ExtensionInputImeEventRouter::Init() {
}

#if defined(OS_CHROMEOS)
bool ExtensionInputImeEventRouter::RegisterIme(
    Profile* profile,
    const std::string& extension_id,
    const Extension::InputComponentInfo& component) {
  VLOG(1) << "RegisterIme: " << extension_id << " id: " << component.id;

  std::map<std::string, chromeos::InputMethodEngine*>& engine_map =
      engines_[extension_id];

  std::map<std::string, chromeos::InputMethodEngine*>::iterator engine_ix =
      engine_map.find(component.id);
  if (engine_ix != engine_map.end()) {
    return false;
  }

  std::string error;
  chromeos::ImeObserver* observer = new chromeos::ImeObserver(profile,
                                                              extension_id,
                                                              component.id);
  chromeos::InputMethodEngine::KeyboardEvent shortcut_key;
  shortcut_key.key = component.shortcut_keycode;
  shortcut_key.key_code = component.shortcut_keycode;
  shortcut_key.alt_key = component.shortcut_alt;
  shortcut_key.ctrl_key = component.shortcut_ctrl;
  shortcut_key.shift_key = component.shortcut_shift;

  std::vector<std::string> layouts;
  layouts.assign(component.layouts.begin(), component.layouts.end());

  chromeos::InputMethodEngine* engine =
      chromeos::InputMethodEngine::CreateEngine(
          observer, component.name.c_str(), extension_id.c_str(),
          component.id.c_str(), component.description.c_str(),
          component.language.c_str(), layouts,
          shortcut_key, &error);
  if (!engine) {
    delete observer;
    LOG(ERROR) << "RegisterIme: " << error;
    return false;
  }

  engine_map[component.id] = engine;

  std::map<std::string, chromeos::ImeObserver*>& observer_list =
      observers_[extension_id];

  observer_list[component.id] = observer;

  return true;
}
#endif

chromeos::InputMethodEngine* ExtensionInputImeEventRouter::GetEngine(
    const std::string& extension_id, const std::string& engine_id) {
  std::map<std::string,
           std::map<std::string, chromeos::InputMethodEngine*> >::const_iterator
               engine_list = engines_.find(extension_id);
  if (engine_list != engines_.end()) {
    std::map<std::string, chromeos::InputMethodEngine*>::const_iterator
        engine_ix = engine_list->second.find(engine_id);
    if (engine_ix != engine_list->second.end()) {
      return engine_ix->second;
    }
  }
  return NULL;
}

chromeos::InputMethodEngine* ExtensionInputImeEventRouter::GetActiveEngine(
    const std::string& extension_id) {
  std::map<std::string,
           std::map<std::string, chromeos::InputMethodEngine*> >::const_iterator
               engine_list = engines_.find(extension_id);
  if (engine_list != engines_.end()) {
    std::map<std::string, chromeos::InputMethodEngine*>::const_iterator
        engine_ix;
    for (engine_ix = engine_list->second.begin();
         engine_ix != engine_list->second.end();
         ++engine_ix) {
      if (engine_ix->second->IsActive()) {
        return engine_ix->second;
      }
    }
  }
  return NULL;
}

void ExtensionInputImeEventRouter::OnEventHandled(
    const std::string& extension_id,
    const std::string& request_id,
    bool handled) {
  RequestMap::iterator request = request_map_.find(request_id);
  if (request == request_map_.end()) {
    LOG(ERROR) << "Request ID not found: " << request_id;
    return;
  }

  std::string engine_id = request->second.first;
  chromeos::input_method::KeyEventHandle* key_data = request->second.second;
  request_map_.erase(request);

  chromeos::InputMethodEngine* engine = GetEngine(extension_id, engine_id);
  if (!engine) {
    LOG(ERROR) << "Engine does not exist: " << engine_id;
    return;
  }

  engine->KeyEventDone(key_data, handled);
}

std::string ExtensionInputImeEventRouter::AddRequest(
    const std::string& engine_id,
    chromeos::input_method::KeyEventHandle* key_data) {
  std::string request_id = base::IntToString(next_request_id_);
  ++next_request_id_;

  request_map_[request_id] = std::make_pair(engine_id, key_data);

  return request_id;
}

bool SetCompositionFunction::RunImpl() {
  chromeos::InputMethodEngine* engine =
      ExtensionInputImeEventRouter::GetInstance()->
          GetActiveEngine(extension_id());
  if (!engine) {
    result_.reset(Value::CreateBooleanValue(false));
    return true;
  }

  DictionaryValue* args;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &args));
  int context_id;
  std::string text;
  int selection_start;
  int selection_end;
  std::vector<chromeos::InputMethodEngine::SegmentInfo> segments;

  EXTENSION_FUNCTION_VALIDATE(args->GetInteger(keys::kContextIdKey,
                                               &context_id));
  EXTENSION_FUNCTION_VALIDATE(args->GetString(keys::kTextKey, &text));
  if (args->HasKey(keys::kSelectionStartKey)) {
    EXTENSION_FUNCTION_VALIDATE(args->GetInteger(keys::kSelectionStartKey,
                                                 &selection_start));
  } else {
    selection_start = 0;
  }
  if (args->HasKey(keys::kSelectionEndKey)) {
    EXTENSION_FUNCTION_VALIDATE(args->GetInteger(keys::kSelectionEndKey,
                                                 &selection_end));
  } else {
    selection_end = 0;
  }

  if (args->HasKey(keys::kSegmentsKey)) {
    // TODO: Handle segments
    ListValue* segment_list;
    EXTENSION_FUNCTION_VALIDATE(args->GetList(keys::kSegmentsKey,
                                              &segment_list));
    int start;
    int end;
    std::string style;

    EXTENSION_FUNCTION_VALIDATE(args->GetInteger(keys::kStartKey,
                                                 &start));
    EXTENSION_FUNCTION_VALIDATE(args->GetInteger(keys::kEndKey, &end));
    EXTENSION_FUNCTION_VALIDATE(args->GetString(keys::kStyleKey, &style));

    segments.push_back(chromeos::InputMethodEngine::SegmentInfo());
    segments.back().start = start;
    segments.back().end = end;
    if (style == keys::kStyleUnderline) {
      segments.back().style =
          chromeos::InputMethodEngine::SEGMENT_STYLE_UNDERLINE;
    } else if (style == keys::kStyleDoubleUnderline) {
      segments.back().style =
          chromeos::InputMethodEngine::SEGMENT_STYLE_DOUBLE_UNDERLINE;
    }
  }

  if (engine->SetComposition(context_id, text.c_str(), selection_start,
                             selection_end, segments, &error_)) {
    result_.reset(Value::CreateBooleanValue(true));
  } else {
    result_.reset(Value::CreateBooleanValue(false));
  }
  return true;
}

bool ClearCompositionFunction::RunImpl() {
  chromeos::InputMethodEngine* engine =
      ExtensionInputImeEventRouter::GetInstance()->
          GetActiveEngine(extension_id());
  if (!engine) {
    result_.reset(Value::CreateBooleanValue(false));
    return true;
  }

  DictionaryValue* args;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &args));
  int context_id;

  EXTENSION_FUNCTION_VALIDATE(args->GetInteger(keys::kContextIdKey,
                                               &context_id));

  if (engine->ClearComposition(context_id, &error_)) {
    result_.reset(Value::CreateBooleanValue(true));
  } else {
    result_.reset(Value::CreateBooleanValue(false));
  }
  return true;
}

bool CommitTextFunction::RunImpl() {
  // TODO(zork): Support committing when not active.
  chromeos::InputMethodEngine* engine =
      ExtensionInputImeEventRouter::GetInstance()->
          GetActiveEngine(extension_id());
  if (!engine) {
    result_.reset(Value::CreateBooleanValue(false));
    return true;
  }

  DictionaryValue* args;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &args));
  int context_id;
  std::string text;

  EXTENSION_FUNCTION_VALIDATE(args->GetInteger(keys::kContextIdKey,
                                                     &context_id));
  EXTENSION_FUNCTION_VALIDATE(args->GetString(keys::kTextKey, &text));

  if (engine->CommitText(context_id, text.c_str(), &error_)) {
    result_.reset(Value::CreateBooleanValue(true));
  } else {
    result_.reset(Value::CreateBooleanValue(false));
  }
  return true;
}

bool SetCandidateWindowPropertiesFunction::RunImpl() {
  DictionaryValue* args;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &args));

  std::string engine_id;
  EXTENSION_FUNCTION_VALIDATE(args->GetString(keys::kEngineIdKey, &engine_id));

  chromeos::InputMethodEngine* engine =
      ExtensionInputImeEventRouter::GetInstance()->GetEngine(extension_id(),
                                                             engine_id);
  if (!engine) {
    result_.reset(Value::CreateBooleanValue(false));
    return true;
  }

  DictionaryValue* properties;
  EXTENSION_FUNCTION_VALIDATE(args->GetDictionary(keys::kPropertiesKey,
                                                  &properties));

  if (properties->HasKey(keys::kVisibleKey)) {
    bool visible;
    EXTENSION_FUNCTION_VALIDATE(properties->GetBoolean(keys::kVisibleKey,
                                                       &visible));
    if (!engine->SetCandidateWindowVisible(visible, &error_)) {
      result_.reset(Value::CreateBooleanValue(false));
      return true;
    }
  }

  if (properties->HasKey(keys::kCursorVisibleKey)) {
    bool visible;
    EXTENSION_FUNCTION_VALIDATE(properties->GetBoolean(keys::kCursorVisibleKey,
                                                       &visible));
    engine->SetCandidateWindowCursorVisible(visible);
  }

  if (properties->HasKey(keys::kVerticalKey)) {
    bool vertical;
    EXTENSION_FUNCTION_VALIDATE(properties->GetBoolean(keys::kVerticalKey,
                                                       &vertical));
    engine->SetCandidateWindowVertical(vertical);
  }

  if (properties->HasKey(keys::kPageSizeKey)) {
    int page_size;
    EXTENSION_FUNCTION_VALIDATE(properties->GetInteger(keys::kPageSizeKey,
                                                       &page_size));
    engine->SetCandidateWindowPageSize(page_size);
  }

  if (properties->HasKey(keys::kAuxiliaryTextKey)) {
    std::string aux_text;
    EXTENSION_FUNCTION_VALIDATE(properties->GetString(keys::kAuxiliaryTextKey,
                                                      &aux_text));
    engine->SetCandidateWindowAuxText(aux_text.c_str());
  }

  if (properties->HasKey(keys::kAuxiliaryTextVisibleKey)) {
    bool visible;
    EXTENSION_FUNCTION_VALIDATE(properties->GetBoolean(
        keys::kAuxiliaryTextVisibleKey,
        &visible));
    engine->SetCandidateWindowAuxTextVisible(visible);
  }

  result_.reset(Value::CreateBooleanValue(true));

  return true;
}

#if defined(OS_CHROMEOS)
bool SetCandidatesFunction::ReadCandidates(
    ListValue* candidates,
    std::vector<chromeos::InputMethodEngine::Candidate>* output) {
  for (size_t i = 0; i < candidates->GetSize(); ++i) {
    DictionaryValue* candidate_dict;
    EXTENSION_FUNCTION_VALIDATE(candidates->GetDictionary(i, &candidate_dict));

    std::string candidate;
    int id;
    std::string label;
    std::string annotation;

    EXTENSION_FUNCTION_VALIDATE(candidate_dict->GetString(keys::kCandidateKey,
                                                          &candidate));
    EXTENSION_FUNCTION_VALIDATE(candidate_dict->GetInteger(keys::kIdKey, &id));

    if (candidate_dict->HasKey(keys::kLabelKey)) {
      EXTENSION_FUNCTION_VALIDATE(candidate_dict->GetString(keys::kLabelKey,
                                                            &label));
    }
    if (candidate_dict->HasKey(keys::kAnnotationKey)) {
      EXTENSION_FUNCTION_VALIDATE(candidate_dict->GetString(
          keys::kAnnotationKey,
          &annotation));
    }

    output->push_back(chromeos::InputMethodEngine::Candidate());
    output->back().value = candidate;
    output->back().id = id;
    output->back().label = label;
    output->back().annotation = annotation;

    if (candidate_dict->HasKey(keys::kCandidatesKey)) {
      ListValue* sub_list;
      EXTENSION_FUNCTION_VALIDATE(candidate_dict->GetList(keys::kCandidatesKey,
                                                          &sub_list));
      if (!ReadCandidates(sub_list, &(output->back().candidates))) {
        error_ = kErrorBadCandidateList;
        return false;
      }
    }
  }

  return true;
}

bool SetCandidatesFunction::RunImpl() {
  chromeos::InputMethodEngine* engine =
      ExtensionInputImeEventRouter::GetInstance()->
          GetActiveEngine(extension_id());
  if (!engine) {
    result_.reset(Value::CreateBooleanValue(false));
    return true;
  }

  DictionaryValue* args;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &args));

  int context_id;
  std::vector<chromeos::InputMethodEngine::Candidate> candidates;

  EXTENSION_FUNCTION_VALIDATE(args->GetInteger(keys::kContextIdKey,
                                                     &context_id));

  ListValue* candidate_list;
  EXTENSION_FUNCTION_VALIDATE(args->GetList(keys::kCandidatesKey,
                                            &candidate_list));
  if (!ReadCandidates(candidate_list, &candidates)) {
    error_ = kErrorBadCandidateList;
    return false;
  }

  std::string error;
  if (engine->SetCandidates(context_id, candidates, &error_)) {
    result_.reset(Value::CreateBooleanValue(true));
  } else {
    result_.reset(Value::CreateBooleanValue(false));
  }
  return true;
}

bool SetCursorPositionFunction::RunImpl() {
  chromeos::InputMethodEngine* engine =
      ExtensionInputImeEventRouter::GetInstance()->
          GetActiveEngine(extension_id());
  if (!engine) {
    result_.reset(Value::CreateBooleanValue(false));
    return true;
  }

  DictionaryValue* args;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &args));
  int context_id;
  int candidate_id;

  EXTENSION_FUNCTION_VALIDATE(args->GetInteger(keys::kContextIdKey,
                                                     &context_id));
  EXTENSION_FUNCTION_VALIDATE(args->GetInteger(keys::kCandidateIdKey,
                                                     &candidate_id));

  if (engine->SetCursorPosition(context_id, candidate_id, &error_)) {
    result_.reset(Value::CreateBooleanValue(true));
  } else {
    result_.reset(Value::CreateBooleanValue(false));
  }
  return true;
}

bool SetMenuItemsFunction::RunImpl() {
  DictionaryValue* args;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &args));

  std::string engine_id;
  EXTENSION_FUNCTION_VALIDATE(args->GetString(keys::kEngineIdKey, &engine_id));

  chromeos::InputMethodEngine* engine =
      ExtensionInputImeEventRouter::GetInstance()->GetEngine(extension_id(),
                                                             engine_id);
  if (!engine) {
    error_ = kErrorEngineNotAvailable;
    return false;
  }

  ListValue* items;
  EXTENSION_FUNCTION_VALIDATE(args->GetList(keys::kItemsKey, &items));

  std::vector<chromeos::InputMethodEngine::MenuItem> menu_items;
  EXTENSION_FUNCTION_VALIDATE(ReadMenuItems(items, &menu_items));

  if (!engine->SetMenuItems(menu_items)) {
    error_ = kErrorSetMenuItemsFail;
  }
  return true;
}

bool UpdateMenuItemsFunction::RunImpl() {
  DictionaryValue* args;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &args));

  std::string engine_id;
  EXTENSION_FUNCTION_VALIDATE(args->GetString(keys::kEngineIdKey, &engine_id));

  chromeos::InputMethodEngine* engine =
      ExtensionInputImeEventRouter::GetInstance()->GetEngine(extension_id(),
                                                             engine_id);
  if (!engine) {
    error_ = kErrorEngineNotAvailable;
    return false;
  }

  ListValue* items;
  EXTENSION_FUNCTION_VALIDATE(args->GetList(keys::kItemsKey, &items));

  std::vector<chromeos::InputMethodEngine::MenuItem> menu_items;
  EXTENSION_FUNCTION_VALIDATE(ReadMenuItems(items, &menu_items));

  if (!engine->UpdateMenuItems(menu_items)) {
    error_ = kErrorUpdateMenuItemsFail;
  }
  return true;
}

bool InputEventHandled::RunImpl() {
  std::string request_id_str;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &request_id_str));

  bool handled = false;
  EXTENSION_FUNCTION_VALIDATE(args_->GetBoolean(1, &handled));

  ExtensionInputImeEventRouter::GetInstance()->OnEventHandled(
      extension_id(), request_id_str, handled);

  return true;
}
#endif
