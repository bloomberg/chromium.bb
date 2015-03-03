// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/layout_test/layout_test_navigator_connect_service_factory.h"

#include "base/values.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/message_port_delegate.h"
#include "content/public/browser/message_port_provider.h"
#include "content/public/common/message_port_types.h"
#include "content/public/common/navigator_connect_client.h"
#include "url/gurl.h"

namespace content {

namespace {
const char* kTestScheme = "chrome-layout-test";
const char* kEchoService = "echo";
const char* kAnnotateService = "annotate";
const char* kAsValue = "as-value";
}

class LayoutTestNavigatorConnectServiceFactory::Service
    : public MessagePortDelegate {
 public:
  Service();
  ~Service() override;

  void RegisterConnection(int message_port_id, const std::string& service);

  // MessagePortDelegate implementation.
  void SendMessage(
      int message_port_id,
      const MessagePortMessage& message,
      const std::vector<TransferredMessagePort>& sent_message_ports) override;
  void SendMessagesAreQueued(int message_port_id) override;

 private:
  std::map<int, std::string> connections_;
};

LayoutTestNavigatorConnectServiceFactory::Service::Service() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

LayoutTestNavigatorConnectServiceFactory::Service::~Service() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  MessagePortProvider::OnMessagePortDelegateClosing(this);
}

void LayoutTestNavigatorConnectServiceFactory::Service::RegisterConnection(
    int message_port_id,
    const std::string& service) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  connections_[message_port_id] = service;
}

void LayoutTestNavigatorConnectServiceFactory::Service::SendMessage(
    int message_port_id,
    const MessagePortMessage& message,
    const std::vector<TransferredMessagePort>& sent_message_ports) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(connections_.find(message_port_id) != connections_.end());
  if (connections_[message_port_id] == kAnnotateService) {
    scoped_ptr<base::DictionaryValue> reply(new base::DictionaryValue);
    reply->SetString("message_as_string", message.message_as_string);
    reply->Set("message_as_value", message.message_as_value.DeepCopy());
    MessagePortProvider::PostMessageToPort(
        message_port_id, MessagePortMessage(reply.Pass()), sent_message_ports);
  } else {
    MessagePortProvider::PostMessageToPort(message_port_id, message,
                                           sent_message_ports);
  }
}

void LayoutTestNavigatorConnectServiceFactory::Service::SendMessagesAreQueued(
    int message_port_id) {
  NOTREACHED() << "This method should never be called.";
}

LayoutTestNavigatorConnectServiceFactory::
    LayoutTestNavigatorConnectServiceFactory()
    : service_(nullptr) {
}

LayoutTestNavigatorConnectServiceFactory::
    ~LayoutTestNavigatorConnectServiceFactory() {
  if (service_)
    BrowserThread::DeleteSoon(BrowserThread::IO, FROM_HERE, service_);
}

bool LayoutTestNavigatorConnectServiceFactory::HandlesUrl(
    const GURL& target_url) {
  return target_url.SchemeIs(kTestScheme);
}

void LayoutTestNavigatorConnectServiceFactory::Connect(
    const NavigatorConnectClient& client,
    const ConnectCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  std::string service = client.target_url.path();
  if (service != kEchoService && service != kAnnotateService) {
    callback.Run(nullptr, false);
    return;
  }
  if (!service_)
    service_ = new Service;
  service_->RegisterConnection(client.message_port_id, service);
  callback.Run(service_, client.target_url.query() == kAsValue);

  if (service == kAnnotateService) {
    scoped_ptr<base::DictionaryValue> value(new base::DictionaryValue);
    value->SetString("origin", client.origin.spec());
    MessagePortProvider::PostMessageToPort(
        client.message_port_id, MessagePortMessage(value.Pass()),
        std::vector<TransferredMessagePort>());
  }
}

}  // namespace content
