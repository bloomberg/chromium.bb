// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/common/named_message_port_connector.h"

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"
#include "fuchsia/common/mem_buffer_util.h"
#include "fuchsia/fidl/chromium/web/cpp/fidl.h"

namespace webrunner {
namespace {

const char kBindingsJsPath[] =
    FILE_PATH_LITERAL("fuchsia/common/named_message_port_connector.js");
const char kConnectedMessage[] = "connected";

}  // namespace

NamedMessagePortConnector::NamedMessagePortConnector() {
  base::FilePath assets_path;
  CHECK(base::PathService::Get(base::DIR_ASSETS, &assets_path));
  bindings_script_ = MemBufferFromFile(
      base::File(assets_path.AppendASCII(kBindingsJsPath),
                 base::File::FLAG_OPEN | base::File::FLAG_READ));
}

NamedMessagePortConnector::~NamedMessagePortConnector() = default;

void NamedMessagePortConnector::Register(const std::string& id,
                                         PortConnectedCallback handler,
                                         chromium::web::Frame* frame) {
  DCHECK(handler);
  DCHECK(!id.empty() && id.find(' ') == std::string::npos);

  if (registrations_.find(frame) == registrations_.end())
    InjectBindings(frame);

  RegistrationEntry entry;
  entry.id = id;
  entry.handler = std::move(handler);
  registrations_.insert(std::make_pair(std::move(frame), std::move(entry)));
}

void NamedMessagePortConnector::Unregister(chromium::web::Frame* frame,
                                           const std::string& id) {
  auto port_range = registrations_.equal_range(frame);
  auto it = port_range.first;
  while (it != port_range.second) {
    if (it->second.id == id) {
      it = registrations_.erase(it);
      continue;
    }
    it++;
  }
}

void NamedMessagePortConnector::NotifyPageLoad(chromium::web::Frame* frame) {
  auto registration_range = registrations_.equal_range(frame);

  // Push all bound MessagePorts to the page after every page load.
  for (auto it = registration_range.first; it != registration_range.second;
       ++it) {
    const RegistrationEntry& registration = it->second;

    chromium::web::WebMessage message;
    message.data = webrunner::MemBufferFromString("connect " + registration.id);

    // Call the handler callback, with the MessagePort client object.
    message.outgoing_transfer =
        std::make_unique<chromium::web::OutgoingTransferable>();
    chromium::web::MessagePortPtr message_port;
    message.outgoing_transfer->set_message_port(message_port.NewRequest());

    // Send the port to the handler once a "connected" message is received from
    // the peer, so that the caller has a stronger guarantee that the content is
    // healthy and capable of processing messages.
    message_port->ReceiveMessage(
        [message_port = std::move(message_port),
         handler =
             registration.handler](chromium::web::WebMessage message) mutable {
          std::string message_str;
          if (!StringFromMemBuffer(message.data, &message_str)) {
            LOG(ERROR) << "Couldn't read from message VMO.";
            return;
          }

          if (message_str != kConnectedMessage) {
            LOG(ERROR) << "Unexpected message from port: " << message_str;
            return;
          }

          handler.Run(std::move(message_port));
        });

    // Pass the other half of the MessagePort connection to the document.
    it->first->PostMessage(std::move(message), "*",
                           [](bool result) { CHECK(result); });
  }
}

void NamedMessagePortConnector::InjectBindings(chromium::web::Frame* frame) {
  DCHECK(frame);

  std::vector<std::string> origins = {"*"};
  frame->ExecuteJavaScript(
      std::move(origins), CloneBuffer(bindings_script_),
      chromium::web::ExecuteMode::ON_PAGE_LOAD,
      [](bool success) { CHECK(success) << "Couldn't inject bindings."; });
}

NamedMessagePortConnector::RegistrationEntry::RegistrationEntry() = default;

NamedMessagePortConnector::RegistrationEntry::~RegistrationEntry() = default;

NamedMessagePortConnector::RegistrationEntry::RegistrationEntry(
    const RegistrationEntry& other) = default;

}  // namespace webrunner
