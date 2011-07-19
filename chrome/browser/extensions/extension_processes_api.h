// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_PROCESSES_API_H__
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_PROCESSES_API_H__
#pragma once

#include <set>
#include <string>

#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "content/common/notification_registrar.h"

// Observes the Task Manager and routes the notifications as events to the
// extension system.
class ExtensionProcessesEventRouter : public TaskManagerModelObserver {
 public:
  // Single instance of the event router.
  static ExtensionProcessesEventRouter* GetInstance();

  // Safe to call multiple times.
  void ObserveProfile(Profile* profile);

  // Called when an extension process wants to listen to process events.
  void ListenerAdded();

  // Called when an extension process with a listener exits or removes it.
  void ListenerRemoved();

 private:
  friend struct DefaultSingletonTraits<ExtensionProcessesEventRouter>;

  ExtensionProcessesEventRouter();
  virtual ~ExtensionProcessesEventRouter();

  // TaskManagerModelObserver methods.
  virtual void OnModelChanged() {}
  virtual void OnItemsChanged(int start, int length);
  virtual void OnItemsAdded(int start, int length) {}
  virtual void OnItemsRemoved(int start, int length) {}

  void DispatchEvent(Profile* profile,
                     const char* event_name,
                     const std::string& json_args);

  // Used for tracking registrations to process related notifications.
  NotificationRegistrar registrar_;

  // Registered profiles.
  typedef std::set<Profile*> ProfileSet;
  ProfileSet profiles_;

  // TaskManager to observe for updates.
  TaskManagerModel* model_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionProcessesEventRouter);
};


// This extension function returns the Process object for the renderer process
// currently in use by the specified Tab.
class GetProcessIdForTabFunction : public SyncExtensionFunction {
  virtual ~GetProcessIdForTabFunction() {}
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.processes.getProcessIdForTab")
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_PROCESSES_API_H__
