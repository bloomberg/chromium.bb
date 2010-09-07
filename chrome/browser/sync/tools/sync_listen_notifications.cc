// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/platform_thread.h"
#include "base/string_util.h"
#include "base/task.h"
#include "chrome/browser/sync/notification_method.h"
#include "chrome/browser/sync/notifier/cache_invalidation_packet_handler.h"
#include "chrome/browser/sync/notifier/chrome_invalidation_client.h"
#include "chrome/browser/sync/notifier/chrome_system_resources.h"
#include "chrome/browser/sync/sync_constants.h"
#include "chrome/common/chrome_switches.h"
#include "jingle/notifier/base/chrome_async_socket.h"
#include "jingle/notifier/base/task_pump.h"
#include "jingle/notifier/base/xmpp_client_socket_factory.h"
#include "jingle/notifier/communicator/xmpp_socket_adapter.h"
#include "jingle/notifier/listener/listen_task.h"
#include "jingle/notifier/listener/notification_constants.h"
#include "jingle/notifier/listener/subscribe_task.h"
#include "net/base/ssl_config_service.h"
#include "net/socket/client_socket_factory.h"
#include "talk/base/cryptstring.h"
#include "talk/base/logging.h"
#include "talk/base/sigslot.h"
#include "talk/base/physicalsocketserver.h"
#include "talk/base/ssladapter.h"
#include "talk/base/thread.h"
#include "talk/xmpp/jid.h"
#include "talk/xmpp/xmppclient.h"
#include "talk/xmpp/xmppclientsettings.h"
#include "talk/xmpp/constants.h"
#include "talk/xmpp/xmppengine.h"

// This is a simple utility that logs into an XMPP server, subscribes
// to Sync notifications, and prints out any such notifications that
// are received.

namespace {

void PumpAuxiliaryLoops() {
  talk_base::Thread* current_thread =
      talk_base::ThreadManager::CurrentThread();
  current_thread->ProcessMessages(100);
  MessageLoop::current()->PostTask(
      FROM_HERE, NewRunnableFunction(&PumpAuxiliaryLoops));
}

// Main class that listens for and handles messages from the XMPP
// client.
class XmppNotificationClient : public sigslot::has_slots<> {
 public:
  // A delegate is notified when we are logged in and out of XMPP or
  // when an error occurs.
  //
  // TODO(akalin): Change Delegate to Observer so we can listen both
  // to legacy and cache invalidation notifications.
  class Delegate {
   public:
    virtual ~Delegate() {}

    // The given xmpp_client is valid until OnLogout() or OnError() is
    // called.
    virtual void OnLogin(
        const buzz::XmppClientSettings& xmpp_client_settings,
        buzz::XmppClient* xmpp_client) = 0;

    virtual void OnLogout() = 0;

    virtual void OnError(buzz::XmppEngine::Error error, int subcode) = 0;
  };

  explicit XmppNotificationClient(Delegate* delegate)
      : delegate_(delegate),
        xmpp_client_(NULL) {
    CHECK(delegate_);
  }

  // Connect with the given XMPP settings and run until disconnected.
  void Run(const buzz::XmppClientSettings& xmpp_client_settings,
           bool use_chrome_async_socket) {
    CHECK(!xmpp_client_);
    xmpp_client_settings_ = xmpp_client_settings;
    xmpp_client_ = new buzz::XmppClient(&task_pump_);
    CHECK(xmpp_client_);
    xmpp_client_->SignalLogInput.connect(
        this, &XmppNotificationClient::OnXmppClientLogInput);
    xmpp_client_->SignalLogOutput.connect(
        this, &XmppNotificationClient::OnXmppClientLogOutput);
    xmpp_client_->SignalStateChange.connect(
        this, &XmppNotificationClient::OnXmppClientStateChange);

    net::SSLConfig ssl_config;
    bool use_fake_ssl_client_socket =
        (xmpp_client_settings.protocol() == cricket::PROTO_SSLTCP);
    buzz::AsyncSocket* buzz_async_socket =
        use_chrome_async_socket ?
        static_cast<buzz::AsyncSocket*>(
            new notifier::ChromeAsyncSocket(
                new notifier::XmppClientSocketFactory(
                    net::ClientSocketFactory::GetDefaultFactory(),
                    use_fake_ssl_client_socket),
                ssl_config, 4096, 64 * 1024, NULL)) :
        static_cast<buzz::AsyncSocket*>(
            new notifier::XmppSocketAdapter(xmpp_client_settings_, false));
    CHECK(buzz_async_socket);
    // Transfers ownership of buzz_async_socket.
    buzz::XmppReturnStatus connect_status =
        xmpp_client_->Connect(xmpp_client_settings_, "",
                              buzz_async_socket, NULL);
    CHECK_EQ(connect_status, buzz::XMPP_RETURN_OK);
    xmpp_client_->Start();
    if (!use_chrome_async_socket) {
      MessageLoop::current()->PostTask(
          FROM_HERE, NewRunnableFunction(&PumpAuxiliaryLoops));
    }
    MessageLoop::current()->Run();
    DCHECK(!xmpp_client_);
  }

 private:
  void OnXmppClientStateChange(buzz::XmppEngine::State state) {
    switch (state) {
      case buzz::XmppEngine::STATE_START:
        LOG(INFO) << "Starting...";
        break;
      case buzz::XmppEngine::STATE_OPENING:
        LOG(INFO) << "Opening...";
        break;
      case buzz::XmppEngine::STATE_OPEN: {
        LOG(INFO) << "Opened";
        delegate_->OnLogin(xmpp_client_settings_, xmpp_client_);
        break;
      }
      case buzz::XmppEngine::STATE_CLOSED:
        LOG(INFO) << "Closed";
        int subcode;
        buzz::XmppEngine::Error error = xmpp_client_->GetError(&subcode);
        if (error == buzz::XmppEngine::ERROR_NONE) {
          delegate_->OnLogout();
        } else {
          delegate_->OnError(error, subcode);
        }
        MessageLoop::current()->Quit();
        xmpp_client_ = NULL;
        break;
    }
  }

  void OnXmppClientLogInput(const char* data, int len) {
    LOG(INFO) << "XMPP Input: " << std::string(data, len);
  }

  void OnXmppClientLogOutput(const char* data, int len) {
    LOG(INFO) << "XMPP Output: " << std::string(data, len);
  }

  Delegate* delegate_;
  notifier::TaskPump task_pump_;
  buzz::XmppClientSettings xmpp_client_settings_;
  // Owned by task_pump.
  buzz::XmppClient* xmpp_client_;
};

// Delegate for legacy notifications.
class LegacyNotifierDelegate : public XmppNotificationClient::Delegate {
 public:
  virtual ~LegacyNotifierDelegate() {}

  virtual void OnLogin(
      const buzz::XmppClientSettings& xmpp_client_settings,
      buzz::XmppClient* xmpp_client) {
    LOG(INFO) << "Logged in";
    browser_sync::NotificationMethod notification_method =
        browser_sync::NOTIFICATION_TRANSITIONAL;
    std::vector<std::string> subscribed_services_list;
    if (notification_method != browser_sync::NOTIFICATION_LEGACY) {
      if (notification_method == browser_sync::NOTIFICATION_TRANSITIONAL) {
        subscribed_services_list.push_back(
            browser_sync::kSyncLegacyServiceUrl);
      }
      subscribed_services_list.push_back(browser_sync::kSyncServiceUrl);
    }
    // Owned by xmpp_client.
    notifier::SubscribeTask* subscribe_task =
        new notifier::SubscribeTask(xmpp_client, subscribed_services_list);
    subscribe_task->Start();
    // Owned by xmpp_client.
    notifier::ListenTask* listen_task =
        new notifier::ListenTask(xmpp_client);
    listen_task->Start();
  }

  virtual void OnLogout() {
    LOG(INFO) << "Logged out";
  }

  virtual void OnError(buzz::XmppEngine::Error error, int subcode) {
    LOG(INFO) << "Error: " << error << ", subcode: " << subcode;
  }
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
    : public XmppNotificationClient::Delegate {
 public:
  CacheInvalidationNotifierDelegate() {}

  virtual ~CacheInvalidationNotifierDelegate() {}

  virtual void OnLogin(
      const buzz::XmppClientSettings& xmpp_client_settings,
      buzz::XmppClient* xmpp_client) {
    LOG(INFO) << "Logged in";

    // TODO(akalin): app_name should be per-client unique.
    const std::string kAppName = "cc_sync_listen_notifications";
    chrome_invalidation_client_.Start(kAppName,
                                      &chrome_invalidation_listener_,
                                      xmpp_client);
    chrome_invalidation_client_.RegisterTypes();
  }

  virtual void OnLogout() {
    LOG(INFO) << "Logged out";
    chrome_invalidation_client_.Stop();
  }

  virtual void OnError(buzz::XmppEngine::Error error, int subcode) {
    LOG(INFO) << "Error: " << error << ", subcode: " << subcode;
    chrome_invalidation_client_.Stop();
  }

 private:
  ChromeInvalidationListener chrome_invalidation_listener_;
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
           "[--use-chrome-async-socket]\n",
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

  // Set up message loops and socket servers.
  talk_base::PhysicalSocketServer physical_socket_server;
  talk_base::InitializeSSL();
  talk_base::Thread main_thread(&physical_socket_server);
  talk_base::ThreadManager::SetCurrent(&main_thread);
  MessageLoopForIO message_loop;

  // Connect and listen.
  LegacyNotifierDelegate legacy_notifier_delegate;
  CacheInvalidationNotifierDelegate cache_invalidation_notifier_delegate;
  XmppNotificationClient::Delegate* delegate = NULL;
  if (command_line.HasSwitch(switches::kSyncUseCacheInvalidation)) {
    delegate = &cache_invalidation_notifier_delegate;
  } else {
    delegate = &legacy_notifier_delegate;
  }
  // TODO(akalin): Revert the move of all switches in this file into
  // chrome_switches.h.
  bool use_chrome_async_socket =
      command_line.HasSwitch("use-chrome-async-socket");
  XmppNotificationClient xmpp_notification_client(delegate);
  xmpp_notification_client.Run(xmpp_client_settings,
                               use_chrome_async_socket);

  return 0;
}
