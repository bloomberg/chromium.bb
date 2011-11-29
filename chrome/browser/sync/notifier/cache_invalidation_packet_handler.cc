// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/cache_invalidation_packet_handler.h"

#include <string>

#include "base/base64.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/string_number_conversions.h"
#include "google/cacheinvalidation/v2/constants.h"
#include "google/cacheinvalidation/v2/invalidation-client.h"
#include "google/cacheinvalidation/v2/system-resources.h"
#include "jingle/notifier/listener/xml_element_util.h"
#include "talk/xmpp/constants.h"
#include "talk/xmpp/jid.h"
#include "talk/xmpp/xmppclient.h"
#include "talk/xmpp/xmpptask.h"

namespace sync_notifier {

namespace {

const char kBotJid[] = "tango@bot.talk.google.com";
const char kServiceUrl[] = "http://www.google.com/chrome/sync";

buzz::QName GetQnData() { return buzz::QName("google:notifier", "data"); }
buzz::QName GetQnSeq() { return buzz::QName("", "seq"); }
buzz::QName GetQnSid() { return buzz::QName("", "sid"); }
buzz::QName GetQnServiceUrl() { return buzz::QName("", "serviceUrl"); }
buzz::QName GetQnProtocolVersion() {
  return buzz::QName("", "protocolVersion");
}
buzz::QName GetQnChannelContext() {
  return buzz::QName("", "channelContext");
}

// TODO(akalin): Move these task classes out so that they can be
// unit-tested.  This'll probably be done easier once we consolidate
// all the packet sending/receiving classes.

// A task that listens for ClientInvalidation messages and calls the
// given callback on them.
class CacheInvalidationListenTask : public buzz::XmppTask {
 public:
  // Takes ownership of callback.
  CacheInvalidationListenTask(
      buzz::XmppTaskParentInterface* parent,
      Callback1<const std::string&>::Type* callback,
      Callback1<const std::string&>::Type* context_change_callback)
      : XmppTask(parent, buzz::XmppEngine::HL_TYPE),
        callback_(callback),
        context_change_callback_(context_change_callback) {}
  virtual ~CacheInvalidationListenTask() {}

  virtual int ProcessStart() {
    DVLOG(2) << "CacheInvalidationListenTask started";
    return STATE_RESPONSE;
  }

  virtual int ProcessResponse() {
    const buzz::XmlElement* stanza = NextStanza();
    if (stanza == NULL) {
      DVLOG(2) << "CacheInvalidationListenTask blocked";
      return STATE_BLOCKED;
    }
    DVLOG(2) << "CacheInvalidationListenTask response received";
    std::string data;
    if (GetCacheInvalidationIqPacketData(stanza, &data)) {
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
    DVLOG(1) << "Stanza received: "
             << notifier::XmlElementToString(*stanza);
    if (IsValidCacheInvalidationIqPacket(stanza)) {
      DVLOG(2) << "Queueing stanza";
      QueueStanza(stanza);
      return true;
    }
    DVLOG(2) << "Stanza skipped";
    return false;
  }

 private:
  bool IsValidCacheInvalidationIqPacket(const buzz::XmlElement* stanza) {
    // We deliberately minimize the verification we do here: see
    // http://crbug.com/71285 .
    return MatchRequestIq(stanza, buzz::STR_SET, GetQnData());
  }

  bool GetCacheInvalidationIqPacketData(const buzz::XmlElement* stanza,
                                        std::string* data) {
    DCHECK(IsValidCacheInvalidationIqPacket(stanza));
    const buzz::XmlElement* cache_invalidation_iq_packet =
        stanza->FirstNamed(GetQnData());
    if (!cache_invalidation_iq_packet) {
      LOG(ERROR) << "Could not find cache invalidation IQ packet element";
      return false;
    }
    // Look for a channelContext attribute in the content of the stanza.  If
    // present, remember it so it can be echoed back.
    if (cache_invalidation_iq_packet->HasAttr(GetQnChannelContext())) {
      context_change_callback_->Run(
          cache_invalidation_iq_packet->Attr(GetQnChannelContext()));
    }
    *data = cache_invalidation_iq_packet->BodyText();
    return true;
  }

  std::string* channel_context_;
  scoped_ptr<Callback1<const std::string&>::Type> callback_;
  scoped_ptr<Callback1<const std::string&>::Type> context_change_callback_;
  DISALLOW_COPY_AND_ASSIGN(CacheInvalidationListenTask);
};

std::string MakeProtocolVersion() {
  return base::Uint64ToString(invalidation::Constants::kProtocolMajorVersion) +
      "." +
      base::Uint64ToString(invalidation::Constants::kProtocolMinorVersion);
}

// A task that sends a single outbound ClientInvalidation message.
class CacheInvalidationSendMessageTask : public buzz::XmppTask {
 public:
  CacheInvalidationSendMessageTask(buzz::XmppTaskParentInterface* parent,
                                   const buzz::Jid& to_jid,
                                   const std::string& msg,
                                   int seq,
                                   const std::string& sid,
                                   const std::string& channel_context)
      : XmppTask(parent, buzz::XmppEngine::HL_SINGLE),
        to_jid_(to_jid), msg_(msg), seq_(seq), sid_(sid),
        channel_context_(channel_context) {}
  virtual ~CacheInvalidationSendMessageTask() {}

  virtual int ProcessStart() {
    scoped_ptr<buzz::XmlElement> stanza(
        MakeCacheInvalidationIqPacket(to_jid_, task_id(), msg_,
                                      seq_, sid_, channel_context_));
    DVLOG(1) << "Sending message: "
             << notifier::XmlElementToString(*stanza.get());
    if (SendStanza(stanza.get()) != buzz::XMPP_RETURN_OK) {
      DVLOG(2) << "Error when sending message";
      return STATE_ERROR;
    }
    return STATE_RESPONSE;
  }

  virtual int ProcessResponse() {
    const buzz::XmlElement* stanza = NextStanza();
    if (stanza == NULL) {
      DVLOG(2) << "CacheInvalidationSendMessageTask blocked...";
      return STATE_BLOCKED;
    }
    DVLOG(2) << "CacheInvalidationSendMessageTask response received: "
             << notifier::XmlElementToString(*stanza);
    // TODO(akalin): Handle errors here.
    return STATE_DONE;
  }

  virtual bool HandleStanza(const buzz::XmlElement* stanza) {
    DVLOG(1) << "Stanza received: "
             << notifier::XmlElementToString(*stanza);
    if (!MatchResponseIq(stanza, to_jid_, task_id())) {
      DVLOG(2) << "Stanza skipped";
      return false;
    }
    DVLOG(2) << "Queueing stanza";
    QueueStanza(stanza);
    return true;
  }

 private:
  static buzz::XmlElement* MakeCacheInvalidationIqPacket(
      const buzz::Jid& to_jid,
      const std::string& task_id,
      const std::string& msg,
      int seq, const std::string& sid, const std::string& channel_context) {
    buzz::XmlElement* iq = MakeIq(buzz::STR_SET, to_jid, task_id);
    buzz::XmlElement* cache_invalidation_iq_packet =
        new buzz::XmlElement(GetQnData(), true);
    iq->AddElement(cache_invalidation_iq_packet);
    cache_invalidation_iq_packet->SetAttr(GetQnSeq(), base::IntToString(seq));
    cache_invalidation_iq_packet->SetAttr(GetQnSid(), sid);
    cache_invalidation_iq_packet->SetAttr(GetQnServiceUrl(), kServiceUrl);
    cache_invalidation_iq_packet->SetAttr(
        GetQnProtocolVersion(), MakeProtocolVersion());
    if (!channel_context.empty()) {
      cache_invalidation_iq_packet->SetAttr(GetQnChannelContext(),
                                            channel_context);
    }
    cache_invalidation_iq_packet->SetBodyText(msg);
    return iq;
  }

  const buzz::Jid to_jid_;
  std::string msg_;
  int seq_;
  std::string sid_;
  const std::string channel_context_;

  DISALLOW_COPY_AND_ASSIGN(CacheInvalidationSendMessageTask);
};

std::string MakeSid() {
  uint64 sid = base::RandUint64();
  return std::string("chrome-sync-") + base::Uint64ToString(sid);
}

}  // namespace

CacheInvalidationPacketHandler::CacheInvalidationPacketHandler(
    base::WeakPtr<buzz::XmppTaskParentInterface> base_task)
    : scoped_callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      base_task_(base_task),
      seq_(0),
      sid_(MakeSid()) {
  CHECK(base_task_.get());
  // Owned by base_task.  Takes ownership of the callback.
  CacheInvalidationListenTask* listen_task =
      new CacheInvalidationListenTask(
          base_task_, scoped_callback_factory_.NewCallback(
              &CacheInvalidationPacketHandler::HandleInboundPacket),
          scoped_callback_factory_.NewCallback(
              &CacheInvalidationPacketHandler::HandleChannelContextChange));
  listen_task->Start();
}

CacheInvalidationPacketHandler::~CacheInvalidationPacketHandler() {
  DCHECK(non_thread_safe_.CalledOnValidThread());
}

void CacheInvalidationPacketHandler::SendMessage(
    const std::string& message) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  if (!base_task_.get()) {
    return;
  }
  std::string encoded_message;
  if (!base::Base64Encode(message, &encoded_message)) {
    LOG(ERROR) << "Could not base64-encode message to send: "
               << message;
    return;
  }
  // Owned by base_task_.
  CacheInvalidationSendMessageTask* send_message_task =
      new CacheInvalidationSendMessageTask(base_task_,
                                           buzz::Jid(kBotJid),
                                           encoded_message,
                                           seq_, sid_, channel_context_);
  send_message_task->Start();
  ++seq_;
}

void CacheInvalidationPacketHandler::SetMessageReceiver(
    invalidation::MessageCallback* incoming_receiver) {
  incoming_receiver_.reset(incoming_receiver);
}

void CacheInvalidationPacketHandler::HandleInboundPacket(
    const std::string& packet) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  std::string decoded_message;
  if (!base::Base64Decode(packet, &decoded_message)) {
    LOG(ERROR) << "Could not base64-decode received message: "
               << packet;
    return;
  }
  incoming_receiver_->Run(decoded_message);
}

void CacheInvalidationPacketHandler::HandleChannelContextChange(
    const std::string& context) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  channel_context_ = context;
}

}  // namespace sync_notifier
