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
#include "chrome/browser/chromeos/input_method/input_method_whitelist.h"
#include "chromeos/dbus/ibus/ibus_panel_service.h"

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
                           public IBusPanelPropertyHandlerInterface {
 public:
  IBusControllerImpl();
  virtual ~IBusControllerImpl();

  // IBusController overrides:
  virtual bool Start() OVERRIDE;
  virtual void Reset() OVERRIDE;
  virtual bool Stop() OVERRIDE;
  virtual bool ChangeInputMethod(const std::string& id) OVERRIDE;
  virtual bool ActivateInputMethodProperty(const std::string& key) OVERRIDE;

  // Calls <anonymous_namespace>::FindAndUpdateProperty. This method is just for
  // unit testing.
  static bool FindAndUpdatePropertyForTesting(
      const InputMethodProperty& new_prop,
      InputMethodPropertyList* prop_list);

  void IBusDaemonInitializationDone(const std::string& ibus_address);

 private:
  enum IBusDaemonStatus{
    IBUS_DAEMON_INITIALIZING,
    IBUS_DAEMON_RUNNING,
    IBUS_DAEMON_SHUTTING_DOWN,
    IBUS_DAEMON_STOP,
  };

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

  // Adds address file watcher in FILE thread and then calls LaunchIBusDaemon.
  bool StartIBusDaemon();

  // Starts ibus-daemon.
  void LaunchIBusDaemon(const std::string& ibus_address);

  // Launches an input method procsess specified by the given command
  // line. On success, returns true and stores the process handle in
  // |process_handle|. Otherwise, returns false, and the contents of
  // |process_handle| is untouched.
  bool LaunchProcess(const std::string& command_line,
                     base::ProcessHandle* process_handle);

  // Returns pointer to InputMethod object.
  ui::InputMethodIBus* GetInputMethod();

  // Injects an alternative ui::InputMethod for testing.
  // The injected object must be released by caller.
  void set_input_method_for_testing(ui::InputMethodIBus* input_method);

  // Called when the IBusConfigClient is initialized.
  void OnIBusConfigClientInitialized();

  // Called when the input method process is shut down.
  void OnIBusDaemonDisconnected(base::ProcessId pid);

  // The current ibus_daemon address. This value is assigned at the launching
  // ibus-daemon and used in bus connection initialization.
  std::string ibus_daemon_address_;

  // The process handle of the IBus daemon. kNullProcessHandle if it's not
  // running.
  base::ProcessHandle process_handle_;

  // Current input context path.
  std::string current_input_context_path_;

  // The input method ID which is currently selected. The ID is sent to the
  // daemon when |ibus_| and |ibus_config_| connections are both established.
  std::string current_input_method_id_;

  // An object which knows all valid input methods and layout IDs.
  InputMethodWhitelist whitelist_;

  // Represents ibus-daemon's status.
  IBusDaemonStatus ibus_daemon_status_;

  // The pointer to global input method. We can inject this value for testing.
  ui::InputMethodIBus* input_method_;

  // IBusControllerImpl should be used only on UI thread.
  base::ThreadChecker thread_checker_;

  // Used for making callbacks for PostTask.
  base::WeakPtrFactory<IBusControllerImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(IBusControllerImpl);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_IBUS_CONTROLLER_IMPL_H_
