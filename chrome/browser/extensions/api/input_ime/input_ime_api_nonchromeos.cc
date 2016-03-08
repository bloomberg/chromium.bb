// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is for non-chromeos (win & linux) functions, such as
// chrome.input.ime.activate, chrome.input.ime.createWindow and
// chrome.input.ime.onSelectionChanged.
// TODO(azurewei): May refactor the code structure by using delegate or
// redesign the API to remove this platform-specific file in the future.

#include "chrome/browser/extensions/api/input_ime/input_ime_api.h"

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/linked_ptr.h"
#include "chrome/browser/ui/input_method/input_method_engine.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/input_ime.h"
#include "ui/base/ime/ime_bridge.h"
#include "ui/gfx/geometry/rect.h"

namespace input_ime = extensions::api::input_ime;
namespace OnCompositionBoundsChanged =
    extensions::api::input_ime::OnCompositionBoundsChanged;
using ui::IMEEngineHandlerInterface;
using input_method::InputMethodEngine;
using input_method::InputMethodEngineBase;

namespace input_ime = extensions::api::input_ime;

namespace {

const char kErrorAPIDisabled[] =
    "The chrome.input.ime API is not supported on the current platform";
const char kErrorNoActiveEngine[] = "The extension has not been activated.";

bool IsInputImeEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableInputImeAPI);

}  // namespace

class ImeObserverNonChromeOS : public ui::ImeObserver {
 public:
  ImeObserverNonChromeOS(const std::string& extension_id, Profile* profile)
      : ImeObserver(extension_id, profile) {}

  ~ImeObserverNonChromeOS() override {}

  void OnCompositionBoundsChanged(
      const std::vector<gfx::Rect>& bounds) override {
    if (extension_id_.empty() || bounds.empty() ||
        !HasListener(OnCompositionBoundsChanged::kEventName))
      return;

    std::vector<linked_ptr<input_ime::Bounds>> bounds_list;
    for (const auto& bound : bounds) {
      linked_ptr<input_ime::Bounds> bounds_value(new input_ime::Bounds());
      bounds_value->left = bound.x();
      bounds_value->top = bound.y();
      bounds_value->width = bound.width();
      bounds_value->height = bound.height();
      bounds_list.push_back(bounds_value);
    }

    scoped_ptr<base::ListValue> args(
        OnCompositionBoundsChanged::Create(bounds_list));

    DispatchEventToExtension(
        extensions::events::INPUT_IME_ON_COMPOSITION_BOUNDS_CHANGED,
        OnCompositionBoundsChanged::kEventName, std::move(args));
  }

 private:
  // ImeObserver overrides.
  void DispatchEventToExtension(
      extensions::events::HistogramValue histogram_value,
      const std::string& event_name,
      scoped_ptr<base::ListValue> args) override {
    if (!IsInputImeEnabled()) {
      return;
    }

    scoped_ptr<extensions::Event> event(
        new extensions::Event(histogram_value, event_name, std::move(args)));
    event->restrict_to_browser_context = profile_;
    extensions::EventRouter::Get(profile_)
        ->DispatchEventToExtension(extension_id_, std::move(event));
  }

  std::string GetCurrentScreenType() override { return "normal"; }

  DISALLOW_COPY_AND_ASSIGN(ImeObserverNonChromeOS);
};

}  // namespace

namespace extensions {

InputMethodEngine* GetActiveEngine(content::BrowserContext* browser_context,
                                   const std::string& extension_id) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  InputImeEventRouter* event_router = GetInputImeEventRouter(profile);
  InputMethodEngine* engine =
      event_router ? static_cast<InputMethodEngine*>(
                         event_router->GetActiveEngine(extension_id))
                   : nullptr;
  return engine;
}

void InputImeAPI::OnExtensionLoaded(content::BrowserContext* browser_context,
                                    const Extension* extension) {
  // No-op if called multiple times.
  ui::IMEBridge::Initialize();
}

void InputImeAPI::OnExtensionUnloaded(content::BrowserContext* browser_context,
                                      const Extension* extension,
                                      UnloadedExtensionInfo::Reason reason) {
  InputImeEventRouter* event_router =
      GetInputImeEventRouter(Profile::FromBrowserContext(browser_context));
  if (event_router)
    event_router->DeleteInputMethodEngine(extension->id());
}

void InputImeAPI::OnListenerAdded(const EventListenerInfo& details) {}

InputImeEventRouter::InputImeEventRouter(Profile* profile)
    : InputImeEventRouterBase(profile), active_engine_(nullptr) {}

InputImeEventRouter::~InputImeEventRouter() {
  if (active_engine_)
    DeleteInputMethodEngine(active_engine_->GetExtensionId());
}

InputMethodEngineBase* InputImeEventRouter::GetActiveEngine(
    const std::string& extension_id) {
  return (ui::IMEBridge::Get()->GetCurrentEngineHandler() &&
          active_engine_ &&
          active_engine_->GetExtensionId() == extension_id)
             ? active_engine_
             : nullptr;
}

void InputImeEventRouter::SetActiveEngine(const std::string& extension_id) {
  if (active_engine_) {
    if (active_engine_->GetExtensionId() == extension_id) {
      active_engine_->Enable(std::string());
      ui::IMEBridge::Get()->SetCurrentEngineHandler(active_engine_);
      return;
    }
    DeleteInputMethodEngine(active_engine_->GetExtensionId());
  }

  scoped_ptr<input_method::InputMethodEngine> engine(
      new input_method::InputMethodEngine());
  scoped_ptr<InputMethodEngineBase::Observer> observer(
      new ImeObserverNonChromeOS(extension_id, profile()));
  engine->Initialize(std::move(observer), extension_id.c_str(), profile());
  engine->Enable(std::string());
  active_engine_ = engine.release();
  ui::IMEBridge::Get()->SetCurrentEngineHandler(active_engine_);
}

void InputImeEventRouter::DeleteInputMethodEngine(
    const std::string& extension_id) {
  if (active_engine_ && active_engine_->GetExtensionId() == extension_id) {
    active_engine_->Disable();
    ui::IMEBridge::Get()->SetCurrentEngineHandler(nullptr);
    delete active_engine_;
    active_engine_ = nullptr;
  }
}

ExtensionFunction::ResponseAction InputImeActivateFunction::Run() {
  if (!IsInputImeEnabled())
    return RespondNow(Error(kErrorAPIDisabled));

  InputImeEventRouter* event_router =
      GetInputImeEventRouter(Profile::FromBrowserContext(browser_context()));
  if (event_router)
    event_router->SetActiveEngine(extension_id());
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction InputImeDeactivateFunction::Run() {
  if (!IsInputImeEnabled())
    return RespondNow(Error(kErrorAPIDisabled));

  InputMethodEngine* engine =
      GetActiveEngine(browser_context(), extension_id());
  ui::IMEBridge::Get()->SetCurrentEngineHandler(nullptr);
  if (engine)
    engine->CloseImeWindows();
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction InputImeCreateWindowFunction::Run() {
  if (!IsInputImeEnabled())
    return RespondNow(Error(kErrorAPIDisabled));

  // Using input_ime::CreateWindow::Params::Create() causes the link errors on
  // Windows, only if the method name is 'createWindow'.
  // So doing the by-hand parameter unpacking here.
  // TODO(shuchen,rdevlin.cronin): investigate the root cause for the link
  // errors.
  const base::DictionaryValue* params = nullptr;
  args_->GetDictionary(0, &params);
  EXTENSION_FUNCTION_VALIDATE(params);
  input_ime::CreateWindowOptions options;
  input_ime::CreateWindowOptions::Populate(*params, &options);

  gfx::Rect bounds(0, 0, 100, 100);  // Default bounds.
  if (options.bounds.get()) {
    bounds.set_x(options.bounds->left);
    bounds.set_y(options.bounds->top);
    bounds.set_width(options.bounds->width);
    bounds.set_height(options.bounds->height);
  }

  InputMethodEngine* engine =
      GetActiveEngine(browser_context(), extension_id());
  if (!engine)
    return RespondNow(Error(kErrorNoActiveEngine));

  int frame_id = engine->CreateImeWindow(
      extension(), render_frame_host(),
      options.url.get() ? *options.url : url::kAboutBlankURL,
      options.window_type == input_ime::WINDOW_TYPE_FOLLOWCURSOR
          ? ui::ImeWindow::FOLLOW_CURSOR
          : ui::ImeWindow::NORMAL,
      bounds, &error_);
  if (!frame_id)
    return RespondNow(Error(error_));

  scoped_ptr<base::DictionaryValue> result(new base::DictionaryValue());
  result->Set("frameId", new base::FundamentalValue(frame_id));

  return RespondNow(OneArgument(std::move(result)));
}

ExtensionFunction::ResponseAction InputImeShowWindowFunction::Run() {
  if (!IsInputImeEnabled())
    return RespondNow(Error(kErrorAPIDisabled));

  InputMethodEngine* engine =
      GetActiveEngine(browser_context(), extension_id());
  if (!engine)
    return RespondNow(Error(kErrorNoActiveEngine));

  scoped_ptr<api::input_ime::ShowWindow::Params> params(
      api::input_ime::ShowWindow::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  engine->ShowImeWindow(params->window_id);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction InputImeHideWindowFunction::Run() {
  if (!IsInputImeEnabled())
    return RespondNow(Error(kErrorAPIDisabled));

  InputMethodEngine* engine =
      GetActiveEngine(browser_context(), extension_id());
  if (!engine)
    return RespondNow(Error(kErrorNoActiveEngine));

  scoped_ptr<api::input_ime::HideWindow::Params> params(
      api::input_ime::HideWindow::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  engine->HideImeWindow(params->window_id);
  return RespondNow(NoArguments());
}

}  // namespace extensions
