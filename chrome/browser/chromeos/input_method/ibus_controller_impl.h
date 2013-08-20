// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_IBUS_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_IBUS_CONTROLLER_IMPL_H_

#include <string>
#include <vector>

#include "base/threading/thread_checker.h"
#include "chrome/browser/chromeos/input_method/ibus_controller_base.h"
#include "chromeos/dbus/ibus/ibus_panel_service.h"
#include "chromeos/ime/ibus_bridge.h"
#include "chromeos/ime/ibus_daemon_controller.h"

namespace ui {
class InputMethodIBus;
}  // namespace ui

namespace chromeos {
namespace input_method {

struct InputMethodConfigValue;
struct InputMethodProperty;
typedef std::vector<InputMethodProperty> InputMethodPropertyList;

// The IBusController implementation.
// TODO(nona): Merge to IBusControllerBase, there is no longer reason to split
//             this class into Impl and Base.
class IBusControllerImpl : public IBusControllerBase,
                           public IBusPanelPropertyHandlerInterface,
                           public IBusDaemonController::Observer {
 public:
  IBusControllerImpl();
  virtual ~IBusControllerImpl();

  // IBusController overrides:
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

  // Called when the IBusConfigClient is initialized.
  void OnIBusConfigClientInitialized();

  // Current input context path.
  std::string current_input_context_path_;

  // IBusControllerImpl should be used only on UI thread.
  base::ThreadChecker thread_checker_;

  // Used for making callbacks for PostTask.
  base::WeakPtrFactory<IBusControllerImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(IBusControllerImpl);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_IBUS_CONTROLLER_IMPL_H_
