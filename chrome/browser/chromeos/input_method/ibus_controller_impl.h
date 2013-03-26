// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_IBUS_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_IBUS_CONTROLLER_IMPL_H_

#include <string>
#include <vector>

#include "base/process_util.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/chromeos/input_method/ibus_controller_base.h"
#include "chromeos/dbus/ibus/ibus_panel_service.h"
#include "chromeos/ime/ibus_daemon_controller.h"
#include "chromeos/ime/input_method_whitelist.h"

namespace ui {
class InputMethodIBus;
}  // namespace ui

namespace chromeos {
namespace input_method {

struct InputMethodConfigValue;
struct InputMethodProperty;
typedef std::vector<InputMethodProperty> InputMethodPropertyList;

// The IBusController implementation.
class IBusControllerImpl : public IBusControllerBase,
                           public IBusPanelPropertyHandlerInterface,
                           public IBusDaemonController::Observer {
 public:
  IBusControllerImpl();
  virtual ~IBusControllerImpl();

  // IBusController overrides:
  virtual bool ChangeInputMethod(const std::string& id) OVERRIDE;
  virtual bool ActivateInputMethodProperty(const std::string& key) OVERRIDE;

  // Calls <anonymous_namespace>::FindAndUpdateProperty. This method is just for
  // unit testing.
  static bool FindAndUpdatePropertyForTesting(
      const InputMethodProperty& new_prop,
      InputMethodPropertyList* prop_list);

 private:
  // IBusDaemonController overrides:
  virtual void OnConnected() OVERRIDE;
  virtual void OnDisconnected() OVERRIDE;

  // IBusControllerBase overrides:
  virtual bool SetInputMethodConfigInternal(
      const ConfigKeyType& key,
      const InputMethodConfigValue& value) OVERRIDE;

  // IBusPanelPropertyHandlerInterface overrides:
  virtual void RegisterProperties(
      const IBusPropertyList& properties) OVERRIDE;
  virtual void UpdateProperty(const IBusProperty& property) OVERRIDE;

  // Checks if |ibus_| and |ibus_config_| connections are alive.
  bool IBusConnectionsAreAlive();

  // Just calls ibus_bus_set_global_engine_async() with the |id|.
  void SendChangeInputMethodRequest(const std::string& id);

  // Called when the IBusConfigClient is initialized.
  void OnIBusConfigClientInitialized();

  // Current input context path.
  std::string current_input_context_path_;

  // The input method ID which is currently selected. The ID is sent to the
  // daemon when |ibus_| and |ibus_config_| connections are both established.
  std::string current_input_method_id_;

  // An object which knows all valid input methods and layout IDs.
  InputMethodWhitelist whitelist_;

  // IBusControllerImpl should be used only on UI thread.
  base::ThreadChecker thread_checker_;

  // Used for making callbacks for PostTask.
  base::WeakPtrFactory<IBusControllerImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(IBusControllerImpl);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_IBUS_CONTROLLER_IMPL_H_
