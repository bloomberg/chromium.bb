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
#include "chrome/browser/chromeos/input_method/input_method_engine_interface.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension.h"

class Profile;

namespace chromeos {
class InputMethodEngineInterface;
class ImeObserver;
}  // namespace chromeos

namespace extensions {
class ExtensionRegistry;
struct InputComponentInfo;

class InputImeEventRouter {
 public:
  static InputImeEventRouter* GetInstance();

  bool RegisterImeExtension(
      const std::string& extension_id,
      const std::vector<extensions::InputComponentInfo>& input_components);
  void UnregisterAllImes(const std::string& extension_id);

  chromeos::InputMethodEngineInterface* GetEngine(
      const std::string& extension_id,
      const std::string& component_id);
  chromeos::InputMethodEngineInterface* GetActiveEngine(
      const std::string& extension_id);


  // Called when a key event was handled.
  void OnKeyEventHandled(const std::string& extension_id,
                         const std::string& request_id,
                         bool handled);

  std::string AddRequest(const std::string& component_id,
                         chromeos::input_method::KeyEventHandle* key_data);

 private:
  friend struct DefaultSingletonTraits<InputImeEventRouter>;
  typedef std::map<std::string, std::pair<std::string,
          chromeos::input_method::KeyEventHandle*> > RequestMap;

  InputImeEventRouter();
  ~InputImeEventRouter();

  // The engine map from extension_id to an engine.
  std::map<std::string, chromeos::InputMethodEngineInterface*> engine_map_;

  unsigned int next_request_id_;
  RequestMap request_map_;

  DISALLOW_COPY_AND_ASSIGN(InputImeEventRouter);
};

class InputImeSetCompositionFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.setComposition",
                             INPUT_IME_SETCOMPOSITION)

 protected:
  virtual ~InputImeSetCompositionFunction() {}

  // ExtensionFunction:
  virtual bool RunSync() OVERRIDE;
};

class InputImeClearCompositionFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.clearComposition",
                             INPUT_IME_CLEARCOMPOSITION)

 protected:
  virtual ~InputImeClearCompositionFunction() {}

  // ExtensionFunction:
  virtual bool RunSync() OVERRIDE;
};

class InputImeCommitTextFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.commitText", INPUT_IME_COMMITTEXT)

 protected:
  virtual ~InputImeCommitTextFunction() {}

  // ExtensionFunction:
  virtual bool RunSync() OVERRIDE;
};

class InputImeSetCandidateWindowPropertiesFunction
    : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.setCandidateWindowProperties",
                             INPUT_IME_SETCANDIDATEWINDOWPROPERTIES)

 protected:
  virtual ~InputImeSetCandidateWindowPropertiesFunction() {}

  // ExtensionFunction:
  virtual bool RunSync() OVERRIDE;
};

class InputImeSetCandidatesFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.setCandidates", INPUT_IME_SETCANDIDATES)

 protected:
  virtual ~InputImeSetCandidatesFunction() {}

  // ExtensionFunction:
  virtual bool RunSync() OVERRIDE;
};

class InputImeSetCursorPositionFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.setCursorPosition",
                             INPUT_IME_SETCURSORPOSITION)

 protected:
  virtual ~InputImeSetCursorPositionFunction() {}

  // ExtensionFunction:
  virtual bool RunSync() OVERRIDE;
};

class InputImeSetMenuItemsFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.setMenuItems", INPUT_IME_SETMENUITEMS)

 protected:
  virtual ~InputImeSetMenuItemsFunction() {}

  // ExtensionFunction:
  virtual bool RunSync() OVERRIDE;
};

class InputImeUpdateMenuItemsFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.updateMenuItems",
                             INPUT_IME_UPDATEMENUITEMS)

 protected:
  virtual ~InputImeUpdateMenuItemsFunction() {}

  // ExtensionFunction:
  virtual bool RunSync() OVERRIDE;
};

class InputImeDeleteSurroundingTextFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.deleteSurroundingText",
                             INPUT_IME_DELETESURROUNDINGTEXT)
 protected:
  virtual ~InputImeDeleteSurroundingTextFunction() {}

  // ExtensionFunction:
  virtual bool RunSync() OVERRIDE;
};

class InputImeKeyEventHandledFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.keyEventHandled",
                             INPUT_IME_KEYEVENTHANDLED)

 protected:
  virtual ~InputImeKeyEventHandledFunction() {}

  // ExtensionFunction:
  virtual bool RunAsync() OVERRIDE;
};

class InputImeSendKeyEventsFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.sendKeyEvents",
                             INPUT_IME_SENDKEYEVENTS)

 protected:
  virtual ~InputImeSendKeyEventsFunction() {}

  // ExtensionFunction:
  virtual bool RunAsync() OVERRIDE;
};

class InputImeHideInputViewFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.hideInputView",
                             INPUT_IME_HIDEINPUTVIEW)

 protected:
  virtual ~InputImeHideInputViewFunction() {}

  // ExtensionFunction:
  virtual bool RunAsync() OVERRIDE;
};

class InputImeAPI : public BrowserContextKeyedAPI,
                    public ExtensionRegistryObserver,
                    public EventRouter::Observer {
 public:
  explicit InputImeAPI(content::BrowserContext* context);
  virtual ~InputImeAPI();

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<InputImeAPI>* GetFactoryInstance();

  // ExtensionRegistryObserver implementation.
  virtual void OnExtensionLoaded(content::BrowserContext* browser_context,
                                 const Extension* extension) OVERRIDE;
  virtual void OnExtensionUnloaded(
      content::BrowserContext* browser_context,
      const Extension* extension,
      UnloadedExtensionInfo::Reason reason) OVERRIDE;

  // EventRouter::Observer implementation.
  virtual void OnListenerAdded(const EventListenerInfo& details) OVERRIDE;

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
