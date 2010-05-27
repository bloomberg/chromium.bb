// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>
#include <string>
#include <sstream>
#include <vector>

#include "base/at_exit.h"
#include "base/base64.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/platform_thread.h"
#include "base/string_util.h"
#include "base/task.h"
#include "chrome/browser/sync/notification_method.h"
#include "chrome/browser/sync/sync_constants.h"
#include "chrome/common/net/notifier/base/task_pump.h"
#include "chrome/common/net/notifier/communicator/xmpp_socket_adapter.h"
#include "chrome/common/net/notifier/listener/listen_task.h"
#include "chrome/common/net/notifier/listener/notification_constants.h"
#include "chrome/common/net/notifier/listener/subscribe_task.h"
#include "chrome/common/net/notifier/listener/xml_element_util.h"
#include "google/cacheinvalidation/invalidation-client.h"
#include "google/cacheinvalidation/invalidation-client-impl.h"
#include "talk/base/cryptstring.h"
#include "talk/base/logging.h"
#include "talk/base/sigslot.h"
#include "talk/base/physicalsocketserver.h"
#include "talk/base/thread.h"
#include "talk/xmpp/jid.h"
#include "talk/xmpp/xmppclient.h"
#include "talk/xmpp/xmppclientsettings.h"
#include "talk/xmpp/xmppconstants.h"
#include "talk/xmpp/xmppengine.h"

// This is a simple utility that logs into an XMPP server, subscribes
// to Sync notifications, and prints out any such notifications that
// are received.

namespace {

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
        xmpp_client_(NULL),
        should_exit_(true) {
    CHECK(delegate_);
  }

  // Connect with the given XMPP settings and run until disconnected.
  void Run(const buzz::XmppClientSettings& xmpp_client_settings) {
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

    notifier::XmppSocketAdapter* xmpp_socket_adapter =
        new notifier::XmppSocketAdapter(xmpp_client_settings_, false);
    CHECK(xmpp_socket_adapter);
    // Transfers ownership of xmpp_socket_adapter.
    buzz::XmppReturnStatus connect_status =
        xmpp_client_->Connect(xmpp_client_settings_, "",
                              xmpp_socket_adapter, NULL);
    CHECK_EQ(connect_status, buzz::XMPP_RETURN_OK);
    xmpp_client_->Start();
    talk_base::Thread* current_thread =
        talk_base::ThreadManager::CurrentThread();
    should_exit_ = false;
    while (!should_exit_) {
      current_thread->ProcessMessages(100);
      MessageLoop::current()->RunAllPending();
    }
    // xmpp_client_ is invalid here.
    xmpp_client_ = NULL;
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

  Delegate* delegate_;
  notifier::TaskPump task_pump_;
  buzz::XmppClientSettings xmpp_client_settings_;
  // Owned by task_pump.
  buzz::XmppClient* xmpp_client_;
  bool should_exit_;
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

// TODO(akalin): Move all the cache-invalidation-related stuff below
// out of this file once we have a real Chrome-integrated
// implementation.

void RunAndDeleteClosure(invalidation::Closure* task) {
  task->Run();
  delete task;
}

// Simple system resources class that uses the current message loop
// for scheduling.  Assumes the current message loop is already
// running.
class ChromeSystemResources : public invalidation::SystemResources {
 public:
  ChromeSystemResources() : scheduler_active_(false) {}

  ~ChromeSystemResources() {
    CHECK(!scheduler_active_);
  }

  virtual invalidation::Time current_time() {
    return base::Time::Now();
  }

  virtual void StartScheduler() {
    CHECK(!scheduler_active_);
    scheduler_active_ = true;
  }

  // We assume that the current message loop is stopped shortly after
  // this is called, i.e. that any in-flight delayed tasks won't get
  // run.
  //
  // TODO(akalin): Make sure that the above actually holds.
  virtual void StopScheduler() {
    CHECK(scheduler_active_);
    scheduler_active_ = false;
  }

  virtual void ScheduleWithDelay(invalidation::TimeDelta delay,
                                 invalidation::Closure* task) {
    if (!scheduler_active_) {
      delete task;
      return;
    }
    CHECK(invalidation::IsCallbackRepeatable(task));
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        NewRunnableFunction(&RunAndDeleteClosure, task),
        delay.InMillisecondsRoundedUp());
  }

  virtual void ScheduleImmediately(invalidation::Closure* task) {
    if (!scheduler_active_) {
      delete task;
      return;
    }
    CHECK(invalidation::IsCallbackRepeatable(task));
    MessageLoop::current()->PostTask(
        FROM_HERE, NewRunnableFunction(&RunAndDeleteClosure, task));
  }

  virtual void Log(LogLevel level, const char* file, int line,
                   const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    std::string result;
    StringAppendV(&result, format, ap);
    logging::LogMessage(file, line).stream() << result;
    va_end(ap);
  }

 private:
  bool scheduler_active_;
};

// We need to write our own protobuf to string functions because we
// use LITE_RUNTIME, which doesn't support DebugString().

std::string ObjectIdToString(
    const invalidation::ObjectId& object_id) {
  std::stringstream ss;
  ss << "{ ";
  ss << "name: " << object_id.name().string_value() << ", ";
  ss << "source: " << object_id.source();
  ss << " }";
  return ss.str();
}

std::string StatusToString(
    const invalidation::Status& status) {
  std::stringstream ss;
  ss << "{ ";
  ss << "code: " << status.code() << ", ";
  ss << "description: " << status.description();
  ss << " }";
  return ss.str();
}

std::string InvalidationToString(
    const invalidation::Invalidation& invalidation) {
  std::stringstream ss;
  ss << "{ ";
  ss << "object_id: " << ObjectIdToString(invalidation.object_id()) << ", ";
  ss << "version: " << invalidation.version() << ", ";
  ss << "components: { ";
  const invalidation::ComponentStampLog& component_stamp_log =
      invalidation.component_stamp_log();
  for (int i = 0; i < component_stamp_log.stamps_size(); ++i) {
    const invalidation::ComponentStamp& component_stamp =
        component_stamp_log.stamps(i);
    ss << "component: " << component_stamp.component() << ", ";
    ss << "time: " << component_stamp.time() << ", ";
  }
  ss << " }";
  ss << " }";
  return ss.str();
}

std::string RegistrationUpdateToString(
    const invalidation::RegistrationUpdate& update) {
  std::stringstream ss;
  ss << "{ ";
  ss << "type: " << update.type() << ", ";
  ss << "object_id: " << ObjectIdToString(update.object_id()) << ", ";
  ss << "version: " << update.version() << ", ";
  ss << "sequence_number: " << update.sequence_number();
  ss << " }";
  return ss.str();
}

std::string RegistrationUpdateResultToString(
    const invalidation::RegistrationUpdateResult& update_result) {
  std::stringstream ss;
  ss << "{ ";
  ss << "operation: "
     << RegistrationUpdateToString(update_result.operation()) << ", ";
  ss << "status: " << StatusToString(update_result.status());
  ss << " }";
  return ss.str();
}

// The actual listener for sync notifications from the cache
// invalidation service.
class ChromeInvalidationListener
    : public invalidation::InvalidationListener {
 public:
  ChromeInvalidationListener() {}

  virtual void Invalidate(const invalidation::Invalidation& invalidation,
                          invalidation::Closure* callback) {
    CHECK(invalidation::IsCallbackRepeatable(callback));
    LOG(INFO) << "Invalidate: " << InvalidationToString(invalidation);
    RunAndDeleteClosure(callback);
    // A real implementation would respond to the invalidation for the
    // given object (e.g., refetch the invalidated object).
  }

  virtual void InvalidateAll(invalidation::Closure* callback) {
    CHECK(invalidation::IsCallbackRepeatable(callback));
    LOG(INFO) << "InvalidateAll";
    RunAndDeleteClosure(callback);
    // A real implementation would loop over the current registered
    // data types and send notifications for those.
  }

  virtual void AllRegistrationsLost(invalidation::Closure* callback) {
    CHECK(invalidation::IsCallbackRepeatable(callback));
    LOG(INFO) << "AllRegistrationsLost";
    RunAndDeleteClosure(callback);
    // A real implementation would try to re-register for all
    // registered data types.
  }

  virtual void RegistrationLost(const invalidation::ObjectId& object_id,
                                invalidation::Closure* callback) {
    CHECK(invalidation::IsCallbackRepeatable(callback));
    LOG(INFO) << "RegistrationLost: "
              << ObjectIdToString(object_id);
    RunAndDeleteClosure(callback);
    // A real implementation would try to re-register for this
    // particular data type.
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeInvalidationListener);
};

static const buzz::QName kQnTangoIqPacket("google:tango", "packet");
static const buzz::QName kQnTangoIqPacketContent(
    "google:tango", "content");

// A task that listens for ClientInvalidation messages and calls the
// given callback on them.
class CacheInvalidationListenTask : public buzz::XmppTask {
 public:
  // Takes ownership of callback.
  CacheInvalidationListenTask(Task* parent,
                              Callback1<const std::string&>::Type* callback)
      : XmppTask(parent, buzz::XmppEngine::HL_TYPE), callback_(callback) {}
  virtual ~CacheInvalidationListenTask() {}

  virtual int ProcessStart() {
    LOG(INFO) << "CacheInvalidationListenTask started";
    return STATE_RESPONSE;
  }

  virtual int ProcessResponse() {
    const buzz::XmlElement* stanza = NextStanza();
    if (stanza == NULL) {
      LOG(INFO) << "CacheInvalidationListenTask blocked";
      return STATE_BLOCKED;
    }
    LOG(INFO) << "CacheInvalidationListenTask response received";
    std::string data;
    if (GetTangoIqPacketData(stanza, &data)) {
      callback_->Run(data);
    } else {
      LOG(ERROR) << "Could not get packet data";
    }
    // Acknowledge receipt of the iq to the buzz server.
    // TODO(akalin): Send an error response for malformed packets.
    scoped_ptr<buzz::XmlElement> response_stanza(MakeIqResult(stanza));
    SendStanza(response_stanza.get());
    return STATE_RESPONSE;
  }

  virtual bool HandleStanza(const buzz::XmlElement* stanza) {
    LOG(INFO) << "Stanza received: "
              << notifier::XmlElementToString(*stanza);
    if (IsValidTangoIqPacket(stanza)) {
      LOG(INFO) << "Queueing stanza";
      QueueStanza(stanza);
      return true;
    }
    LOG(INFO) << "Stanza skipped";
    return false;
  }

 private:
  bool IsValidTangoIqPacket(const buzz::XmlElement* stanza) {
    return
        (MatchRequestIq(stanza, buzz::STR_SET, kQnTangoIqPacket) &&
         (stanza->Attr(buzz::QN_TO) == GetClient()->jid().Str()));
  }

  bool GetTangoIqPacketData(const buzz::XmlElement* stanza,
                            std::string* data) {
    DCHECK(IsValidTangoIqPacket(stanza));
    const buzz::XmlElement* tango_iq_packet =
        stanza->FirstNamed(kQnTangoIqPacket);
    if (!tango_iq_packet) {
      LOG(ERROR) << "Could not find tango IQ packet element";
      return false;
    }
    const buzz::XmlElement* tango_iq_packet_content =
        tango_iq_packet->FirstNamed(kQnTangoIqPacketContent);
    if (!tango_iq_packet_content) {
      LOG(ERROR) << "Could not find tango IQ packet content element";
      return false;
    }
    *data = tango_iq_packet_content->BodyText();
    return true;
  }

  scoped_ptr<Callback1<const std::string&>::Type> callback_;
  DISALLOW_COPY_AND_ASSIGN(CacheInvalidationListenTask);
};

// A task that sends a single outbound ClientInvalidation message.
class CacheInvalidationSendMessageTask : public buzz::XmppTask {
 public:
  CacheInvalidationSendMessageTask(Task* parent,
                                   const buzz::Jid& to_jid,
                                   const std::string& msg)
      : XmppTask(parent, buzz::XmppEngine::HL_SINGLE),
        to_jid_(to_jid), msg_(msg) {}
  virtual ~CacheInvalidationSendMessageTask() {}

  virtual int ProcessStart() {
    scoped_ptr<buzz::XmlElement> stanza(
        MakeTangoIqPacket(to_jid_, task_id(), msg_));
    LOG(INFO) << "Sending message: "
              << notifier::XmlElementToString(*stanza.get());
    if (SendStanza(stanza.get()) != buzz::XMPP_RETURN_OK) {
      LOG(INFO) << "Error when sending message";
      return STATE_ERROR;
    }
    return STATE_RESPONSE;
  }

  virtual int ProcessResponse() {
    const buzz::XmlElement* stanza = NextStanza();
    if (stanza == NULL) {
      LOG(INFO) << "CacheInvalidationSendMessageTask blocked...";
      return STATE_BLOCKED;
    }
    LOG(INFO) << "CacheInvalidationSendMessageTask response received: "
              << notifier::XmlElementToString(*stanza);
    return STATE_DONE;
  }

  virtual bool HandleStanza(const buzz::XmlElement* stanza) {
    LOG(INFO) << "Stanza received: "
              << notifier::XmlElementToString(*stanza);
    if (!MatchResponseIq(stanza, to_jid_, task_id())) {
      LOG(INFO) << "Stanza skipped";
      return false;
    }
    LOG(INFO) << "Queueing stanza";
    QueueStanza(stanza);
    return true;
  }

 private:
  static buzz::XmlElement* MakeTangoIqPacket(
      const buzz::Jid& to_jid,
      const std::string& task_id,
      const std::string& msg) {
    buzz::XmlElement* iq = MakeIq(buzz::STR_SET, to_jid, task_id);
    buzz::XmlElement* tango_iq_packet =
        new buzz::XmlElement(kQnTangoIqPacket, true);
    iq->AddElement(tango_iq_packet);
    buzz::XmlElement* tango_iq_packet_content =
        new buzz::XmlElement(kQnTangoIqPacketContent, true);
    tango_iq_packet->AddElement(tango_iq_packet_content);
    tango_iq_packet_content->SetBodyText(msg);
    return iq;
  }

  buzz::Jid to_jid_;
  std::string msg_;

  DISALLOW_COPY_AND_ASSIGN(CacheInvalidationSendMessageTask);
};

// Class that handles the details of sending and receiving client
// invalidation packets.
class CacheInvalidationPacketHandler {
 public:
  CacheInvalidationPacketHandler(
      buzz::XmppClient* xmpp_client,
      invalidation::NetworkEndpoint* network_endpoint,
      const buzz::Jid& to_jid)
      : xmpp_client_(xmpp_client), network_endpoint_(network_endpoint),
        to_jid_(to_jid) {
    // Owned by xmpp_client.
    CacheInvalidationListenTask* listen_task =
        new CacheInvalidationListenTask(
            xmpp_client, NewCallback(
                this, &CacheInvalidationPacketHandler::HandleInboundPacket));
    listen_task->Start();
  }

  void HandleOutboundPacket(invalidation::NetworkEndpoint* const &
                            network_endpoint) {
    CHECK_EQ(network_endpoint, network_endpoint_);
    invalidation::string message;
    network_endpoint->TakeOutboundMessage(&message);
    std::string encoded_message;
    CHECK(base::Base64Encode(message, &encoded_message));
    // Owned by xmpp_client.
    CacheInvalidationSendMessageTask* send_message_task =
        new CacheInvalidationSendMessageTask(xmpp_client_,
                                             to_jid_,
                                             encoded_message);
    send_message_task->Start();
  }

 private:
  void HandleInboundPacket(const std::string& packet) {
    std::string decoded_message;
    CHECK(base::Base64Decode(packet, &decoded_message));
    network_endpoint_->HandleInboundMessage(decoded_message);
  }

  buzz::XmppClient* xmpp_client_;
  invalidation::NetworkEndpoint* network_endpoint_;
  buzz::Jid to_jid_;
};

// Delegate for server-side notifications.
class CacheInvalidationNotifierDelegate
    : public XmppNotificationClient::Delegate {
 public:
  CacheInvalidationNotifierDelegate(
      MessageLoop* message_loop,
      const std::vector<std::string>& data_types) {
    if (data_types.empty()) {
      LOG(WARNING) << "No data types given";
    }
    for (std::vector<std::string>::const_iterator it = data_types.begin();
         it != data_types.end(); ++it) {
      invalidation::ObjectId object_id;
      object_id.mutable_name()->set_string_value(*it);
      object_id.set_source(invalidation::ObjectId::CHROME_SYNC);
      object_ids_.push_back(object_id);
    }
  }

  virtual ~CacheInvalidationNotifierDelegate() {}

  virtual void OnLogin(
      const buzz::XmppClientSettings& xmpp_client_settings,
      buzz::XmppClient* xmpp_client) {
    LOG(INFO) << "Logged in";

    chrome_system_resources_.StartScheduler();

    invalidation::ClientType client_type;
    client_type.set_type(invalidation::ClientType::CHROME_SYNC);
    // TODO(akalin): app_name should be per-client unique.
    std::string app_name = "cc_sync_listen_notifications";
    invalidation::ClientConfig ticl_config;
    invalidation_client_.reset(
        new invalidation::InvalidationClientImpl(
            &chrome_system_resources_, client_type,
            app_name, &chrome_invalidation_listener_, ticl_config));

    invalidation::NetworkEndpoint* network_endpoint =
        invalidation_client_->network_endpoint();
    // TODO(akalin): Make tango jid configurable (so we can use
    // staging).
    buzz::Jid to_jid("tango@bot.talk.google.com/PROD");
    packet_handler_.reset(
        new CacheInvalidationPacketHandler(xmpp_client,
                                           network_endpoint,
                                           to_jid));

    network_endpoint->RegisterOutboundListener(
        invalidation::NewPermanentCallback(
            packet_handler_.get(),
            &CacheInvalidationPacketHandler::HandleOutboundPacket));

    for (std::vector<invalidation::ObjectId>::const_iterator it =
             object_ids_.begin(); it != object_ids_.end(); ++it) {
      invalidation_client_->Register(
          *it,
          invalidation::NewPermanentCallback(
              this, &CacheInvalidationNotifierDelegate::RegisterCallback));
    }
  }

  virtual void OnLogout() {
    LOG(INFO) << "Logged out";

    // TODO(akalin): Figure out the correct place to put this.
    for (std::vector<invalidation::ObjectId>::const_iterator it =
             object_ids_.begin(); it != object_ids_.end(); ++it) {
      invalidation_client_->Unregister(
          *it,
          invalidation::NewPermanentCallback(
              this, &CacheInvalidationNotifierDelegate::RegisterCallback));
    }

    packet_handler_.reset();
    invalidation_client_.reset();

    chrome_system_resources_.StopScheduler();
  }

  virtual void OnError(buzz::XmppEngine::Error error, int subcode) {
    LOG(INFO) << "Error: " << error << ", subcode: " << subcode;
    packet_handler_.reset();
    invalidation_client_.reset();

    // TODO(akalin): Figure out whether we should stop the scheduler
    // here.
  }

 private:
  void RegisterCallback(
      const invalidation::RegistrationUpdateResult& result) {
    LOG(INFO) << "Registered: " << RegistrationUpdateResultToString(result);
  }

  void UnregisterCallback(
      const invalidation::RegistrationUpdateResult& result) {
    LOG(INFO) << "Unregistered: " << RegistrationUpdateResultToString(result);
  }

  std::vector<invalidation::ObjectId> object_ids_;
  ChromeSystemResources chrome_system_resources_;
  ChromeInvalidationListener chrome_invalidation_listener_;
  scoped_ptr<invalidation::InvalidationClient> invalidation_client_;
  scoped_ptr<CacheInvalidationPacketHandler> packet_handler_;
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
           "[--disable-tls] [--use-cache-invalidation] [--use-ssl-tcp]\n",
           argv[0]);
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
    if (port_from_port_str == 0) {
      LOG(WARNING) << "Invalid port " << port_str << "; using default";
    } else {
      port = port_from_port_str;
    }
  }
  bool allow_plain = command_line.HasSwitch("allow-plain");
  bool disable_tls = command_line.HasSwitch("disable-tls");
  bool use_ssl_tcp = command_line.HasSwitch("use-ssl-tcp");
  if (use_ssl_tcp && (port != 443)) {
    LOG(WARNING) << "--use-ssl-tcp is set but port is " << port
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
  xmpp_client_settings.set_server(
      talk_base::SocketAddress(server, port));

  // Set up message loops and socket servers.
  talk_base::PhysicalSocketServer physical_socket_server;
  talk_base::InitializeSSL();
  talk_base::Thread main_thread(&physical_socket_server);
  talk_base::ThreadManager::SetCurrent(&main_thread);
  MessageLoopForIO message_loop;

  // TODO(akalin): Make this configurable.
  // TODO(akalin): Store these constants in a header somewhere (maybe
  // browser/sync/protocol).
  std::vector<std::string> data_types;
  data_types.push_back("AUTOFILL");
  data_types.push_back("BOOKMARK");
  data_types.push_back("THEME");
  data_types.push_back("PREFERENCE");

  // Connect and listen.
  LegacyNotifierDelegate legacy_notifier_delegate;
  CacheInvalidationNotifierDelegate cache_invalidation_notifier_delegate(
      &message_loop, data_types);
  XmppNotificationClient::Delegate* delegate = NULL;
  if (command_line.HasSwitch("use-cache-invalidation")) {
    delegate = &cache_invalidation_notifier_delegate;
  } else {
    delegate = &legacy_notifier_delegate;
  }
  XmppNotificationClient xmpp_notification_client(delegate);
  xmpp_notification_client.Run(xmpp_client_settings);

  return 0;
}
