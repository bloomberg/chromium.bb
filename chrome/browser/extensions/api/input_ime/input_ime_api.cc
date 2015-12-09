// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/input_ime/input_ime_api.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/common/extensions/api/input_ime.h"
#include "chrome/common/extensions/api/input_ime/input_components_handler.h"
#include "extensions/browser/extension_registry.h"

namespace input_ime = extensions::api::input_ime;
namespace KeyEventHandled = extensions::api::input_ime::KeyEventHandled;
using ui::IMEEngineHandlerInterface;

namespace ui {

ImeObserver::ImeObserver(const std::string& extension_id, Profile* profile)
    : extension_id_(extension_id), profile_(profile) {}

void ImeObserver::OnFocus(
    const IMEEngineHandlerInterface::InputContext& context) {
  if (extension_id_.empty() || !HasListener(input_ime::OnFocus::kEventName))
    return;

  input_ime::InputContext context_value;
  context_value.context_id = context.id;
  context_value.type =
      input_ime::ParseInputContextType(ConvertInputContextType(context));
  context_value.auto_correct = ConvertInputContextAutoCorrect(context);
  context_value.auto_complete = ConvertInputContextAutoComplete(context);
  context_value.spell_check = ConvertInputContextSpellCheck(context);

  scoped_ptr<base::ListValue> args(input_ime::OnFocus::Create(context_value));

  DispatchEventToExtension(extensions::events::INPUT_IME_ON_FOCUS,
                           input_ime::OnFocus::kEventName, args.Pass());
}

void ImeObserver::OnBlur(int context_id) {
  if (extension_id_.empty() || !HasListener(input_ime::OnBlur::kEventName))
    return;

  scoped_ptr<base::ListValue> args(input_ime::OnBlur::Create(context_id));

  DispatchEventToExtension(extensions::events::INPUT_IME_ON_BLUR,
                           input_ime::OnBlur::kEventName, args.Pass());
}

void ImeObserver::OnKeyEvent(
    const std::string& component_id,
    const IMEEngineHandlerInterface::KeyboardEvent& event,
    IMEEngineHandlerInterface::KeyEventDoneCallback& key_data) {
  if (extension_id_.empty())
    return;

  // If there is no listener for the event, no need to dispatch the event to
  // extension. Instead, releases the key event for default system behavior.
  if (!ShouldForwardKeyEvent()) {
    // Continue processing the key event so that the physical keyboard can
    // still work.
    key_data.Run(false);
    return;
  }

  const std::string request_id = extensions::GetInputImeEventRouter(profile_)
                                     ->AddRequest(component_id, key_data);

  input_ime::KeyboardEvent key_data_value;
  key_data_value.type = input_ime::ParseKeyboardEventType(event.type);
  key_data_value.request_id = request_id;
  if (!event.extension_id.empty())
      key_data_value.extension_id.reset(new std::string(event.extension_id));
  key_data_value.key = event.key;
  key_data_value.code = event.code;
  key_data_value.alt_key.reset(new bool(event.alt_key));
  key_data_value.ctrl_key.reset(new bool(event.ctrl_key));
  key_data_value.shift_key.reset(new bool(event.shift_key));
  key_data_value.caps_lock.reset(new bool(event.caps_lock));

  scoped_ptr<base::ListValue> args(
      input_ime::OnKeyEvent::Create(component_id, key_data_value));

  DispatchEventToExtension(extensions::events::INPUT_IME_ON_KEY_EVENT,
                           input_ime::OnKeyEvent::kEventName, args.Pass());
}

void ImeObserver::OnReset(const std::string& component_id) {
  if (extension_id_.empty() || !HasListener(input_ime::OnReset::kEventName))
    return;

  scoped_ptr<base::ListValue> args(input_ime::OnReset::Create(component_id));

  DispatchEventToExtension(extensions::events::INPUT_IME_ON_RESET,
                           input_ime::OnReset::kEventName, args.Pass());
}

void ImeObserver::OnDeactivated(const std::string& component_id) {
  if (extension_id_.empty() ||
      !HasListener(input_ime::OnDeactivated::kEventName))
    return;

  scoped_ptr<base::ListValue> args(
      input_ime::OnDeactivated::Create(component_id));

  DispatchEventToExtension(extensions::events::INPUT_IME_ON_DEACTIVATED,
                           input_ime::OnDeactivated::kEventName, args.Pass());
}

// TODO(azurewei): This function implementation should be shared on all
// platforms, while with some changing on the current code on ChromeOS.
void ImeObserver::OnCompositionBoundsChanged(
    const std::vector<gfx::Rect>& bounds) {}

bool ImeObserver::ShouldForwardKeyEvent() const {
  // Only forward key events to extension if there are non-lazy listeners
  // for onKeyEvent. Because if something wrong with the lazy background
  // page which doesn't register listener for onKeyEvent, it will not handle
  // the key events, and therefore, all key events will be eaten.
  // This is for error-tolerance, and it means that onKeyEvent will never wake
  // up lazy background page.
  const extensions::EventListenerMap::ListenerList& listener_list =
      extensions::EventRouter::Get(profile_)
          ->listeners()
          .GetEventListenersByName(input_ime::OnKeyEvent::kEventName);
  for (extensions::EventListenerMap::ListenerList::const_iterator it =
           listener_list.begin();
       it != listener_list.end(); ++it) {
    if ((*it)->extension_id() == extension_id_ && !(*it)->IsLazy())
      return true;
  }
  return false;
}

bool ImeObserver::HasListener(const std::string& event_name) const {
  return extensions::EventRouter::Get(profile_)->HasEventListener(event_name);
}

std::string ImeObserver::ConvertInputContextType(
    ui::IMEEngineHandlerInterface::InputContext input_context) {
  std::string input_context_type = "text";
  switch (input_context.type) {
    case ui::TEXT_INPUT_TYPE_SEARCH:
      input_context_type = "search";
      break;
    case ui::TEXT_INPUT_TYPE_TELEPHONE:
      input_context_type = "tel";
      break;
    case ui::TEXT_INPUT_TYPE_URL:
      input_context_type = "url";
      break;
    case ui::TEXT_INPUT_TYPE_EMAIL:
      input_context_type = "email";
      break;
    case ui::TEXT_INPUT_TYPE_NUMBER:
      input_context_type = "number";
      break;
    case ui::TEXT_INPUT_TYPE_PASSWORD:
      input_context_type = "password";
      break;
    default:
      input_context_type = "text";
      break;
  }
  return input_context_type;
}

bool ImeObserver::ConvertInputContextAutoCorrect(
    ui::IMEEngineHandlerInterface::InputContext input_context) {
  return !(input_context.flags & ui::TEXT_INPUT_FLAG_AUTOCORRECT_OFF);
}

bool ImeObserver::ConvertInputContextAutoComplete(
    ui::IMEEngineHandlerInterface::InputContext input_context) {
  return !(input_context.flags & ui::TEXT_INPUT_FLAG_AUTOCOMPLETE_OFF);
}

bool ImeObserver::ConvertInputContextSpellCheck(
    ui::IMEEngineHandlerInterface::InputContext input_context) {
  return !(input_context.flags & ui::TEXT_INPUT_FLAG_SPELLCHECK_OFF);
}

}  // namespace ui

namespace extensions {

InputImeEventRouterFactory* InputImeEventRouterFactory::GetInstance() {
  return base::Singleton<InputImeEventRouterFactory>::get();
}

InputImeEventRouterFactory::InputImeEventRouterFactory() {
}

InputImeEventRouterFactory::~InputImeEventRouterFactory() {
}

InputImeEventRouter* InputImeEventRouterFactory::GetRouter(Profile* profile) {
  InputImeEventRouter* router = router_map_[profile];
  if (!router) {
    router = new InputImeEventRouter(profile);
    router_map_[profile] = router;
  }
  return router;
}

InputImeEventRouter::InputImeEventRouter(Profile* profile)
    : next_request_id_(1), profile_(profile) {
}

InputImeEventRouter::~InputImeEventRouter() {
}

IMEEngineHandlerInterface* InputImeEventRouter::GetEngine(
    const std::string& extension_id,
    const std::string& component_id) {
  std::map<std::string, IMEEngineHandlerInterface*>::iterator it =
      engine_map_.find(extension_id);
  if (it != engine_map_.end())
    return it->second;
  return NULL;
}

IMEEngineHandlerInterface* InputImeEventRouter::GetActiveEngine(
    const std::string& extension_id) {
  std::map<std::string, IMEEngineHandlerInterface*>::iterator it =
      engine_map_.find(extension_id);
  if (it != engine_map_.end() && it->second->IsActive())
    return it->second;
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

  std::string component_id = request->second.first;
  (request->second.second).Run(handled);
  request_map_.erase(request);
}

std::string InputImeEventRouter::AddRequest(
    const std::string& component_id,
    IMEEngineHandlerInterface::KeyEventDoneCallback& key_data) {
  std::string request_id = base::IntToString(next_request_id_);
  ++next_request_id_;

  request_map_[request_id] = std::make_pair(component_id, key_data);

  return request_id;
}

bool InputImeKeyEventHandledFunction::RunAsync() {
  scoped_ptr<KeyEventHandled::Params> params(
      KeyEventHandled::Params::Create(*args_));
  GetInputImeEventRouter(Profile::FromBrowserContext(browser_context()))
      ->OnKeyEventHandled(extension_id(), params->request_id, params->response);
  return true;
}

InputImeAPI::InputImeAPI(content::BrowserContext* context)
    : browser_context_(context), extension_registry_observer_(this) {
  extension_registry_observer_.Add(ExtensionRegistry::Get(browser_context_));

  EventRouter* event_router = EventRouter::Get(browser_context_);
  event_router->RegisterObserver(this, input_ime::OnFocus::kEventName);
}

InputImeAPI::~InputImeAPI() {
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<InputImeAPI> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<InputImeAPI>* InputImeAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

void InputImeAPI::OnExtensionLoaded(content::BrowserContext* browser_context,
                                    const Extension* extension) {
  const std::vector<InputComponentInfo>* input_components =
      extensions::InputComponents::GetInputComponents(extension);
  if (input_components)
    GetInputImeEventRouter(Profile::FromBrowserContext(browser_context))
        ->RegisterImeExtension(extension->id(), *input_components);
}

void InputImeAPI::OnExtensionUnloaded(content::BrowserContext* browser_context,
                                      const Extension* extension,
                                      UnloadedExtensionInfo::Reason reason) {
  const std::vector<InputComponentInfo>* input_components =
      extensions::InputComponents::GetInputComponents(extension);
  if (!input_components)
    return;
  if (input_components->size() > 0) {
    GetInputImeEventRouter(Profile::FromBrowserContext(browser_context))
        ->UnregisterAllImes(extension->id());
  }
}

void InputImeAPI::OnListenerAdded(const EventListenerInfo& details) {
  if (!details.browser_context)
    return;
  IMEEngineHandlerInterface* engine =
      GetInputImeEventRouter(
          Profile::FromBrowserContext(details.browser_context))
          ->GetActiveEngine(details.extension_id);
  // Notifies the IME extension for IME ready with onActivate/onFocus events.
  if (engine)
    engine->Enable(engine->GetActiveComponentId());
}

InputImeEventRouter* GetInputImeEventRouter(Profile* profile) {
  if (profile->HasOffTheRecordProfile())
    profile = profile->GetOffTheRecordProfile();
  return extensions::InputImeEventRouterFactory::GetInstance()->GetRouter(
      profile);
}

}  // namespace extensions
