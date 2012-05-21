// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_LAZY_BACKGROUND_TASK_QUEUE_H_
#define CHROME_BROWSER_EXTENSIONS_LAZY_BACKGROUND_TASK_QUEUE_H_
#pragma once

#include <map>
#include <set>
#include <string>

#include "base/compiler_specific.h"
#include "base/callback_forward.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class ExtensionHost;
class Profile;

namespace extensions {

class Extension;

// This class maintains a queue of tasks that should execute when an
// extension's lazy background page is loaded. It is also in charge of loading
// the page when the first task is queued.
//
// It is the consumer's responsibility to use this class when appropriate, i.e.
// only with extensions that have not-yet-loaded lazy background pages.
class LazyBackgroundTaskQueue
    : public content::NotificationObserver,
      public base::SupportsWeakPtr<LazyBackgroundTaskQueue> {
 public:
  typedef base::Callback<void(ExtensionHost*)> PendingTask;

  explicit LazyBackgroundTaskQueue(Profile* profile);
  virtual ~LazyBackgroundTaskQueue();

  // Returns true if the task should be added to the queue (that is, if the
  // extension has a lazy background page that isn't ready yet).
  bool ShouldEnqueueTask(Profile* profile, const Extension* extension);

  // Adds a task to the queue for a given extension. If this is the first
  // task added for the extension, its lazy background page will be loaded.
  // The task will be called either when the page is loaded, or when the
  // page fails to load for some reason (e.g. a crash). In the latter case,
  // the ExtensionHost parameter is NULL.
  void AddPendingTask(
      Profile* profile,
      const std::string& extension_id,
      const PendingTask& task);

 private:
  // A map between an extension_id,Profile pair and the queue of tasks pending
  // the load of its background page.
  typedef std::string ExtensionID;
  typedef std::pair<Profile*, ExtensionID> PendingTasksKey;
  typedef std::vector<PendingTask> PendingTasksList;
  typedef std::map<PendingTasksKey,
                   linked_ptr<PendingTasksList> > PendingTasksMap;
  typedef std::set<PendingTasksKey> PendingPageLoadList;

  void StartLazyBackgroundPage(Profile* profile,
                               const std::string& extension_id);

  // content::NotificationObserver interface.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Called when a lazy background page has finished loading, or has failed to
  // load (host is NULL in that case). All enqueued tasks are run in order.
  void ProcessPendingTasks(
      ExtensionHost* host,
      Profile* profile,
      const Extension* extension);

  Profile* profile_;
  content::NotificationRegistrar registrar_;
  PendingTasksMap pending_tasks_;
  PendingPageLoadList pending_page_loads_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_LAZY_BACKGROUND_TASK_QUEUE_H_
