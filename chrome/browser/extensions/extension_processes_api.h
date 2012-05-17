// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_PROCESSES_API_H__
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_PROCESSES_API_H__
#pragma once

#include <set>
#include <string>

#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host.h"

// Observes the Task Manager and routes the notifications as events to the
// extension system.
class ExtensionProcessesEventRouter : public TaskManagerModelObserver,
                                      public content::NotificationObserver {
 public:
  // Single instance of the event router.
  static ExtensionProcessesEventRouter* GetInstance();

  // Safe to call multiple times.
  void ObserveProfile(Profile* profile);

  // Called when an extension process wants to listen to process events.
  void ListenerAdded();

  // Called when an extension process with a listener exits or removes it.
  void ListenerRemoved();

  // Called on the first invocation of extension API function. This will call
  // out to the Task Manager to start listening for notifications. Returns
  // true if this was the first call and false if this has already been called.
  void StartTaskManagerListening();

  bool is_task_manager_listening() { return task_manager_listening_; }
  int num_listeners() { return listeners_; }

 private:
  friend struct DefaultSingletonTraits<ExtensionProcessesEventRouter>;

  ExtensionProcessesEventRouter();
  virtual ~ExtensionProcessesEventRouter();

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // TaskManagerModelObserver methods.
  virtual void OnItemsAdded(int start, int length) OVERRIDE;
  virtual void OnModelChanged() OVERRIDE {}
  virtual void OnItemsChanged(int start, int length) OVERRIDE;
  virtual void OnItemsRemoved(int start, int length) OVERRIDE {}
  virtual void OnItemsToBeRemoved(int start, int length) OVERRIDE;

  // Internal helpers for processing notifications.
  void ProcessHangEvent(content::RenderWidgetHost* widget);
  void ProcessClosedEvent(
      content::RenderProcessHost* rph,
      content::RenderProcessHost::RendererClosedDetails* details);

  void NotifyProfiles(const char* event_name, std::string json_args);

  void DispatchEvent(Profile* profile,
                     const char* event_name,
                     const std::string& json_args);

  // Determines whether there is a registered listener for the specified event.
  // It helps to avoid collecing data if no one is interested in it.
  bool HasEventListeners(std::string& event_name);

  // Used for tracking registrations to process related notifications.
  content::NotificationRegistrar registrar_;

  // Registered profiles.
  typedef std::set<Profile*> ProfileSet;
  ProfileSet profiles_;

  // TaskManager to observe for updates.
  TaskManagerModel* model_;

  // Count of listeners, so we avoid sending updates if no one is interested.
  int listeners_;

  // Indicator whether we've initialized the Task Manager listeners. This is
  // done once for the lifetime of this object.
  bool task_manager_listening_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionProcessesEventRouter);
};


// This extension function returns the Process object for the renderer process
// currently in use by the specified Tab.
class GetProcessIdForTabFunction : public AsyncExtensionFunction,
                                   public content::NotificationObserver {
 private:
  virtual ~GetProcessIdForTabFunction() {}
  virtual bool RunImpl() OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  void GetProcessIdForTab();

  content::NotificationRegistrar registrar_;

  // Storage for the tab ID parameter.
  int tab_id_;

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.processes.getProcessIdForTab")
};

// Extension function that allows terminating Chrome subprocesses, by supplying
// the unique ID for the process coming from the ChildProcess ID pool.
// Using unique IDs instead of OS process IDs allows two advantages:
// * guaranteed uniqueness, since OS process IDs can be reused
// * guards against killing non-Chrome processes
class TerminateFunction : public AsyncExtensionFunction,
                          public content::NotificationObserver {
 private:
  virtual ~TerminateFunction() {}
  virtual bool RunImpl() OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  void TerminateProcess();

  content::NotificationRegistrar registrar_;

  // Storage for the process ID parameter.
  int process_id_;

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.processes.terminate")
};

// Extension function which returns a set of Process objects, containing the
// details corresponding to the process IDs supplied as input.
class GetProcessInfoFunction : public AsyncExtensionFunction,
                               public content::NotificationObserver {
 public:
  GetProcessInfoFunction();

 private:
  virtual ~GetProcessInfoFunction();
  virtual bool RunImpl() OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  void GatherProcessInfo();

  content::NotificationRegistrar registrar_;

  // Member variables to store the function parameters
  std::vector<int> process_ids_;
  bool memory_;

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.processes.getProcessInfo")
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_PROCESSES_API_H__
