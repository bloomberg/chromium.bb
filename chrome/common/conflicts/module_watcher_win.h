// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CONFLICTS_MODULE_WATCHER_WIN_H_
#define CHROME_COMMON_CONFLICTS_MODULE_WATCHER_WIN_H_

#include <memory>

#include "base/callback.h"
#include "chrome/common/conflicts/module_event_win.mojom.h"

class ModuleWatcherTest;

union LDR_DLL_NOTIFICATION_DATA;

// This class observes modules as they are loaded and unloaded into a process's
// address space.
//
// This class is safe to be created on any thread. Similarly, it is safe to be
// destroyed on any thread, independent of the thread on which the instance was
// created.
class ModuleWatcher {
 public:
  // The type of callback that will be invoked for each module event. This is
  // invoked by the loader and potentially on any thread. The loader lock is not
  // held but the execution of this callback blocks the module from being bound.
  // Keep the amount of work performed here to an absolute minimum. Note that
  // it is possible for this callback to be invoked after the destruction of the
  // watcher, but very unlikely.
  using OnModuleEventCallback =
      base::Callback<void(const mojom::ModuleEvent& event)>;

  // Creates and starts a watcher. This enumerates all loaded modules
  // synchronously on the current thread during construction, and provides
  // synchronous notifications as modules are loaded and unloaded. The callback
  // is invoked in the context of the thread that is loading a module, and as
  // such may be invoked on any thread in the process. Note that it is possible
  // to receive two notifications for some modules as the initial loaded module
  // enumeration races briefly with the callback mechanism. In this case both a
  // MODULE_LOADED and a MODULE_ALREADY_LOADED event will be received for the
  // same module. Since the callback is installed first no modules can be
  // missed, however. This factory function may be called on any thread.
  //
  // Only a single instance of a watcher may exist at any moment. This will
  // return nullptr when trying to create a second watcher.
  static std::unique_ptr<ModuleWatcher> Create(
      const OnModuleEventCallback& callback);

  // This can be called on any thread. After destruction the |callback|
  // provided to the constructor will no longer be invoked with module events.
  ~ModuleWatcher();

 protected:
  // For unittesting.
  friend class ModuleWatcherTest;

  // Registers a DllNotification callback with the OS. Modifies
  // |dll_notification_cookie_|. Can be called on any thread.
  void RegisterDllNotificationCallback();

  // Removes the installed DllNotification callback. Modifies
  // |dll_notification_cookie_|. Can be called on any thread.
  void UnregisterDllNotificationCallback();

  // Enumerates all currently loaded modules, synchronously invoking callbacks
  // on the current thread. Can be called on any thread.
  void EnumerateAlreadyLoadedModules();

  // Helper function for retrieving the callback associated with a given
  // LdrNotification context.
  static OnModuleEventCallback GetCallbackForContext(void* context);

  // The loader notification callback. This is actually
  // void CALLBACK LoaderNotificationCallback(
  //     DWORD, const LDR_DLL_NOTIFICATION_DATA*, PVOID)
  // Not using CALLBACK/DWORD/PVOID allows skipping the windows.h header from
  // this file.
  static void __stdcall LoaderNotificationCallback(
      unsigned long notification_reason,
      const LDR_DLL_NOTIFICATION_DATA* notification_data,
      void* context);

 private:
  // Private to enforce Singleton semantics. See Create above.
  explicit ModuleWatcher(const OnModuleEventCallback& callback);

  // The current callback. Can end up being invoked on any thread.
  OnModuleEventCallback callback_;
  // Used by the DllNotification mechanism.
  void* dll_notification_cookie_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ModuleWatcher);
};

#endif  // CHROME_COMMON_CONFLICTS_MODULE_WATCHER_WIN_H_
