// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_IBUS_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_IBUS_CONTROLLER_IMPL_H_
#pragma once

#include <gio/gio.h>  // GAsyncResult and related types.
#include <glib-object.h>

#include <string>
#include <vector>

#include "base/process_util.h"
#include "chrome/browser/chromeos/input_method/ibus_controller_base.h"
#include "chrome/browser/chromeos/input_method/input_method_whitelist.h"
#include "ui/base/glib/glib_signal.h"

// Do not #include ibus.h here. That makes it impossible to compile unit tests
// for the class.
struct _IBusBus;
struct _IBusConfig;
struct _IBusPanelService;
struct _IBusPropList;
struct _IBusProperty;
typedef struct _IBusBus IBusBus;
typedef struct _IBusConfig IBusConfig;
typedef struct _IBusPanelService IBusPanelService;
typedef struct _IBusPropList IBusPropList;
typedef struct _IBusProperty IBusProperty;

namespace chromeos {
namespace input_method {

struct InputMethodConfigValue;
struct InputMethodProperty;
typedef std::vector<InputMethodProperty> InputMethodPropertyList;

// The IBusController implementation.
class IBusControllerImpl : public IBusControllerBase {
 public:
  IBusControllerImpl();
  virtual ~IBusControllerImpl();

  // IBusController overrides:
  virtual bool Start() OVERRIDE;
  virtual void Reset() OVERRIDE;
  virtual bool Stop() OVERRIDE;
  virtual bool ChangeInputMethod(const std::string& id) OVERRIDE;
  virtual bool ActivateInputMethodProperty(const std::string& key) OVERRIDE;
#if defined(USE_VIRTUAL_KEYBOARD)
  virtual void SendHandwritingStroke(const HandwritingStroke& stroke) OVERRIDE;
  virtual void CancelHandwriting(int n_strokes) OVERRIDE;
#endif

  // Calls <anonymous_namespace>::FindAndUpdateProperty. This method is just for
  // unit testing.
  static bool FindAndUpdatePropertyForTesting(
      const InputMethodProperty& new_prop,
      InputMethodPropertyList* prop_list);

  static void IBusDaemonInitializationDone(IBusControllerImpl* controller,
                                           const std::string& ibus_address);

 private:
  enum IBusDaemonStatus{
    IBUS_DAEMON_INITIALIZING,
    IBUS_DAEMON_RUNNING,
    IBUS_DAEMON_SHUTTING_DOWN,
    IBUS_DAEMON_STOP,
  };

  // Functions that end with Thunk are used to deal with glib callbacks.
  CHROMEG_CALLBACK_0(IBusControllerImpl, void, BusConnected, IBusBus*);
  CHROMEG_CALLBACK_0(IBusControllerImpl, void, BusDisconnected, IBusBus*);
  CHROMEG_CALLBACK_3(IBusControllerImpl, void, BusNameOwnerChanged,
                     IBusBus*, const gchar*, const gchar*, const gchar*);
  CHROMEG_CALLBACK_1(IBusControllerImpl, void, FocusIn,
                     IBusPanelService*, const gchar*);
  CHROMEG_CALLBACK_1(IBusControllerImpl, void, RegisterProperties,
                     IBusPanelService*, IBusPropList*);
  CHROMEG_CALLBACK_1(IBusControllerImpl, void, UpdateProperty,
                     IBusPanelService*, IBusProperty*);

  // IBusControllerBase overrides:
  virtual bool SetInputMethodConfigInternal(
      const ConfigKeyType& key,
      const InputMethodConfigValue& value) OVERRIDE;

  // Checks if |ibus_| and |ibus_config_| connections are alive.
  bool IBusConnectionsAreAlive();

  // Restores connections to ibus-daemon and ibus-memconf if they are not ready.
  void MaybeRestoreConnections();

  // Initializes IBusBus object if it's not yet done.
  void MaybeInitializeIBusBus();

  // Creates IBusConfig object if it's not created yet AND |ibus_| connection
  // is ready.
  void MaybeRestoreIBusConfig();

  // Destroys IBusConfig object if |ibus_| connection is not ready. This
  // function does nothing if |ibus_config_| is NULL or |ibus_| connection is
  // still alive. Note that the IBusConfig object can't be used when |ibus_|
  // connection is not ready.
  void MaybeDestroyIBusConfig();

  // Just calls ibus_bus_set_global_engine_async() with the |id|.
  void SendChangeInputMethodRequest(const std::string& id);

  // Calls SetInputMethodConfigInternal() for each |current_config_values_|.
  void SendAllInputMethodConfigs();

  // Starts listening to the "connected", "disconnected", and
  // "name-owner-changed" D-Bus signals from ibus-daemon.
  void ConnectBusSignals();

  // Starts listening to the "focus-in", "register-properties", and
  // "update-property" D-Bus signals from ibus-daemon.
  void ConnectPanelServiceSignals();

  // Adds address file watcher in FILE thread and then calls LaunchIBusDaemon.
  bool StartIBusDaemon();

  // Starts ibus-daemon.
  void LaunchIBusDaemon(const std::string& ibus_address);

  // Launches an input method procsess specified by the given command
  // line. On success, returns true and stores the process handle in
  // |process_handle|. Otherwise, returns false, and the contents of
  // |process_handle| is untouched. |watch_func| will be called when the
  // process terminates.
  bool LaunchProcess(const std::string& command_line,
                     base::ProcessHandle* process_handle,
                     GChildWatchFunc watch_func);

  // A callback function that will be called when ibus_config_set_value_async()
  // request is finished.
  static void SetInputMethodConfigCallback(GObject* source_object,
                                           GAsyncResult* res,
                                           gpointer user_data);

  // Called when the input method process is shut down.
  static void OnIBusDaemonExit(GPid pid,
                               gint status,
                               IBusControllerImpl* controller);

  // Connection to the ibus-daemon via IBus API. These objects are used to
  // call ibus-daemon's API (e.g. activate input methods, set config, ...)
  IBusBus* ibus_;
  IBusConfig* ibus_config_;

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

  // Used for making callbacks for PostTask.
  base::WeakPtrFactory<IBusControllerImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(IBusControllerImpl);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_IBUS_CONTROLLER_IMPL_H_
