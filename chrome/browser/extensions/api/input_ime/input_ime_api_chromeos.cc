// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/input_ime/input_ime_api.h"

#include <stddef.h>
#include <utility>

#include "base/macros.h"
#include "chrome/browser/chromeos/input_method/input_method_engine.h"
#include "chrome/browser/chromeos/login/lock/screen_locker.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/login/ui/user_adding_screen.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/extensions/api/input_ime.h"
#include "chrome/common/extensions/api/input_method_private.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "ui/base/ime/chromeos/component_extension_ime_manager.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/ime/ime_engine_handler_interface.h"

namespace input_ime = extensions::api::input_ime;
namespace DeleteSurroundingText =
    extensions::api::input_ime::DeleteSurroundingText;
namespace UpdateMenuItems = extensions::api::input_ime::UpdateMenuItems;
namespace SendKeyEvents = extensions::api::input_ime::SendKeyEvents;
namespace HideInputView = extensions::api::input_ime::HideInputView;
namespace SetMenuItems = extensions::api::input_ime::SetMenuItems;
namespace SetCursorPosition = extensions::api::input_ime::SetCursorPosition;
namespace SetCandidates = extensions::api::input_ime::SetCandidates;
namespace SetCandidateWindowProperties =
    extensions::api::input_ime::SetCandidateWindowProperties;
namespace CommitText = extensions::api::input_ime::CommitText;
namespace ClearComposition = extensions::api::input_ime::ClearComposition;
namespace SetComposition = extensions::api::input_ime::SetComposition;
namespace OnCompositionBoundsChanged =
    extensions::api::input_method_private::OnCompositionBoundsChanged;
using ui::IMEEngineHandlerInterface;
using input_method::InputMethodEngineBase;
using chromeos::InputMethodEngine;

namespace {
const char kErrorEngineNotAvailable[] = "Engine is not available";
const char kErrorSetMenuItemsFail[] = "Could not create menu Items";
const char kErrorUpdateMenuItemsFail[] = "Could not update menu Items";

void SetMenuItemToMenu(const input_ime::MenuItem& input,
                       InputMethodEngine::MenuItem* out) {
  out->modified = 0;
  out->id = input.id;
  if (input.label) {
    out->modified |= InputMethodEngine::MENU_ITEM_MODIFIED_LABEL;
    out->label = *input.label;
  }

  if (input.style != input_ime::MENU_ITEM_STYLE_NONE) {
    out->modified |= InputMethodEngine::MENU_ITEM_MODIFIED_STYLE;
    out->style = static_cast<InputMethodEngine::MenuItemStyle>(input.style);
  }

  if (input.visible)
    out->modified |= InputMethodEngine::MENU_ITEM_MODIFIED_VISIBLE;
  out->visible = input.visible ? *input.visible : true;

  if (input.checked)
    out->modified |= InputMethodEngine::MENU_ITEM_MODIFIED_CHECKED;
  out->checked = input.checked ? *input.checked : false;

  if (input.enabled)
    out->modified |= InputMethodEngine::MENU_ITEM_MODIFIED_ENABLED;
  out->enabled = input.enabled ? *input.enabled : true;
}

class ImeObserverChromeOS : public ui::ImeObserver {
 public:
  ImeObserverChromeOS(const std::string& extension_id, Profile* profile)
      : ImeObserver(extension_id, profile) {}

  ~ImeObserverChromeOS() override {}

  // input_method::InputMethodEngineBase::Observer overrides.
  void OnInputContextUpdate(
      const IMEEngineHandlerInterface::InputContext& context) override {
    if (extension_id_.empty() ||
        !HasListener(input_ime::OnInputContextUpdate::kEventName))
      return;

    input_ime::InputContext context_value;
    context_value.context_id = context.id;
    context_value.type =
        input_ime::ParseInputContextType(ConvertInputContextType(context));

    scoped_ptr<base::ListValue> args(
        input_ime::OnInputContextUpdate::Create(context_value));

    DispatchEventToExtension(
        extensions::events::INPUT_IME_ON_INPUT_CONTEXT_UPDATE,
        input_ime::OnInputContextUpdate::kEventName, std::move(args));
  }

  void OnCandidateClicked(
      const std::string& component_id,
      int candidate_id,
      InputMethodEngineBase::MouseButtonEvent button) override {
    if (extension_id_.empty() ||
        !HasListener(input_ime::OnCandidateClicked::kEventName))
      return;

    input_ime::MouseButton button_enum = input_ime::MOUSE_BUTTON_NONE;
    switch (button) {
      case InputMethodEngineBase::MOUSE_BUTTON_MIDDLE:
        button_enum = input_ime::MOUSE_BUTTON_MIDDLE;
        break;

      case InputMethodEngineBase::MOUSE_BUTTON_RIGHT:
        button_enum = input_ime::MOUSE_BUTTON_RIGHT;
        break;

      case InputMethodEngineBase::MOUSE_BUTTON_LEFT:
      // Default to left.
      default:
        button_enum = input_ime::MOUSE_BUTTON_LEFT;
        break;
    }

    scoped_ptr<base::ListValue> args(input_ime::OnCandidateClicked::Create(
        component_id, candidate_id, button_enum));

    DispatchEventToExtension(extensions::events::INPUT_IME_ON_CANDIDATE_CLICKED,
                             input_ime::OnCandidateClicked::kEventName,
                             std::move(args));
  }

  void OnMenuItemActivated(const std::string& component_id,
                           const std::string& menu_id) override {
    if (extension_id_.empty() ||
        !HasListener(input_ime::OnMenuItemActivated::kEventName))
      return;

    scoped_ptr<base::ListValue> args(
        input_ime::OnMenuItemActivated::Create(component_id, menu_id));

    DispatchEventToExtension(
        extensions::events::INPUT_IME_ON_MENU_ITEM_ACTIVATED,
        input_ime::OnMenuItemActivated::kEventName, std::move(args));
  }

  void OnCompositionBoundsChanged(
      const std::vector<gfx::Rect>& bounds) override {
    if (extension_id_.empty() ||
        !HasListener(OnCompositionBoundsChanged::kEventName))
      return;

    // Note: this is a private API event.
    base::ListValue* bounds_list = new base::ListValue();
    for (size_t i = 0; i < bounds.size(); ++i) {
      base::DictionaryValue* bounds_value = new base::DictionaryValue();
      bounds_value->SetInteger("x", bounds[i].x());
      bounds_value->SetInteger("y", bounds[i].y());
      bounds_value->SetInteger("w", bounds[i].width());
      bounds_value->SetInteger("h", bounds[i].height());
      bounds_list->Append(bounds_value);
    }

    if (bounds_list->GetSize() <= 0)
      return;
    scoped_ptr<base::ListValue> args(new base::ListValue());

    // The old extension code uses the first parameter to get the bounds of the
    // first composition character, so for backward compatibility, add it here.
    base::Value* first_value = NULL;
    if (bounds_list->Get(0, &first_value))
      args->Append(first_value->DeepCopy());
    args->Append(bounds_list);

    DispatchEventToExtension(
        extensions::events::INPUT_METHOD_PRIVATE_ON_COMPOSITION_BOUNDS_CHANGED,
        OnCompositionBoundsChanged::kEventName, std::move(args));
  }

 private:
  // ui::ImeObserver overrides.
  void DispatchEventToExtension(
      extensions::events::HistogramValue histogram_value,
      const std::string& event_name,
      scoped_ptr<base::ListValue> args) override {
    if (event_name != input_ime::OnActivate::kEventName) {
      // For suspended IME extension (e.g. XKB extension), don't awake it by IME
      // events except onActivate. The IME extension should be awake by other
      // events (e.g. runtime.onMessage) from its other pages.
      // This is to save memory for steady state Chrome OS on which the users
      // don't want any IME features.
      extensions::ExtensionSystem* extension_system =
          extensions::ExtensionSystem::Get(profile_);
      if (extension_system) {
        const extensions::Extension* extension =
            extension_system->extension_service()->GetExtensionById(
                extension_id_, false /* include_disabled */);
        if (!extension)
          return;
        extensions::ProcessManager* process_manager =
            extensions::ProcessManager::Get(profile_);
        if (extensions::BackgroundInfo::HasBackgroundPage(extension) &&
            !process_manager->GetBackgroundHostForExtension(extension_id_)) {
          return;
        }
      }
    }

    scoped_ptr<extensions::Event> event(
        new extensions::Event(histogram_value, event_name, std::move(args)));
    event->restrict_to_browser_context = profile_;
    extensions::EventRouter::Get(profile_)
        ->DispatchEventToExtension(extension_id_, std::move(event));
  }

  // The component IME extensions need to know the current screen type (e.g.
  // lock screen, login screen, etc.) so that its on-screen keyboard page
  // won't open new windows/pages. See crbug.com/395621.
  std::string GetCurrentScreenType() override {
    switch (chromeos::input_method::InputMethodManager::Get()
                ->GetUISessionState()) {
      case chromeos::input_method::InputMethodManager::STATE_LOGIN_SCREEN:
        return "login";
      case chromeos::input_method::InputMethodManager::STATE_LOCK_SCREEN:
        return "lock";
      case chromeos::input_method::InputMethodManager::STATE_BROWSER_SCREEN:
        return chromeos::UserAddingScreen::Get()->IsRunning()
                   ? "secondary-login"
                   : "normal";
      case chromeos::input_method::InputMethodManager::STATE_TERMINATING:
        return "normal";
    }
    NOTREACHED() << "New screen type is added. Please add new entry above.";
    return "normal";
  }

  DISALLOW_COPY_AND_ASSIGN(ImeObserverChromeOS);
};

}  // namespace

namespace extensions {

InputMethodEngine* GetActiveEngine(Profile* profile,
                                   const std::string& extension_id) {
  InputImeEventRouter* event_router = GetInputImeEventRouter(profile);
  InputMethodEngine* engine =
      event_router ? static_cast<InputMethodEngine*>(
                         event_router->GetActiveEngine(extension_id))
                   : nullptr;
  return engine;
}

InputImeEventRouter::InputImeEventRouter(Profile* profile)
    : InputImeEventRouterBase(profile) {}

InputImeEventRouter::~InputImeEventRouter() {}

bool InputImeEventRouter::RegisterImeExtension(
    const std::string& extension_id,
    const std::vector<InputComponentInfo>& input_components) {
  VLOG(1) << "RegisterImeExtension: " << extension_id;

  if (engine_map_[extension_id])
    return false;

  chromeos::input_method::InputMethodManager* manager =
      chromeos::input_method::InputMethodManager::Get();
  chromeos::ComponentExtensionIMEManager* comp_ext_ime_manager =
      manager->GetComponentExtensionIMEManager();

  chromeos::input_method::InputMethodDescriptors descriptors;
  // Only creates descriptors for 3rd party IME extension, because the
  // descriptors for component IME extensions are managed by InputMethodUtil.
  if (!comp_ext_ime_manager->IsWhitelistedExtension(extension_id)) {
    for (std::vector<InputComponentInfo>::const_iterator it =
             input_components.begin();
         it != input_components.end();
         ++it) {
      const InputComponentInfo& component = *it;
      DCHECK(component.type == INPUT_COMPONENT_TYPE_IME);

      std::vector<std::string> layouts;
      layouts.assign(component.layouts.begin(), component.layouts.end());
      std::vector<std::string> languages;
      languages.assign(component.languages.begin(), component.languages.end());

      const std::string& input_method_id =
          chromeos::extension_ime_util::GetInputMethodID(extension_id,
                                                         component.id);
      descriptors.push_back(chromeos::input_method::InputMethodDescriptor(
          input_method_id,
          component.name,
          std::string(),  // TODO(uekawa): Set short name.
          layouts,
          languages,
          false,  // 3rd party IMEs are always not for login.
          component.options_page_url,
          component.input_view_url));
    }
  }

  scoped_ptr<InputMethodEngineBase::Observer> observer(
      new ImeObserverChromeOS(extension_id, profile()));
  chromeos::InputMethodEngine* engine = new chromeos::InputMethodEngine();
  engine->Initialize(std::move(observer), extension_id.c_str(), profile());
  engine_map_[extension_id] = engine;
  chromeos::UserSessionManager::GetInstance()
      ->GetDefaultIMEState(profile())
      ->AddInputMethodExtension(extension_id, descriptors, engine);

  return true;
}

void InputImeEventRouter::UnregisterAllImes(const std::string& extension_id) {
  std::map<std::string, InputMethodEngine*>::iterator it =
      engine_map_.find(extension_id);
  if (it != engine_map_.end()) {
    chromeos::input_method::InputMethodManager::Get()
        ->GetActiveIMEState()
        ->RemoveInputMethodExtension(extension_id);
    delete it->second;
    engine_map_.erase(it);
  }
}

InputMethodEngine* InputImeEventRouter::GetEngine(
    const std::string& extension_id,
    const std::string& component_id) {
  std::map<std::string, InputMethodEngine*>::iterator it =
      engine_map_.find(extension_id);
  return (it != engine_map_.end()) ? it->second : nullptr;
}

InputMethodEngineBase* InputImeEventRouter::GetActiveEngine(
    const std::string& extension_id) {
  std::map<std::string, InputMethodEngine*>::iterator it =
      engine_map_.find(extension_id);
  return (it != engine_map_.end() && it->second->IsActive()) ? it->second
                                                             : nullptr;
}

bool InputImeSetCompositionFunction::RunSync() {
  InputMethodEngine* engine = GetActiveEngine(
      Profile::FromBrowserContext(browser_context()), extension_id());
  if (!engine) {
    SetResult(new base::FundamentalValue(false));
    return true;
  }

  scoped_ptr<SetComposition::Params> parent_params(
      SetComposition::Params::Create(*args_));
  const SetComposition::Params::Parameters& params = parent_params->parameters;
  std::vector<InputMethodEngineBase::SegmentInfo> segments;
  if (params.segments) {
    const std::vector<linked_ptr<
        SetComposition::Params::Parameters::SegmentsType> >&
            segments_args = *params.segments;
    for (size_t i = 0; i < segments_args.size(); ++i) {
      EXTENSION_FUNCTION_VALIDATE(
          segments_args[i]->style !=
          input_ime::UNDERLINE_STYLE_NONE);
      segments.push_back(InputMethodEngineBase::SegmentInfo());
      segments.back().start = segments_args[i]->start;
      segments.back().end = segments_args[i]->end;
      if (segments_args[i]->style ==
          input_ime::UNDERLINE_STYLE_UNDERLINE) {
        segments.back().style = InputMethodEngineBase::SEGMENT_STYLE_UNDERLINE;
      } else if (segments_args[i]->style ==
                 input_ime::UNDERLINE_STYLE_DOUBLEUNDERLINE) {
        segments.back().style =
            InputMethodEngineBase::SEGMENT_STYLE_DOUBLE_UNDERLINE;
      } else {
        segments.back().style =
            InputMethodEngineBase::SEGMENT_STYLE_NO_UNDERLINE;
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

bool InputImeClearCompositionFunction::RunSync() {
  InputMethodEngine* engine = GetActiveEngine(
      Profile::FromBrowserContext(browser_context()), extension_id());
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

bool InputImeCommitTextFunction::RunSync() {
  InputMethodEngine* engine = GetActiveEngine(
      Profile::FromBrowserContext(browser_context()), extension_id());
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

bool InputImeHideInputViewFunction::RunAsync() {
  InputMethodEngine* engine = GetActiveEngine(
      Profile::FromBrowserContext(browser_context()), extension_id());
  if (!engine) {
    return true;
  }
  engine->HideInputView();
  return true;
}

bool InputImeSendKeyEventsFunction::RunAsync() {
  scoped_ptr<SendKeyEvents::Params> parent_params(
      SendKeyEvents::Params::Create(*args_));
  const SendKeyEvents::Params::Parameters& params =
      parent_params->parameters;
  InputMethodEngine* engine = GetActiveEngine(
      Profile::FromBrowserContext(browser_context()), extension_id());
  if (!engine) {
    error_ = kErrorEngineNotAvailable;
    return false;
  }

  const std::vector<linked_ptr<input_ime::KeyboardEvent> >& key_data =
      params.key_data;
  std::vector<InputMethodEngineBase::KeyboardEvent> key_data_out;

  for (size_t i = 0; i < key_data.size(); ++i) {
    InputMethodEngineBase::KeyboardEvent event;
    event.type = input_ime::ToString(key_data[i]->type);
    event.key = key_data[i]->key;
    event.code = key_data[i]->code;
    event.key_code = key_data[i]->key_code.get() ? *(key_data[i]->key_code) : 0;
    if (key_data[i]->alt_key)
      event.alt_key = *(key_data[i]->alt_key);
    if (key_data[i]->ctrl_key)
      event.ctrl_key = *(key_data[i]->ctrl_key);
    if (key_data[i]->shift_key)
      event.shift_key = *(key_data[i]->shift_key);
    if (key_data[i]->caps_lock)
      event.caps_lock = *(key_data[i]->caps_lock);
    key_data_out.push_back(event);
  }

  engine->SendKeyEvents(params.context_id, key_data_out);
  return true;
}

bool InputImeSetCandidateWindowPropertiesFunction::RunSync() {
  scoped_ptr<SetCandidateWindowProperties::Params> parent_params(
      SetCandidateWindowProperties::Params::Create(*args_));
  const SetCandidateWindowProperties::Params::Parameters&
      params = parent_params->parameters;

  InputImeEventRouter* event_router =
      GetInputImeEventRouter(Profile::FromBrowserContext(browser_context()));
  InputMethodEngine* engine =
      event_router ? event_router->GetEngine(extension_id(), params.engine_id)
                   : nullptr;
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

  InputMethodEngine::CandidateWindowProperty properties_out =
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

  if (properties.window_position == input_ime::WINDOW_POSITION_COMPOSITION) {
    properties_out.show_window_at_composition = true;
    modified = true;
  } else if (properties.window_position == input_ime::WINDOW_POSITION_CURSOR) {
    properties_out.show_window_at_composition = false;
    modified = true;
  }

  if (properties.auxiliary_text) {
    properties_out.auxiliary_text = *properties.auxiliary_text;
    modified = true;
  }

  if (properties.auxiliary_text_visible) {
    properties_out.is_auxiliary_text_visible =
        *properties.auxiliary_text_visible;
    modified = true;
  }

  if (modified) {
    engine->SetCandidateWindowProperty(properties_out);
  }

  SetResult(new base::FundamentalValue(true));

  return true;
}

bool InputImeSetCandidatesFunction::RunSync() {
  InputMethodEngine* engine = GetActiveEngine(
      Profile::FromBrowserContext(browser_context()), extension_id());
  if (!engine) {
    SetResult(new base::FundamentalValue(false));
    return true;
  }

  scoped_ptr<SetCandidates::Params> parent_params(
      SetCandidates::Params::Create(*args_));
  const SetCandidates::Params::Parameters& params =
      parent_params->parameters;

  std::vector<InputMethodEngine::Candidate> candidates_out;
  const std::vector<linked_ptr<
      SetCandidates::Params::Parameters::CandidatesType> >& candidates_in =
          params.candidates;
  for (size_t i = 0; i < candidates_in.size(); ++i) {
    candidates_out.push_back(InputMethodEngine::Candidate());
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

bool InputImeSetCursorPositionFunction::RunSync() {
  InputMethodEngine* engine = GetActiveEngine(
      Profile::FromBrowserContext(browser_context()), extension_id());
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

bool InputImeSetMenuItemsFunction::RunSync() {
  scoped_ptr<SetMenuItems::Params> parent_params(
      SetMenuItems::Params::Create(*args_));
  const SetMenuItems::Params::Parameters& params =
      parent_params->parameters;

  InputImeEventRouter* event_router =
      GetInputImeEventRouter(Profile::FromBrowserContext(browser_context()));
  InputMethodEngine* engine =
      event_router ? event_router->GetEngine(extension_id(), params.engine_id)
                   : nullptr;
  if (!engine) {
    error_ = kErrorEngineNotAvailable;
    return false;
  }

  const std::vector<linked_ptr<input_ime::MenuItem> >& items = params.items;
  std::vector<InputMethodEngine::MenuItem> items_out;

  for (size_t i = 0; i < items.size(); ++i) {
    items_out.push_back(InputMethodEngine::MenuItem());
    SetMenuItemToMenu(*items[i], &items_out.back());
  }

  if (!engine->SetMenuItems(items_out))
    error_ = kErrorSetMenuItemsFail;
  return true;
}

bool InputImeUpdateMenuItemsFunction::RunSync() {
  scoped_ptr<UpdateMenuItems::Params> parent_params(
      UpdateMenuItems::Params::Create(*args_));
  const UpdateMenuItems::Params::Parameters& params =
      parent_params->parameters;

  InputImeEventRouter* event_router =
      GetInputImeEventRouter(Profile::FromBrowserContext(browser_context()));
  InputMethodEngine* engine =
      event_router ? event_router->GetEngine(extension_id(), params.engine_id)
                   : nullptr;
  if (!engine) {
    error_ = kErrorEngineNotAvailable;
    return false;
  }

  const std::vector<linked_ptr<input_ime::MenuItem> >& items = params.items;
  std::vector<InputMethodEngine::MenuItem> items_out;

  for (size_t i = 0; i < items.size(); ++i) {
    items_out.push_back(InputMethodEngine::MenuItem());
    SetMenuItemToMenu(*items[i], &items_out.back());
  }

  if (!engine->UpdateMenuItems(items_out))
    error_ = kErrorUpdateMenuItemsFail;
  return true;
}

bool InputImeDeleteSurroundingTextFunction::RunSync() {
  scoped_ptr<DeleteSurroundingText::Params> parent_params(
      DeleteSurroundingText::Params::Create(*args_));
  const DeleteSurroundingText::Params::Parameters& params =
      parent_params->parameters;

  InputImeEventRouter* event_router =
      GetInputImeEventRouter(Profile::FromBrowserContext(browser_context()));
  InputMethodEngine* engine =
      event_router ? event_router->GetEngine(extension_id(), params.engine_id)
                   : nullptr;
  if (!engine) {
    error_ = kErrorEngineNotAvailable;
    return false;
  }

  engine->DeleteSurroundingText(params.context_id, params.offset, params.length,
                                &error_);
  return true;
}

void InputImeAPI::OnExtensionLoaded(content::BrowserContext* browser_context,
                                    const Extension* extension) {
  const std::vector<InputComponentInfo>* input_components =
      InputComponents::GetInputComponents(extension);
  InputImeEventRouter* event_router =
      GetInputImeEventRouter(Profile::FromBrowserContext(browser_context));
  if (input_components && event_router) {
    event_router->RegisterImeExtension(extension->id(), *input_components);
  }
}

void InputImeAPI::OnExtensionUnloaded(content::BrowserContext* browser_context,
                                      const Extension* extension,
                                      UnloadedExtensionInfo::Reason reason) {
  const std::vector<InputComponentInfo>* input_components =
      InputComponents::GetInputComponents(extension);
  InputImeEventRouter* event_router =
      GetInputImeEventRouter(Profile::FromBrowserContext(browser_context));
  if (input_components && !input_components->empty() && event_router) {
    event_router->UnregisterAllImes(extension->id());
  }
}

void InputImeAPI::OnListenerAdded(const EventListenerInfo& details) {
  if (!details.browser_context)
    return;
  InputMethodEngine* engine =
      GetActiveEngine(Profile::FromBrowserContext(details.browser_context),
                      details.extension_id);
  // Notifies the IME extension for IME ready with onActivate/onFocus events.
  if (engine)
    engine->Enable(engine->GetActiveComponentId());
}

}  // namespace extensions
