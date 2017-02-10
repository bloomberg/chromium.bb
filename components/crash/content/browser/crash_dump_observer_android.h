// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CONTENT_BROWSER_CRASH_DUMP_OBSERVER_ANDROID_H_
#define COMPONENTS_CRASH_CONTENT_BROWSER_CRASH_DUMP_OBSERVER_ANDROID_H_

#include <memory>
#include <vector>

#include "base/android/application_status_listener.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
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
// The CrashDumpObserver instance exists on the browser main thread.
class CrashDumpObserver : public content::BrowserChildProcessObserver,
                          public content::NotificationObserver {
 public:
  // CrashDumpObserver client interface.
  // Client methods will be called synchronously in the order in which
  // clients were registered. It is the implementer's responsibility
  // to post tasks to the appropriate threads if required (and be
  // aware that this may break ordering guarantees).
  class Client {
   public:
    // OnChildStart is called on the launcher thread.
    virtual void OnChildStart(int child_process_id,
                              content::FileDescriptorInfo* mappings) = 0;
    // OnChildExit is called on the UI thread.
    // OnChildExit may be called twice (once for the child process
    // termination, and once for the IPC channel disconnection).
    virtual void OnChildExit(int child_process_id,
                             base::ProcessHandle pid,
                             content::ProcessType process_type,
                             base::TerminationStatus termination_status,
                             base::android::ApplicationState app_state) = 0;

    virtual ~Client() {}
  };

  // The global CrashDumpObserver instance is created by calling
  // Create (on the UI thread), and lives until process exit. Tests
  // making use of this class should register an AtExitManager.
  static void Create();

  // Fetch a pointer to the global CrashDumpObserver instance. The
  // global instance must have been created by the time GetInstance is
  // called.
  static CrashDumpObserver* GetInstance();

  void RegisterClient(std::unique_ptr<Client> client);

  // BrowserChildProcessStarted must be called from
  // ContentBrowserClient::GetAdditionalMappedFilesForChildProcess
  // overrides, to notify the CrashDumpObserver of child process
  // creation, and to allow clients to register any fd mappings they
  // need.
  void BrowserChildProcessStarted(int child_process_id,
                                  content::FileDescriptorInfo* mappings);

 private:
  friend struct base::DefaultLazyInstanceTraits<CrashDumpObserver>;

  CrashDumpObserver();
  ~CrashDumpObserver() override;

  // content::BrowserChildProcessObserver implementation:
  void BrowserChildProcessHostDisconnected(
      const content::ChildProcessData& data) override;
  void BrowserChildProcessCrashed(const content::ChildProcessData& data,
                                  int exit_code) override;
  // On Android we will never observe BrowserChildProcessCrashed
  // because we do not receive exit codes from zygote spawned
  // processes.

  // NotificationObserver implementation:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Called on child process exit (including crash).
  void OnChildExit(int child_process_id,
                   base::ProcessHandle pid,
                   content::ProcessType process_type,
                   base::TerminationStatus termination_status,
                   base::android::ApplicationState app_state);

  content::NotificationRegistrar notification_registrar_;

  base::Lock registered_clients_lock_;
  std::vector<std::unique_ptr<Client>> registered_clients_;

  // child_process_id to process id.
  std::map<int, base::ProcessHandle> child_process_id_to_pid_;

  DISALLOW_COPY_AND_ASSIGN(CrashDumpObserver);
};

}  // namespace breakpad

#endif  // COMPONENTS_CRASH_CONTENT_BROWSER_CRASH_DUMP_OBSERVER_ANDROID_H_
