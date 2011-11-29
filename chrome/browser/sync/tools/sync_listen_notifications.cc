// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdio>
#include <string>

#include "base/at_exit.h"
#include "base/base64.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/threading/thread.h"
#include "chrome/browser/sync/notifier/invalidation_version_tracker.h"
#include "chrome/browser/sync/notifier/sync_notifier.h"
#include "chrome/browser/sync/notifier/sync_notifier_factory.h"
#include "chrome/browser/sync/notifier/sync_notifier_observer.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/browser/sync/syncable/model_type_payload_map.h"
#include "chrome/test/base/test_url_request_context_getter.h"
#include "content/public/browser/browser_thread.h"
#include "content/browser/browser_thread_impl.h"

using content::BrowserThread;

// This is a simple utility that initializes a sync notifier and
// listens to any received notifications.

namespace {

// Class to print received notifications events.
class NotificationPrinter : public sync_notifier::SyncNotifierObserver {
 public:
  NotificationPrinter() {}
  virtual ~NotificationPrinter() {}

  virtual void OnIncomingNotification(
      const syncable::ModelTypePayloadMap& type_payloads) OVERRIDE {
    for (syncable::ModelTypePayloadMap::const_iterator it =
             type_payloads.begin(); it != type_payloads.end(); ++it) {
      LOG(INFO) << "Notification: type = "
                << syncable::ModelTypeToString(it->first)
                << ", payload = " << it->second;
    }
  }

  virtual void OnNotificationStateChange(
      bool notifications_enabled) OVERRIDE {
    LOG(INFO) << "Notifications enabled: " << notifications_enabled;
  }

  virtual void StoreState(const std::string& state) OVERRIDE {
    std::string base64_state;
    CHECK(base::Base64Encode(state, &base64_state));
    LOG(INFO) << "Got state to store: " << base64_state;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NotificationPrinter);
};

class NullInvalidationVersionTracker
    : public base::SupportsWeakPtr<NullInvalidationVersionTracker>,
      public sync_notifier::InvalidationVersionTracker {
 public:
  NullInvalidationVersionTracker() {}
  virtual ~NullInvalidationVersionTracker() {}

  virtual sync_notifier::InvalidationVersionMap
      GetAllMaxVersions() const OVERRIDE {
    return sync_notifier::InvalidationVersionMap();
  }

  virtual void SetMaxVersion(
      syncable::ModelType model_type,
      int64 max_invalidation_version) OVERRIDE {
    DVLOG(1) << "Setting max invalidation version for "
             << syncable::ModelTypeToString(model_type) << " to "
             << max_invalidation_version;
  }
};

}  // namespace

int main(int argc, char* argv[]) {
  base::AtExitManager exit_manager;
  CommandLine::Init(argc, argv);
  logging::InitLogging(
      NULL,
      logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG,
      logging::LOCK_LOG_FILE,
      logging::DELETE_OLD_LOG_FILE,
      logging::DISABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS);

  MessageLoop ui_loop;
  content::BrowserThreadImpl ui_thread(BrowserThread::UI, &ui_loop);
  content::BrowserThreadImpl io_thread(BrowserThread::IO);
  base::Thread::Options options;
  options.message_loop_type = MessageLoop::TYPE_IO;
  io_thread.StartWithOptions(options);

  // Parse command line.
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  std::string email = command_line.GetSwitchValueASCII("email");
  std::string token = command_line.GetSwitchValueASCII("token");
  // TODO(akalin): Write a wrapper script that gets a token for an
  // email and password and passes that in to this utility.
  if (email.empty() || token.empty()) {
    std::printf("Usage: %s --email=foo@bar.com --token=token\n\n"
                "See sync_notifier_factory.cc for more switches.\n\n"
                "Run chrome and set a breakpoint on "
                "sync_api::SyncManager::SyncInternal::UpdateCredentials() "
                "after logging into sync to get the token to pass into this "
                "utility.\n",
                argv[0]);
    return -1;
  }

  const char kClientInfo[] = "sync_listen_notifications";
  scoped_refptr<TestURLRequestContextGetter> request_context_getter(
      new TestURLRequestContextGetter());
  NullInvalidationVersionTracker null_invalidation_version_tracker;
  sync_notifier::SyncNotifierFactory sync_notifier_factory(
      kClientInfo, request_context_getter,
      null_invalidation_version_tracker.AsWeakPtr(), command_line);
  scoped_ptr<sync_notifier::SyncNotifier> sync_notifier(
      sync_notifier_factory.CreateSyncNotifier());
  NotificationPrinter notification_printer;
  sync_notifier->AddObserver(&notification_printer);

  const char kUniqueId[] = "fake_unique_id";
  sync_notifier->SetUniqueId(kUniqueId);
  sync_notifier->SetState("");
  sync_notifier->UpdateCredentials(email, token);
  {
    // Listen for notifications for all known types.
    syncable::ModelTypeSet types;
    for (int i = syncable::FIRST_REAL_MODEL_TYPE;
         i < syncable::MODEL_TYPE_COUNT; ++i) {
      types.insert(syncable::ModelTypeFromInt(i));
    }
    sync_notifier->UpdateEnabledTypes(types);
  }

  ui_loop.Run();

  sync_notifier->RemoveObserver(&notification_printer);
  io_thread.Stop();
  return 0;
}
