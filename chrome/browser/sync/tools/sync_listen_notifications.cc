// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/at_exit.h"
#include "base/base64.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/observer_list.h"
#include "base/string_util.h"
#include "base/task.h"
#include "base/weak_ptr.h"
#include "chrome/browser/sync/notifier/cache_invalidation_packet_handler.h"
#include "chrome/browser/sync/notifier/chrome_invalidation_client.h"
#include "chrome/browser/sync/notifier/chrome_system_resources.h"
#include "chrome/browser/sync/sync_constants.h"
#include "chrome/common/chrome_switches.h"
#include "jingle/notifier/base/notification_method.h"
#include "jingle/notifier/base/xmpp_connection.h"
#include "jingle/notifier/listener/listen_task.h"
#include "jingle/notifier/listener/notification_constants.h"
#include "jingle/notifier/listener/subscribe_task.h"
#include "jingle/notifier/listener/xml_element_util.h"
#include "net/base/ssl_config_service.h"
#include "net/socket/client_socket_factory.h"
#include "talk/base/cryptstring.h"
#include "talk/base/logging.h"
#include "talk/base/task.h"
#include "talk/xmpp/jid.h"
#include "talk/xmpp/xmppclientsettings.h"
#include "talk/xmpp/constants.h"
#include "talk/xmpp/xmppengine.h"

// This is a simple utility that logs into an XMPP server, subscribes
// to Sync notifications, and prints out any such notifications that
// are received.

namespace {

// Main class that listens for and handles messages from the XMPP
// client.
class XmppNotificationClient : public notifier::XmppConnection::Delegate {
 public:
  // An observer is notified when we are logged into XMPP or when an
  // error occurs.
  class Observer {
   public:
    virtual ~Observer() {}

    virtual void OnConnect(base::WeakPtr<talk_base::Task> base_task) = 0;

    virtual void OnError() = 0;
  };

  template <class T> XmppNotificationClient(T begin, T end) {
    for (T it = begin; it != end; ++it) {
      observer_list_.AddObserver(*it);
    }
  }

  virtual ~XmppNotificationClient() {}

  // Connect with the given XMPP settings and run until disconnected.
  void Run(const buzz::XmppClientSettings& xmpp_client_settings) {
    DCHECK(!xmpp_connection_.get());
    xmpp_connection_.reset(
        new notifier::XmppConnection(xmpp_client_settings, this, NULL));
    MessageLoop::current()->Run();
    DCHECK(!xmpp_connection_.get());
  }

  virtual void OnConnect(base::WeakPtr<talk_base::Task> base_task) {
    FOR_EACH_OBSERVER(Observer, observer_list_, OnConnect(base_task));
  }

  virtual void OnError(buzz::XmppEngine::Error error, int subcode,
                       const buzz::XmlElement* stream_error) {
    LOG(INFO) << "Error: " << error << ", subcode: " << subcode;
    if (stream_error) {
      LOG(INFO) << "Stream error: "
                << notifier::XmlElementToString(*stream_error);
    }
    FOR_EACH_OBSERVER(Observer, observer_list_, OnError());
    // This has to go before the message loop quits.
    xmpp_connection_.reset();
    MessageLoop::current()->Quit();
  }

 private:
  ObserverList<Observer> observer_list_;
  scoped_ptr<notifier::XmppConnection> xmpp_connection_;

  DISALLOW_COPY_AND_ASSIGN(XmppNotificationClient);
};

// Delegate for legacy notifications.
class LegacyNotifierDelegate : public XmppNotificationClient::Observer {
 public:
  virtual ~LegacyNotifierDelegate() {}

  virtual void OnConnect(base::WeakPtr<talk_base::Task> base_task) {
    LOG(INFO) << "Logged in";
    notifier::NotificationMethod notification_method =
        notifier::NOTIFICATION_TRANSITIONAL;
    std::vector<std::string> subscribed_services_list;
    if (notification_method != notifier::NOTIFICATION_LEGACY) {
      if (notification_method == notifier::NOTIFICATION_TRANSITIONAL) {
        subscribed_services_list.push_back(
            browser_sync::kSyncLegacyServiceUrl);
      }
      subscribed_services_list.push_back(browser_sync::kSyncServiceUrl);
    }
    // Owned by base_task.
    notifier::SubscribeTask* subscribe_task =
        new notifier::SubscribeTask(base_task, subscribed_services_list);
    subscribe_task->Start();
    // Owned by xmpp_client.
    notifier::ListenTask* listen_task =
        new notifier::ListenTask(base_task);
    listen_task->Start();
  }

  virtual void OnError() {}
};

// The actual listener for sync notifications.
class ChromeInvalidationListener
    : public sync_notifier::ChromeInvalidationClient::Listener {
 public:
  ChromeInvalidationListener() {}

  virtual void OnInvalidate(syncable::ModelType model_type) {
    // TODO(akalin): This is a hack to make new sync data types work
    // with server-issued notifications.  Remove this when it's not
    // needed anymore.
    if (model_type == syncable::UNSPECIFIED) {
      LOG(INFO) << "OnInvalidate: UNKNOWN";
    } else {
      LOG(INFO) << "OnInvalidate: "
                << syncable::ModelTypeToString(model_type);
    }
    // A real implementation would respond to the invalidation.
  }

  virtual void OnInvalidateAll() {
    LOG(INFO) << "InvalidateAll";
    // A real implementation would loop over the current registered
    // data types and send notifications for those.
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeInvalidationListener);
};

// Delegate for server-side notifications.
class CacheInvalidationNotifierDelegate
    : public XmppNotificationClient::Observer,
      public sync_notifier::StateWriter {
 public:
  explicit CacheInvalidationNotifierDelegate(
      const std::string& cache_invalidation_state)
      : cache_invalidation_state_(cache_invalidation_state) {}

  virtual ~CacheInvalidationNotifierDelegate() {}

  virtual void OnConnect(base::WeakPtr<talk_base::Task> base_task) {
    LOG(INFO) << "Logged in";

    // TODO(akalin): app_name should be per-client unique.
    const std::string kAppName = "cc_sync_listen_notifications";
    chrome_invalidation_client_.Start(kAppName, cache_invalidation_state_,
                                      &chrome_invalidation_listener_,
                                      this, base_task);
    chrome_invalidation_client_.RegisterTypes();
  }

  virtual void OnError() {
    chrome_invalidation_client_.Stop();
  }

  virtual void WriteState(const std::string& state) {
    std::string base64_state;
    CHECK(base::Base64Encode(state, &base64_state));
    LOG(INFO) << "New state: " << base64_state;
  }

 private:
  ChromeInvalidationListener chrome_invalidation_listener_;
  // Opaque blob capturing the notifications state of a previous run
  // (i.e., the base64-decoded value of a string output by
  // WriteState()).
  std::string cache_invalidation_state_;
  sync_notifier::ChromeInvalidationClient chrome_invalidation_client_;
};

}  // namespace

int main(int argc, char* argv[]) {
  base::AtExitManager exit_manager;
  CommandLine::Init(argc, argv);
  logging::InitLogging(NULL, logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG,
                       logging::LOCK_LOG_FILE, logging::DELETE_OLD_LOG_FILE);
  logging::SetMinLogLevel(logging::LOG_INFO);
  // TODO(akalin): Make sure that all log messages are printed to the
  // console, even on Windows (SetMinLogLevel isn't enough).
  talk_base::LogMessage::LogToDebug(talk_base::LS_VERBOSE);

  // Parse command line.
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  std::string email = command_line.GetSwitchValueASCII(switches::kSyncEmail);
  if (email.empty()) {
    printf("Usage: %s --email=foo@bar.com [--password=mypassword] "
           "[--server=talk.google.com] [--port=5222] [--allow-plain] "
           "[--disable-tls] [--use-cache-invalidation] [--use-ssl-tcp] "
           "[--cache-invalidtion-state]\n",
           argv[0]);
    return -1;
  }
  std::string password =
      command_line.GetSwitchValueASCII(switches::kSyncPassword);
  std::string server =
      command_line.GetSwitchValueASCII(switches::kSyncServer);
  if (server.empty()) {
    server = "talk.google.com";
  }
  std::string port_str =
      command_line.GetSwitchValueASCII(switches::kSyncPort);
  int port = 5222;
  if (!port_str.empty()) {
    int port_from_port_str = std::strtol(port_str.c_str(), NULL, 10);
    if (port_from_port_str == 0) {
      LOG(WARNING) << "Invalid port " << port_str << "; using default";
    } else {
      port = port_from_port_str;
    }
  }
  bool allow_plain = command_line.HasSwitch(switches::kSyncAllowPlain);
  bool disable_tls = command_line.HasSwitch(switches::kSyncDisableTls);
  bool use_ssl_tcp = command_line.HasSwitch(switches::kSyncUseSslTcp);
  if (use_ssl_tcp && (port != 443)) {
    LOG(WARNING) << switches::kSyncUseSslTcp << " is set but port is " << port
                 << " instead of 443";
  }
  std::string cache_invalidation_state;
  std::string cache_invalidation_state_encoded =
      command_line.GetSwitchValueASCII("cache-invalidation-state");
  if (!cache_invalidation_state_encoded.empty() &&
      !base::Base64Decode(cache_invalidation_state_encoded,
                          &cache_invalidation_state)) {
    LOG(ERROR) << "Could not decode state: "
               << cache_invalidation_state_encoded;
  }

  // Build XMPP client settings.
  buzz::XmppClientSettings xmpp_client_settings;
  buzz::Jid jid(email);
  xmpp_client_settings.set_user(jid.node());
  xmpp_client_settings.set_resource("cc_sync_listen_notifications");
  xmpp_client_settings.set_host(jid.domain());
  xmpp_client_settings.set_allow_plain(allow_plain);
  xmpp_client_settings.set_use_tls(!disable_tls);
  if (use_ssl_tcp) {
    xmpp_client_settings.set_protocol(cricket::PROTO_SSLTCP);
  }
  talk_base::InsecureCryptStringImpl insecure_crypt_string;
  insecure_crypt_string.password() = password;
  xmpp_client_settings.set_pass(
      talk_base::CryptString(insecure_crypt_string));
  talk_base::SocketAddress addr(server, port);
  if (!addr.ResolveIP()) {
    LOG(ERROR) << "Could not resolve " << addr.ToString();
    return -1;
  }
  xmpp_client_settings.set_server(addr);

  MessageLoopForIO message_loop;

  // Connect and listen.
  LegacyNotifierDelegate legacy_notifier_delegate;
  CacheInvalidationNotifierDelegate cache_invalidation_notifier_delegate(
      cache_invalidation_state);
  std::vector<XmppNotificationClient::Observer*> observers;
  // TODO(akalin): Add switch to listen to both.
  if (command_line.HasSwitch(switches::kSyncUseCacheInvalidation)) {
    observers.push_back(&cache_invalidation_notifier_delegate);
  } else {
    observers.push_back(&legacy_notifier_delegate);
  }
  // TODO(akalin): Revert the move of all switches in this file into
  // chrome_switches.h.
  XmppNotificationClient xmpp_notification_client(
      observers.begin(), observers.end());
  xmpp_notification_client.Run(xmpp_client_settings);

  return 0;
}
