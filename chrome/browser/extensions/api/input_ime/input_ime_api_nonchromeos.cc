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
#include "chrome/browser/ui/input_method/input_method_engine.h"
#include "chrome/common/chrome_switches.h"
#include "ui/base/ime/ime_bridge.h"

using ui::IMEEngineHandlerInterface;
using input_method::InputMethodEngine;
using input_method::InputMethodEngineBase;

namespace {

const char kErrorAPIDisabled[] =
    "The chrome.input.ime API is not supported on the current platform";

bool IsInputImeEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableInputImeAPI);

}  // namespace

class ImeObserverNonChromeOS : public ui::ImeObserver {
 public:
  ImeObserverNonChromeOS(const std::string& extension_id, Profile* profile)
      : ImeObserver(extension_id, profile) {}

  ~ImeObserverNonChromeOS() override {}

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

void InputImeAPI::OnExtensionLoaded(content::BrowserContext* browser_context,
                                    const Extension* extension) {
  // No-op if called multiple times.
  ui::IMEBridge::Initialize();
}

void InputImeAPI::OnExtensionUnloaded(content::BrowserContext* browser_context,
                                      const Extension* extension,
                                      UnloadedExtensionInfo::Reason reason) {
  GetInputImeEventRouter(Profile::FromBrowserContext(browser_context))
      ->UnregisterImeExtension(extension->id());
}

void InputImeAPI::OnListenerAdded(const EventListenerInfo& details) {}

InputImeEventRouter::InputImeEventRouter(Profile* profile)
    : InputImeEventRouterBase(profile), active_engine_(nullptr) {}

InputImeEventRouter::~InputImeEventRouter() {
  DeleteInputMethodEngine();
}

bool InputImeEventRouter::RegisterImeExtension(
    const std::string& extension_id) {
  // Check if the IME extension is already registered.
  if (std::find(extension_ids_.begin(), extension_ids_.end(), extension_id) !=
      extension_ids_.end())
    return false;

  extension_ids_.push_back(extension_id);
  return true;
}

void InputImeEventRouter::UnregisterImeExtension(
    const std::string& extension_id) {
  std::vector<std::string>::iterator it =
      std::find(extension_ids_.begin(), extension_ids_.end(), extension_id);
  if (it != extension_ids_.end()) {
    if (GetActiveEngine(extension_id))
      DeleteInputMethodEngine();
    extension_ids_.erase(it);
  }
}

InputMethodEngine* InputImeEventRouter::GetActiveEngine(
    const std::string& extension_id) {
  return (active_engine_ && active_engine_->GetExtensionId() == extension_id)
             ? active_engine_
             : nullptr;
}

void InputImeEventRouter::SetActiveEngine(const std::string& extension_id) {
  if (active_engine_) {
    if (active_engine_->GetExtensionId() == extension_id)
      return;
    DeleteInputMethodEngine();
  }

  RegisterImeExtension(extension_id);
  scoped_ptr<input_method::InputMethodEngine> engine(
      new input_method::InputMethodEngine());
  scoped_ptr<InputMethodEngineBase::Observer> observer(
      new ImeObserverNonChromeOS(extension_id, profile()));
  engine->Initialize(std::move(observer), extension_id.c_str(), profile());
  engine->Enable("");
  active_engine_ = engine.release();
  ui::IMEBridge::Get()->SetCurrentEngineHandler(active_engine_);
}

void InputImeEventRouter::DeleteInputMethodEngine() {
  if (active_engine_) {
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
  event_router->SetActiveEngine(extension_id());
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction InputImeDeactivateFunction::Run() {
  if (!IsInputImeEnabled())
    return RespondNow(Error(kErrorAPIDisabled));

  InputImeEventRouter* event_router =
      GetInputImeEventRouter(Profile::FromBrowserContext(browser_context()));
  event_router->UnregisterImeExtension(extension_id());
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction InputImeCreateWindowFunction::Run() {
  // TODO(shuchen): Implement this API.
  return RespondNow(NoArguments());
}

}  // namespace extensions
