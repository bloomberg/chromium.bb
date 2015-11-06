// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_INPUT_IME_INPUT_IME_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_INPUT_IME_INPUT_IME_API_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/singleton.h"
#include "base/scoped_observer.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension.h"
#include "ui/base/ime/ime_engine_handler_interface.h"
#include "ui/base/ime/ime_engine_observer.h"

class Profile;

namespace ui {
class IMEEngineHandlerInterface;
class IMEEngineObserver;
}  // namespace ui

namespace extensions {
class ExtensionRegistry;
struct InputComponentInfo;

class InputImeEventRouter {
 public:
  explicit InputImeEventRouter(Profile* profile);
  ~InputImeEventRouter();

  bool RegisterImeExtension(
      const std::string& extension_id,
      const std::vector<extensions::InputComponentInfo>& input_components);
  void UnregisterAllImes(const std::string& extension_id);

  ui::IMEEngineHandlerInterface* GetEngine(const std::string& extension_id,
                                           const std::string& component_id);
  ui::IMEEngineHandlerInterface* GetActiveEngine(
      const std::string& extension_id);

  // Called when a key event was handled.
  void OnKeyEventHandled(const std::string& extension_id,
                         const std::string& request_id,
                         bool handled);

  std::string AddRequest(
      const std::string& component_id,
      ui::IMEEngineHandlerInterface::KeyEventDoneCallback& key_data);

 private:
  typedef std::map<
      std::string,
      std::pair<std::string,
                ui::IMEEngineHandlerInterface::KeyEventDoneCallback>>
      RequestMap;

  // The engine map from extension_id to an engine.
  std::map<std::string, ui::IMEEngineHandlerInterface*> engine_map_;

  unsigned int next_request_id_;
  RequestMap request_map_;
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(InputImeEventRouter);
};

class InputImeEventRouterFactory {
 public:
  static InputImeEventRouterFactory* GetInstance();
  InputImeEventRouter* GetRouter(Profile* profile);

 private:
  friend struct base::DefaultSingletonTraits<InputImeEventRouterFactory>;
  InputImeEventRouterFactory();
  ~InputImeEventRouterFactory();

  std::map<Profile*, InputImeEventRouter*, ProfileCompare> router_map_;

  DISALLOW_COPY_AND_ASSIGN(InputImeEventRouterFactory);
};

class InputImeSetCompositionFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.setComposition",
                             INPUT_IME_SETCOMPOSITION)

 protected:
  ~InputImeSetCompositionFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class InputImeClearCompositionFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.clearComposition",
                             INPUT_IME_CLEARCOMPOSITION)

 protected:
  ~InputImeClearCompositionFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class InputImeCommitTextFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.commitText", INPUT_IME_COMMITTEXT)

 protected:
  ~InputImeCommitTextFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class InputImeSetCandidateWindowPropertiesFunction
    : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.setCandidateWindowProperties",
                             INPUT_IME_SETCANDIDATEWINDOWPROPERTIES)

 protected:
  ~InputImeSetCandidateWindowPropertiesFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class InputImeSetCandidatesFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.setCandidates", INPUT_IME_SETCANDIDATES)

 protected:
  ~InputImeSetCandidatesFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class InputImeSetCursorPositionFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.setCursorPosition",
                             INPUT_IME_SETCURSORPOSITION)

 protected:
  ~InputImeSetCursorPositionFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class InputImeSetMenuItemsFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.setMenuItems", INPUT_IME_SETMENUITEMS)

 protected:
  ~InputImeSetMenuItemsFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class InputImeUpdateMenuItemsFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.updateMenuItems",
                             INPUT_IME_UPDATEMENUITEMS)

 protected:
  ~InputImeUpdateMenuItemsFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class InputImeDeleteSurroundingTextFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.deleteSurroundingText",
                             INPUT_IME_DELETESURROUNDINGTEXT)
 protected:
  ~InputImeDeleteSurroundingTextFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class InputImeKeyEventHandledFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.keyEventHandled",
                             INPUT_IME_KEYEVENTHANDLED)

 protected:
  ~InputImeKeyEventHandledFunction() override {}

  // ExtensionFunction:
  bool RunAsync() override;
};

class InputImeSendKeyEventsFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.sendKeyEvents",
                             INPUT_IME_SENDKEYEVENTS)

 protected:
  ~InputImeSendKeyEventsFunction() override {}

  // ExtensionFunction:
  bool RunAsync() override;
};

class InputImeHideInputViewFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.hideInputView",
                             INPUT_IME_HIDEINPUTVIEW)

 protected:
  ~InputImeHideInputViewFunction() override {}

  // ExtensionFunction:
  bool RunAsync() override;
};

class InputImeAPI : public BrowserContextKeyedAPI,
                    public ExtensionRegistryObserver,
                    public EventRouter::Observer {
 public:
  explicit InputImeAPI(content::BrowserContext* context);
  ~InputImeAPI() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<InputImeAPI>* GetFactoryInstance();

  // ExtensionRegistryObserver implementation.
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionInfo::Reason reason) override;

  // EventRouter::Observer implementation.
  void OnListenerAdded(const EventListenerInfo& details) override;

 private:
  friend class BrowserContextKeyedAPIFactory<InputImeAPI>;
  InputImeEventRouter* input_ime_event_router();

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() {
    return "InputImeAPI";
  }
  static const bool kServiceIsNULLWhileTesting = true;

  content::BrowserContext* const browser_context_;

  // Listen to extension load, unloaded notifications.
  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_INPUT_IME_INPUT_IME_API_H_
