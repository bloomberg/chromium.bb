// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdio>
#include <string>

#include "base/at_exit.h"
#include "base/base64.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chrome/browser/sync/notifier/sync_notifier.h"
#include "chrome/browser/sync/notifier/sync_notifier_factory.h"
#include "chrome/browser/sync/notifier/sync_notifier_observer.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/browser/sync/syncable/model_type_payload_map.h"
#include "chrome/test/test_url_request_context_getter.h"
#include "content/browser/browser_thread.h"

// This is a simple utility that initializes a sync notifier and
// listens to any received notifications.

namespace {

// Class to print received notifications events.
class NotificationPrinter : public sync_notifier::SyncNotifierObserver {
 public:
  NotificationPrinter() {}
  virtual ~NotificationPrinter() {}

  virtual void OnIncomingNotification(
      const syncable::ModelTypePayloadMap& type_payloads) {
    for (syncable::ModelTypePayloadMap::const_iterator it =
             type_payloads.begin(); it != type_payloads.end(); ++it) {
      LOG(INFO) << "Notification: type = "
                << syncable::ModelTypeToString(it->first)
                << ", payload = " << it->second;
    }
  }

  virtual void OnNotificationStateChange(bool notifications_enabled) {
    LOG(INFO) << "Notifications enabled: " << notifications_enabled;
  }

  virtual void StoreState(const std::string& state) {
    std::string base64_state;
    CHECK(base::Base64Encode(state, &base64_state));
    LOG(INFO) << "Got state to store: " << base64_state;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NotificationPrinter);
};

}  // namespace

int main(int argc, char* argv[]) {
  base::AtExitManager exit_manager;
  scoped_refptr<TestURLRequestContextGetter> request_context_getter(
      new TestURLRequestContextGetter);
  BrowserThread io_thread(BrowserThread::IO);
  base::Thread::Options options;
  options.message_loop_type = MessageLoop::TYPE_IO;
  io_thread.StartWithOptions(options);
  CommandLine::Init(argc, argv);
  logging::InitLogging(
      NULL,
      logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG,
      logging::LOCK_LOG_FILE,
      logging::DELETE_OLD_LOG_FILE,
      logging::DISABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS);

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

  // Needed by the SyncNotifier implementations.
  MessageLoop main_loop;

  const char kClientInfo[] = "sync_listen_notifications";
  sync_notifier::SyncNotifierFactory sync_notifier_factory(kClientInfo);
  scoped_ptr<sync_notifier::SyncNotifier> sync_notifier(
      sync_notifier_factory.CreateSyncNotifier(command_line,
                                               request_context_getter.get()));
  NotificationPrinter notification_printer;
  sync_notifier->AddObserver(&notification_printer);

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

  main_loop.Run();

  sync_notifier->RemoveObserver(&notification_printer);
  io_thread.Stop();
  return 0;
}
