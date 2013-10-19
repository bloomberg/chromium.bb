// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/input_ime/input_ime_api.h"

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/input_method/input_method_engine.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_function_registry.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/input_ime.h"
#include "chrome/common/extensions/api/input_ime/input_components_handler.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"

namespace input_ime = extensions::api::input_ime;
namespace KeyEventHandled = extensions::api::input_ime::KeyEventHandled;
namespace DeleteSurroundingText =
    extensions::api::input_ime::DeleteSurroundingText;
namespace UpdateMenuItems = extensions::api::input_ime::UpdateMenuItems;
namespace SetMenuItems = extensions::api::input_ime::SetMenuItems;
namespace SetCursorPosition = extensions::api::input_ime::SetCursorPosition;
namespace SetCandidates = extensions::api::input_ime::SetCandidates;
namespace SetCandidateWindowProperties =
    extensions::api::input_ime::SetCandidateWindowProperties;
namespace CommitText = extensions::api::input_ime::CommitText;
namespace ClearComposition = extensions::api::input_ime::ClearComposition;
namespace SetComposition = extensions::api::input_ime::SetComposition;

namespace {

const char kErrorEngineNotAvailable[] = "Engine is not available";
const char kErrorSetMenuItemsFail[] = "Could not create menu Items";
const char kErrorUpdateMenuItemsFail[] = "Could not update menu Items";

void SetMenuItemToMenu(const input_ime::MenuItem& input,
                       chromeos::InputMethodEngine::MenuItem* out) {
  out->modified = 0;
  out->id = input.id;
  if (input.label) {
    out->modified |= chromeos::InputMethodEngine::MENU_ITEM_MODIFIED_LABEL;
    out->label = *input.label;
  }

  if (input.style != input_ime::MenuItem::STYLE_NONE) {
    out->modified |= chromeos::InputMethodEngine::MENU_ITEM_MODIFIED_STYLE;
    out->style = static_cast<chromeos::InputMethodEngine::MenuItemStyle>(
        input.style);
  }

  if (input.visible)
    out->modified |= chromeos::InputMethodEngine::MENU_ITEM_MODIFIED_VISIBLE;
  out->visible = input.visible ? *input.visible : true;

  if (input.checked)
    out->modified |= chromeos::InputMethodEngine::MENU_ITEM_MODIFIED_CHECKED;
  out->checked = input.checked ? *input.checked : false;

  if (input.enabled)
    out->modified |= chromeos::InputMethodEngine::MENU_ITEM_MODIFIED_ENABLED;
  out->enabled = input.enabled ? *input.enabled : true;
}

static void DispatchEventToExtension(Profile* profile,
                                     const std::string& extension_id,
                                     const std::string& event_name,
                                     scoped_ptr<base::ListValue> args) {
  scoped_ptr<extensions::Event> event(new extensions::Event(
      event_name, args.Pass()));
  event->restrict_to_profile = profile;
  extensions::ExtensionSystem::Get(profile)->event_router()->
      DispatchEventToExtension(extension_id, event.Pass());
}

}  // namespace

namespace chromeos {
class ImeObserver : public chromeos::InputMethodEngine::Observer {
 public:
  ImeObserver(Profile* profile, const std::string& extension_id,
              const std::string& engine_id) :
    profile_(profile),
    extension_id_(extension_id),
    engine_id_(engine_id) {
  }

  virtual ~ImeObserver() {}

  virtual void OnActivate(const std::string& engine_id) OVERRIDE {
    if (profile_ == NULL || extension_id_.empty())
      return;

    scoped_ptr<base::ListValue> args(input_ime::OnActivate::Create(engine_id));

    DispatchEventToExtension(profile_, extension_id_,
                             input_ime::OnActivate::kEventName, args.Pass());
  }

  virtual void OnDeactivated(const std::string& engine_id) OVERRIDE {
    if (profile_ == NULL || extension_id_.empty())
      return;

    scoped_ptr<base::ListValue> args(
        input_ime::OnDeactivated::Create(engine_id));

    DispatchEventToExtension(profile_, extension_id_,
                             input_ime::OnDeactivated::kEventName, args.Pass());
  }

  virtual void OnFocus(
      const InputMethodEngine::InputContext& context) OVERRIDE {
    if (profile_ == NULL || extension_id_.empty())
      return;

    input_ime::InputContext context_value;
    context_value.context_id = context.id;
    context_value.type = input_ime::InputContext::ParseType(context.type);

    scoped_ptr<base::ListValue> args(input_ime::OnFocus::Create(context_value));

    DispatchEventToExtension(profile_, extension_id_,
                             input_ime::OnFocus::kEventName, args.Pass());
  }

  virtual void OnBlur(int context_id) OVERRIDE {
    if (profile_ == NULL || extension_id_.empty())
      return;

    scoped_ptr<base::ListValue> args(input_ime::OnBlur::Create(context_id));

    DispatchEventToExtension(profile_, extension_id_,
                             input_ime::OnBlur::kEventName, args.Pass());
  }

  virtual void OnInputContextUpdate(
      const InputMethodEngine::InputContext& context) OVERRIDE {
    if (profile_ == NULL || extension_id_.empty())
      return;

    input_ime::InputContext context_value;
    context_value.context_id = context.id;
    context_value.type = input_ime::InputContext::ParseType(context.type);

    scoped_ptr<base::ListValue> args(
        input_ime::OnInputContextUpdate::Create(context_value));

    DispatchEventToExtension(profile_,
                             extension_id_,
                             input_ime::OnInputContextUpdate::kEventName,
                             args.Pass());
  }

  virtual void OnKeyEvent(
      const std::string& engine_id,
      const InputMethodEngine::KeyboardEvent& event,
      chromeos::input_method::KeyEventHandle* key_data) OVERRIDE {
    if (profile_ == NULL || extension_id_.empty())
      return;

    std::string request_id =
        extensions::InputImeEventRouter::GetInstance()->AddRequest(engine_id,
                                                                   key_data);

    input_ime::KeyboardEvent key_data_value;
    key_data_value.type = input_ime::KeyboardEvent::ParseType(event.type);
    key_data_value.request_id = request_id;
    key_data_value.key = event.key;
    key_data_value.code = event.code;
    key_data_value.alt_key.reset(new bool(event.alt_key));
    key_data_value.ctrl_key.reset(new bool(event.ctrl_key));
    key_data_value.shift_key.reset(new bool(event.shift_key));
    key_data_value.caps_lock.reset(new bool(event.caps_lock));

    scoped_ptr<base::ListValue> args(
        input_ime::OnKeyEvent::Create(engine_id, key_data_value));

    DispatchEventToExtension(profile_, extension_id_,
                             input_ime::OnKeyEvent::kEventName, args.Pass());
  }

  virtual void OnCandidateClicked(
      const std::string& engine_id,
      int candidate_id,
      chromeos::InputMethodEngine::MouseButtonEvent button) OVERRIDE {
    if (profile_ == NULL || extension_id_.empty())
      return;

    input_ime::OnCandidateClicked::Button button_enum =
        input_ime::OnCandidateClicked::BUTTON_NONE;
    switch (button) {
      case chromeos::InputMethodEngine::MOUSE_BUTTON_MIDDLE:
        button_enum = input_ime::OnCandidateClicked::BUTTON_MIDDLE;
        break;

      case chromeos::InputMethodEngine::MOUSE_BUTTON_RIGHT:
        button_enum = input_ime::OnCandidateClicked::BUTTON_RIGHT;
        break;

      case chromeos::InputMethodEngine::MOUSE_BUTTON_LEFT:
      // Default to left.
      default:
        button_enum = input_ime::OnCandidateClicked::BUTTON_LEFT;
        break;
    }

    scoped_ptr<base::ListValue> args(
        input_ime::OnCandidateClicked::Create(engine_id,
                                              candidate_id,
                                              button_enum));

    DispatchEventToExtension(profile_,
                             extension_id_,
                             input_ime::OnCandidateClicked::kEventName,
                             args.Pass());
  }

  virtual void OnMenuItemActivated(const std::string& engine_id,
                                   const std::string& menu_id) OVERRIDE {
    if (profile_ == NULL || extension_id_.empty())
      return;

    scoped_ptr<base::ListValue> args(
        input_ime::OnMenuItemActivated::Create(engine_id, menu_id));

    DispatchEventToExtension(profile_,
                             extension_id_,
                             input_ime::OnMenuItemActivated::kEventName,
                             args.Pass());
  }

  virtual void OnSurroundingTextChanged(const std::string& engine_id,
                                        const std::string& text,
                                        int cursor_pos,
                                        int anchor_pos) OVERRIDE {
    if (profile_ == NULL || extension_id_.empty())
      return;

    input_ime::OnSurroundingTextChanged::SurroundingInfo info;
    info.text = text;
    info.focus = cursor_pos;
    info.anchor = anchor_pos;
    scoped_ptr<base::ListValue> args(
        input_ime::OnSurroundingTextChanged::Create(engine_id, info));

    DispatchEventToExtension(profile_,
                             extension_id_,
                             input_ime::OnSurroundingTextChanged::kEventName,
                             args.Pass());
  }

  virtual void OnReset(const std::string& engine_id) OVERRIDE {
    if (profile_ == NULL || extension_id_.empty())
      return;

    scoped_ptr<base::ListValue> args(input_ime::OnReset::Create(engine_id));

    DispatchEventToExtension(profile_,
                             extension_id_,
                             input_ime::OnReset::kEventName,
                             args.Pass());
  }

 private:
  Profile* profile_;
  std::string extension_id_;
  std::string engine_id_;

  DISALLOW_COPY_AND_ASSIGN(ImeObserver);
};

}  // namespace chromeos

namespace extensions {

InputImeEventRouter*
InputImeEventRouter::GetInstance() {
  return Singleton<InputImeEventRouter>::get();
}

#if defined(OS_CHROMEOS)
bool InputImeEventRouter::RegisterIme(
    Profile* profile,
    const std::string& extension_id,
    const extensions::InputComponentInfo& component) {
  VLOG(1) << "RegisterIme: " << extension_id << " id: " << component.id;

  std::map<std::string, chromeos::InputMethodEngine*>& engine_map =
      engines_[extension_id];

  std::map<std::string, chromeos::InputMethodEngine*>::iterator engine_ix =
      engine_map.find(component.id);
  if (engine_ix != engine_map.end())
    return false;

  std::string error;
  chromeos::ImeObserver* observer = new chromeos::ImeObserver(profile,
                                                              extension_id,
                                                              component.id);
  std::vector<std::string> layouts;
  layouts.assign(component.layouts.begin(), component.layouts.end());

  std::vector<std::string> languages;
  languages.assign(component.languages.begin(), component.languages.end());

  chromeos::InputMethodEngine* engine =
      chromeos::InputMethodEngine::CreateEngine(
          observer, component.name.c_str(), extension_id.c_str(),
          component.id.c_str(), component.description.c_str(),
          languages, layouts, component.options_page_url,
          &error);
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

void InputImeEventRouter::UnregisterAllImes(
    Profile* profile, const std::string& extension_id) {
  std::map<std::string,
           std::map<std::string,
                    chromeos::InputMethodEngine*> >::iterator engine_map =
      engines_.find(extension_id);
  if (engine_map != engines_.end()) {
    STLDeleteContainerPairSecondPointers(engine_map->second.begin(),
                                         engine_map->second.end());
    engines_.erase(engine_map);
  }

  std::map<std::string,
           std::map<std::string,
                    chromeos::ImeObserver*> >::iterator observer_list =
      observers_.find(extension_id);
  if (observer_list != observers_.end()) {
    STLDeleteContainerPairSecondPointers(observer_list->second.begin(),
                                         observer_list->second.end());
    observers_.erase(observer_list);
  }
}
#endif

chromeos::InputMethodEngine* InputImeEventRouter::GetEngine(
    const std::string& extension_id, const std::string& engine_id) {
  std::map<std::string,
           std::map<std::string, chromeos::InputMethodEngine*> >::const_iterator
               engine_list = engines_.find(extension_id);
  if (engine_list != engines_.end()) {
    std::map<std::string, chromeos::InputMethodEngine*>::const_iterator
        engine_ix = engine_list->second.find(engine_id);
    if (engine_ix != engine_list->second.end())
      return engine_ix->second;
  }
  return NULL;
}

chromeos::InputMethodEngine* InputImeEventRouter::GetActiveEngine(
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
      if (engine_ix->second->IsActive())
        return engine_ix->second;
    }
  }
  return NULL;
}

void InputImeEventRouter::OnKeyEventHandled(
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

std::string InputImeEventRouter::AddRequest(
    const std::string& engine_id,
    chromeos::input_method::KeyEventHandle* key_data) {
  std::string request_id = base::IntToString(next_request_id_);
  ++next_request_id_;

  request_map_[request_id] = std::make_pair(engine_id, key_data);

  return request_id;
}

InputImeEventRouter::InputImeEventRouter()
  : next_request_id_(1) {
}

InputImeEventRouter::~InputImeEventRouter() {}

bool InputImeSetCompositionFunction::RunImpl() {
  chromeos::InputMethodEngine* engine =
      InputImeEventRouter::GetInstance()->GetActiveEngine(extension_id());
  if (!engine) {
    SetResult(new base::FundamentalValue(false));
    return true;
  }

  scoped_ptr<SetComposition::Params> parent_params(
      SetComposition::Params::Create(*args_));
  const SetComposition::Params::Parameters& params = parent_params->parameters;
  std::vector<chromeos::InputMethodEngine::SegmentInfo> segments;
  if (params.segments) {
    const std::vector<linked_ptr<
        SetComposition::Params::Parameters::SegmentsType> >&
            segments_args = *params.segments;
    for (size_t i = 0; i < segments_args.size(); ++i) {
      EXTENSION_FUNCTION_VALIDATE(
          segments_args[i]->style !=
          SetComposition::Params::Parameters::SegmentsType::STYLE_NONE);
      segments.push_back(chromeos::InputMethodEngine::SegmentInfo());
      segments.back().start = segments_args[i]->start;
      segments.back().end = segments_args[i]->end;
      if (segments_args[i]->style ==
          SetComposition::Params::Parameters::SegmentsType::STYLE_UNDERLINE) {
        segments.back().style =
            chromeos::InputMethodEngine::SEGMENT_STYLE_UNDERLINE;
      } else {
        segments.back().style =
            chromeos::InputMethodEngine::SEGMENT_STYLE_DOUBLE_UNDERLINE;
      }
    }
  }

  int selection_start =
      params.selection_start ? *params.selection_start : params.cursor;
  int selection_end =
      params.selection_end ? *params.selection_end : params.cursor;

  SetResult(new base::FundamentalValue(
      engine->SetComposition(params.context_id, params.text.c_str(),
                             selection_start, selection_end, params.cursor,
                             segments, &error_)));
  return true;
}

bool InputImeClearCompositionFunction::RunImpl() {
  chromeos::InputMethodEngine* engine =
      InputImeEventRouter::GetInstance()->GetActiveEngine(extension_id());
  if (!engine) {
    SetResult(new base::FundamentalValue(false));
    return true;
  }

  scoped_ptr<ClearComposition::Params> parent_params(
      ClearComposition::Params::Create(*args_));
  const ClearComposition::Params::Parameters& params =
      parent_params->parameters;

  SetResult(new base::FundamentalValue(
      engine->ClearComposition(params.context_id, &error_)));
  return true;
}

bool InputImeCommitTextFunction::RunImpl() {
  // TODO(zork): Support committing when not active.
  chromeos::InputMethodEngine* engine =
      InputImeEventRouter::GetInstance()->GetActiveEngine(extension_id());
  if (!engine) {
    SetResult(new base::FundamentalValue(false));
    return true;
  }

  scoped_ptr<CommitText::Params> parent_params(
      CommitText::Params::Create(*args_));
  const CommitText::Params::Parameters& params =
      parent_params->parameters;

  SetResult(new base::FundamentalValue(
      engine->CommitText(params.context_id, params.text.c_str(), &error_)));
  return true;
}

bool InputImeSetCandidateWindowPropertiesFunction::RunImpl() {
  scoped_ptr<SetCandidateWindowProperties::Params> parent_params(
      SetCandidateWindowProperties::Params::Create(*args_));
  const SetCandidateWindowProperties::Params::Parameters&
      params = parent_params->parameters;

  chromeos::InputMethodEngine* engine =
      InputImeEventRouter::GetInstance()->GetEngine(extension_id(),
                                                    params.engine_id);

  if (!engine) {
    SetResult(new base::FundamentalValue(false));
    return true;
  }

  const SetCandidateWindowProperties::Params::Parameters::Properties&
      properties = params.properties;

  if (properties.visible &&
      !engine->SetCandidateWindowVisible(*properties.visible, &error_)) {
    SetResult(new base::FundamentalValue(false));
    return true;
  }

  chromeos::InputMethodEngine::CandidateWindowProperty properties_out =
    engine->GetCandidateWindowProperty();
  bool modified = false;

  if (properties.cursor_visible) {
    properties_out.is_cursor_visible = *properties.cursor_visible;
    modified = true;
  }

  if (properties.vertical) {
    properties_out.is_vertical = *properties.vertical;
    modified = true;
  }

  if (properties.page_size) {
    properties_out.page_size = *properties.page_size;
    modified = true;
  }

  if (properties.window_position ==
      SetCandidateWindowProperties::Params::Parameters::Properties::
          WINDOW_POSITION_COMPOSITION) {
    properties_out.show_window_at_composition = true;
    modified = true;
  } else if (properties.window_position ==
             SetCandidateWindowProperties::Params::Parameters::Properties::
                 WINDOW_POSITION_CURSOR) {
    properties_out.show_window_at_composition = false;
    modified = true;
  }

  if (modified) {
    engine->SetCandidateWindowProperty(properties_out);
  }

  if (properties.auxiliary_text)
    engine->SetCandidateWindowAuxText(properties.auxiliary_text->c_str());

  if (properties.auxiliary_text_visible) {
    engine->SetCandidateWindowAuxTextVisible(
        *properties.auxiliary_text_visible);
  }

  SetResult(new base::FundamentalValue(true));

  return true;
}

#if defined(OS_CHROMEOS)
bool InputImeSetCandidatesFunction::RunImpl() {
  chromeos::InputMethodEngine* engine =
      InputImeEventRouter::GetInstance()->GetActiveEngine(extension_id());
  if (!engine) {
    SetResult(new base::FundamentalValue(false));
    return true;
  }

  scoped_ptr<SetCandidates::Params> parent_params(
      SetCandidates::Params::Create(*args_));
  const SetCandidates::Params::Parameters& params =
      parent_params->parameters;

  std::vector<chromeos::InputMethodEngine::Candidate> candidates_out;
  const std::vector<linked_ptr<
      SetCandidates::Params::Parameters::CandidatesType> >& candidates_in =
          params.candidates;
  for (size_t i = 0; i < candidates_in.size(); ++i) {
    candidates_out.push_back(chromeos::InputMethodEngine::Candidate());
    candidates_out.back().value = candidates_in[i]->candidate;
    candidates_out.back().id = candidates_in[i]->id;
    if (candidates_in[i]->label)
      candidates_out.back().label = *candidates_in[i]->label;
    if (candidates_in[i]->annotation)
      candidates_out.back().annotation = *candidates_in[i]->annotation;
    if (candidates_in[i]->usage) {
      candidates_out.back().usage.title = candidates_in[i]->usage->title;
      candidates_out.back().usage.body = candidates_in[i]->usage->body;
    }
  }

  SetResult(new base::FundamentalValue(
      engine->SetCandidates(params.context_id, candidates_out, &error_)));
  return true;
}

bool InputImeSetCursorPositionFunction::RunImpl() {
  chromeos::InputMethodEngine* engine =
      InputImeEventRouter::GetInstance()->GetActiveEngine(extension_id());
  if (!engine) {
    SetResult(new base::FundamentalValue(false));
    return true;
  }

  scoped_ptr<SetCursorPosition::Params> parent_params(
      SetCursorPosition::Params::Create(*args_));
  const SetCursorPosition::Params::Parameters& params =
      parent_params->parameters;

  SetResult(new base::FundamentalValue(
      engine->SetCursorPosition(params.context_id, params.candidate_id,
                                &error_)));
  return true;
}

bool InputImeSetMenuItemsFunction::RunImpl() {
  scoped_ptr<SetMenuItems::Params> parent_params(
      SetMenuItems::Params::Create(*args_));
  const SetMenuItems::Params::Parameters& params =
      parent_params->parameters;

  chromeos::InputMethodEngine* engine =
      InputImeEventRouter::GetInstance()->GetEngine(extension_id(),
                                                    params.engine_id);
  if (!engine) {
    error_ = kErrorEngineNotAvailable;
    return false;
  }

  const std::vector<linked_ptr<input_ime::MenuItem> >& items = params.items;
  std::vector<chromeos::InputMethodEngine::MenuItem> items_out;

  for (size_t i = 0; i < items.size(); ++i) {
    items_out.push_back(chromeos::InputMethodEngine::MenuItem());
    SetMenuItemToMenu(*items[i], &items_out.back());
  }

  if (!engine->SetMenuItems(items_out))
    error_ = kErrorSetMenuItemsFail;
  return true;
}

bool InputImeUpdateMenuItemsFunction::RunImpl() {
  scoped_ptr<UpdateMenuItems::Params> parent_params(
      UpdateMenuItems::Params::Create(*args_));
  const UpdateMenuItems::Params::Parameters& params =
      parent_params->parameters;

  chromeos::InputMethodEngine* engine =
      InputImeEventRouter::GetInstance()->GetEngine(extension_id(),
                                                    params.engine_id);
  if (!engine) {
    error_ = kErrorEngineNotAvailable;
    return false;
  }

  const std::vector<linked_ptr<input_ime::MenuItem> >& items = params.items;
  std::vector<chromeos::InputMethodEngine::MenuItem> items_out;

  for (size_t i = 0; i < items.size(); ++i) {
    items_out.push_back(chromeos::InputMethodEngine::MenuItem());
    SetMenuItemToMenu(*items[i], &items_out.back());
  }

  if (!engine->UpdateMenuItems(items_out))
    error_ = kErrorUpdateMenuItemsFail;
  return true;
}

bool InputImeDeleteSurroundingTextFunction::RunImpl() {
  scoped_ptr<DeleteSurroundingText::Params> parent_params(
      DeleteSurroundingText::Params::Create(*args_));
  const DeleteSurroundingText::Params::Parameters& params =
      parent_params->parameters;

  chromeos::InputMethodEngine* engine =
      InputImeEventRouter::GetInstance()->GetEngine(extension_id(),
                                                    params.engine_id);
  if (!engine) {
    error_ = kErrorEngineNotAvailable;
    return false;
  }

  engine->DeleteSurroundingText(params.context_id, params.offset, params.length,
                                &error_);
  return true;
}

bool InputImeKeyEventHandledFunction::RunImpl() {
  scoped_ptr<KeyEventHandled::Params> params(
      KeyEventHandled::Params::Create(*args_));
  InputImeEventRouter::GetInstance()->OnKeyEventHandled(
      extension_id(), params->request_id, params->response);
  return true;
}
#endif

InputImeAPI::InputImeAPI(Profile* profile)
    : profile_(profile) {
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                 content::Source<Profile>(profile));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile));
}

InputImeAPI::~InputImeAPI() {
}

static base::LazyInstance<ProfileKeyedAPIFactory<InputImeAPI> >
g_factory = LAZY_INSTANCE_INITIALIZER;

// static
ProfileKeyedAPIFactory<InputImeAPI>* InputImeAPI::GetFactoryInstance() {
  return &g_factory.Get();
}

void InputImeAPI::Observe(int type,
                          const content::NotificationSource& source,
                          const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_EXTENSION_LOADED) {
    const Extension* extension =
        content::Details<const Extension>(details).ptr();
    const std::vector<InputComponentInfo>* input_components =
        extensions::InputComponents::GetInputComponents(extension);
    if (!input_components)
      return;
    for (std::vector<extensions::InputComponentInfo>::const_iterator component =
        input_components->begin(); component != input_components->end();
        ++component) {
      if (component->type == extensions::INPUT_COMPONENT_TYPE_IME) {
        input_ime_event_router()->RegisterIme(
            profile_, extension->id(), *component);
      }
    }
  } else if (type == chrome::NOTIFICATION_EXTENSION_UNLOADED) {
    const Extension* extension =
        content::Details<const UnloadedExtensionInfo>(details)->extension;
    const std::vector<InputComponentInfo>* input_components =
        extensions::InputComponents::GetInputComponents(extension);
    if (!input_components)
      return;
    if (input_components->size() > 0)
      input_ime_event_router()->UnregisterAllImes(profile_, extension->id());
  }
}

InputImeEventRouter* InputImeAPI::input_ime_event_router() {
  return InputImeEventRouter::GetInstance();
}

}  // namespace extensions
