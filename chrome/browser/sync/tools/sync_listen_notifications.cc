// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>
#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/platform_thread.h"
#include "chrome/browser/sync/notification_method.h"
#include "chrome/browser/sync/sync_constants.h"
#include "chrome/browser/sync/notifier/base/task_pump.h"
#include "chrome/browser/sync/notifier/communicator/xmpp_socket_adapter.h"
#include "chrome/browser/sync/notifier/listener/listen_task.h"
#include "chrome/browser/sync/notifier/listener/notification_constants.h"
#include "chrome/browser/sync/notifier/listener/subscribe_task.h"
#include "talk/base/cryptstring.h"
#include "talk/base/logging.h"
#include "talk/base/sigslot.h"
#include "talk/base/physicalsocketserver.h"
#include "talk/base/thread.h"
#include "talk/xmpp/jid.h"
#include "talk/xmpp/xmppclient.h"
#include "talk/xmpp/xmppclientsettings.h"
#include "talk/xmpp/xmppengine.h"

// This is a simple utility that logs into an XMPP server, subscribes
// to Sync notifications, and prints out any such notifications that
// are received.

namespace {

// Main class that listens for and handles messages from the XMPP
// client.
class XmppNotificationClient : public sigslot::has_slots<> {
 public:
  XmppNotificationClient() : xmpp_client_(NULL), should_exit_(true) {}

  // Connect with the given XMPP settings and run until disconnected.
  void Run(const buzz::XmppClientSettings& xmpp_client_settings) {
    CHECK(!xmpp_client_);
    xmpp_client_ = new buzz::XmppClient(&task_pump_);
    CHECK(xmpp_client_);
    xmpp_client_->SignalLogInput.connect(
        this, &XmppNotificationClient::OnXmppClientLogInput);
    xmpp_client_->SignalLogOutput.connect(
        this, &XmppNotificationClient::OnXmppClientLogOutput);
    xmpp_client_->SignalStateChange.connect(
        this, &XmppNotificationClient::OnXmppClientStateChange);

    notifier::XmppSocketAdapter* xmpp_socket_adapter =
        new notifier::XmppSocketAdapter(xmpp_client_settings, false);
    CHECK(xmpp_socket_adapter);
    // Transfers ownership of xmpp_socket_adapter.
    buzz::XmppReturnStatus connect_status =
        xmpp_client_->Connect(xmpp_client_settings, "",
                              xmpp_socket_adapter, NULL, NULL);
    CHECK_EQ(connect_status, buzz::XMPP_RETURN_OK);
    xmpp_client_->Start();
    talk_base::Thread* current_thread =
        talk_base::ThreadManager::CurrentThread();
    should_exit_ = false;
    while (!should_exit_) {
      current_thread->ProcessMessages(100);
      if (task_pump_.HasPendingTimeoutTask()) {
        task_pump_.WakeTasks();
      }
      MessageLoop::current()->RunAllPending();
    }
    // xmpp_client_ is invalid here.
    xmpp_client_ = NULL;
  }

 private:
  void OnXmppClientStateChange(buzz::XmppEngine::State state) {
    switch (state) {
      case buzz::XmppEngine::STATE_START:
        LOG(INFO) << "Connecting...";
        break;
      case buzz::XmppEngine::STATE_OPENING:
        LOG(INFO) << "Logging in...";
        break;
      case buzz::XmppEngine::STATE_OPEN: {
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
        // Owned by task_pump_.
        browser_sync::SubscribeTask* subscribe_task =
            new browser_sync::SubscribeTask(xmpp_client_,
                                            subscribed_services_list);
        subscribe_task->Start();
        // Owned by task_pump_.
        browser_sync::ListenTask* listen_task =
            new browser_sync::ListenTask(xmpp_client_);
        listen_task->Start();
        break;
      }
      case buzz::XmppEngine::STATE_CLOSED:
        LOG(INFO) << "Logged out";
        int subcode;
        buzz::XmppEngine::Error error = xmpp_client_->GetError(&subcode);
        if (error != buzz::XmppEngine::ERROR_NONE) {
          LOG(INFO) << "error: " << error << ", subcode: " << subcode;
        }
        should_exit_ = true;
        buzz::XmppReturnStatus disconnect_status =
            xmpp_client_->Disconnect();
        CHECK_EQ(disconnect_status, buzz::XMPP_RETURN_OK);
        break;
    }
  }

  void OnXmppClientLogInput(const char* data, int len) {
    LOG(INFO) << "XMPP Input: " << std::string(data, len);
  }

  void OnXmppClientLogOutput(const char* data, int len) {
    LOG(INFO) << "XMPP Output: " << std::string(data, len);
  }

  notifier::TaskPump task_pump_;
  // Owned by task_pump.
  buzz::XmppClient* xmpp_client_;
  bool should_exit_;
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
  std::string email = command_line.GetSwitchValueASCII("email");
  if (email.empty()) {
    printf("Usage: %s --email=foo@bar.com [--password=mypassword] "
           "[--server=talk.google.com] [--port=5222] [--allow-plain] "
           "[--disable-tls]\n", argv[0]);
    return -1;
  }
  std::string password = command_line.GetSwitchValueASCII("password");
  std::string server = command_line.GetSwitchValueASCII("server");
  if (server.empty()) {
    server = "talk.google.com";
  }
  std::string port_str = command_line.GetSwitchValueASCII("port");
  int port = 5222;
  if (!port_str.empty()) {
    int port_from_port_str = std::strtol(port_str.c_str(), NULL, 10);
    if (port_from_port_str != 0) {
      LOG(WARNING) << "Invalid port " << port_str << "; using default";
    }
  }
  bool allow_plain = command_line.HasSwitch("allow-plain");
  bool disable_tls = command_line.HasSwitch("disable-tls");

  // Build XMPP client settings.
  buzz::XmppClientSettings xmpp_client_settings;
  buzz::Jid jid(email);
  xmpp_client_settings.set_user(jid.node());
  xmpp_client_settings.set_resource("cc_sync_listen_notifications");
  xmpp_client_settings.set_host(jid.domain());
  xmpp_client_settings.set_allow_plain(allow_plain);
  xmpp_client_settings.set_use_tls(!disable_tls);
  talk_base::InsecureCryptStringImpl insecure_crypt_string;
  insecure_crypt_string.password() = password;
  xmpp_client_settings.set_pass(
      talk_base::CryptString(insecure_crypt_string));
  xmpp_client_settings.set_server(
      talk_base::SocketAddress(server, port));

  // Set up message loops and socket servers.
  talk_base::PhysicalSocketServer physical_socket_server;
  talk_base::InitializeSSL();
  talk_base::Thread main_thread(&physical_socket_server);
  talk_base::ThreadManager::SetCurrent(&main_thread);
  MessageLoopForIO message_loop;

  // Connect and listen.
  XmppNotificationClient xmpp_notification_client;
  xmpp_notification_client.Run(xmpp_client_settings);

  return 0;
}
