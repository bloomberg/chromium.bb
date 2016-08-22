// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CONTENT_BROWSER_CRASH_DUMP_OBSERVER_ANDROID_H_
#define COMPONENTS_CRASH_CONTENT_BROWSER_CRASH_DUMP_OBSERVER_ANDROID_H_

#include <list>
#include <memory>

#include "base/android/application_status_listener.h"
#include "base/memory/singleton.h"
#include "base/process/kill.h"
#include "base/process/process.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/file_descriptor_info.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/common/process_type.h"

namespace breakpad {

// This class centralises the observation of child processes for the
// purpose of reacting to child process crashes.
class CrashDumpObserver : public content::BrowserChildProcessObserver,
                          public content::NotificationObserver {
 public:
  // CrashDumpObserver client interface.
  // Note: do not access the CrashDumpObserver singleton to add or
  // remove clients from within client callbacks.
  class Client {
   public:
    // Called on the launcher thread.
    virtual void OnChildStart(int child_process_id,
                              content::FileDescriptorInfo* mappings) = 0;
    // Called on the blocking pool.
    virtual void OnChildExit(int child_process_id,
                             base::ProcessHandle pid,
                             content::ProcessType process_type,
                             base::TerminationStatus termination_status,
                             base::android::ApplicationState app_state) = 0;

   protected:
    virtual ~Client() {}
  };

  static CrashDumpObserver* GetInstance();

  void RegisterClient(Client* client);
  void UnregisterClient(Client* client);

  void BrowserChildProcessStarted(int child_process_id,
                                  content::FileDescriptorInfo* mappings);

 private:
  friend struct base::DefaultSingletonTraits<CrashDumpObserver>;

  CrashDumpObserver();
  ~CrashDumpObserver() override;

  // content::BrowserChildProcessObserver implementation:
  void BrowserChildProcessHostDisconnected(
      const content::ChildProcessData& data) override;
  void BrowserChildProcessCrashed(const content::ChildProcessData& data,
                                  int exit_code) override;

  // NotificationObserver implementation:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  void OnChildExitOnBlockingPool(Client* client,
                                 int child_process_id,
                                 base::ProcessHandle pid,
                                 content::ProcessType process_type,
                                 base::TerminationStatus termination_status,
                                 base::android::ApplicationState app_state);

  // Called on child process exit (including crash).
  void OnChildExit(int child_process_id,
                   base::ProcessHandle pid,
                   content::ProcessType process_type,
                   base::TerminationStatus termination_status,
                   base::android::ApplicationState app_state);

  content::NotificationRegistrar notification_registrar_;

  base::Lock registered_clients_lock_;
  std::list<Client*> registered_clients_;

  DISALLOW_COPY_AND_ASSIGN(CrashDumpObserver);
};

}  // namespace breakpad

#endif  // COMPONENTS_CRASH_CONTENT_BROWSER_CRASH_DUMP_OBSERVER_ANDROID_H_
