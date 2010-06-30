// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/cache_invalidation_packet_handler.h"

#include "base/base64.h"
#include "base/callback.h"
#include "base/logging.h"
#include "google/cacheinvalidation/invalidation-client.h"
#include "jingle/notifier/listener/xml_element_util.h"
#include "talk/xmpp/constants.h"
#include "talk/xmpp/jid.h"
#include "talk/xmpp/xmppclient.h"
#include "talk/xmpp/xmpptask.h"

namespace sync_notifier {

namespace {

const char kBotJid[] = "tango@bot.talk.google.com";

// TODO(akalin): Eliminate use of 'tango' name.

// TODO(akalin): Hash out details of tango IQ protocol.

static const buzz::QName kQnTangoIqPacket("google:tango", "packet");
static const buzz::QName kQnTangoIqPacketContent(
    "google:tango", "content");

// TODO(akalin): Move these task classes out so that they can be
// unit-tested.  This'll probably be done easier once we consolidate
// all the packet sending/receiving classes.

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
    // TODO(akalin): Handle errors here.
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

  const buzz::Jid to_jid_;
  std::string msg_;

  DISALLOW_COPY_AND_ASSIGN(CacheInvalidationSendMessageTask);
};

}  // namespace

CacheInvalidationPacketHandler::CacheInvalidationPacketHandler(
    buzz::XmppClient* xmpp_client,
    invalidation::InvalidationClient* invalidation_client)
    : xmpp_client_(xmpp_client),
      invalidation_client_(invalidation_client) {
  CHECK(xmpp_client_);
  CHECK(invalidation_client_);
  invalidation::NetworkEndpoint* network_endpoint =
      invalidation_client_->network_endpoint();
  CHECK(network_endpoint);
  network_endpoint->RegisterOutboundListener(
      invalidation::NewPermanentCallback(
          this,
          &CacheInvalidationPacketHandler::HandleOutboundPacket));
  // Owned by xmpp_client.
  CacheInvalidationListenTask* listen_task =
      new CacheInvalidationListenTask(
          xmpp_client, NewCallback(
              this, &CacheInvalidationPacketHandler::HandleInboundPacket));
  listen_task->Start();
}

CacheInvalidationPacketHandler::~CacheInvalidationPacketHandler() {
  invalidation::NetworkEndpoint* network_endpoint =
      invalidation_client_->network_endpoint();
  CHECK(network_endpoint);
  network_endpoint->RegisterOutboundListener(NULL);
}

void CacheInvalidationPacketHandler::HandleOutboundPacket(
    invalidation::NetworkEndpoint* const& network_endpoint) {
  CHECK_EQ(network_endpoint, invalidation_client_->network_endpoint());
  invalidation::string message;
  network_endpoint->TakeOutboundMessage(&message);
  std::string encoded_message;
  if (!base::Base64Encode(message, &encoded_message)) {
    LOG(ERROR) << "Could not base64-encode message to send: "
               << message;
    return;
  }
  // Owned by xmpp_client.
  CacheInvalidationSendMessageTask* send_message_task =
      new CacheInvalidationSendMessageTask(xmpp_client_,
                                           buzz::Jid(kBotJid),
                                           encoded_message);
  send_message_task->Start();
}

void CacheInvalidationPacketHandler::HandleInboundPacket(
    const std::string& packet) {
  invalidation::NetworkEndpoint* network_endpoint =
      invalidation_client_->network_endpoint();
  std::string decoded_message;
  if (!base::Base64Decode(packet, &decoded_message)) {
    LOG(ERROR) << "Could not base64-decode received message: "
               << packet;
    return;
  }
  network_endpoint->HandleInboundMessage(decoded_message);
}

}  // namespace sync_notifier
