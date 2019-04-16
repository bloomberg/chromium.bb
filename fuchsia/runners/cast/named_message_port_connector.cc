// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/runners/cast/named_message_port_connector.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"
#include "fuchsia/base/mem_buffer_util.h"
#include "fuchsia/runners/cast/cast_platform_bindings_ids.h"

namespace {

const char kBindingsJsPath[] =
    FILE_PATH_LITERAL("fuchsia/runners/cast/named_message_port_connector.js");
const char kConnectedMessage[] = "connected";

}  // namespace

NamedMessagePortConnector::NamedMessagePortConnector() {
  base::FilePath assets_path;
  CHECK(base::PathService::Get(base::DIR_ASSETS, &assets_path));
  bindings_script_ = cr_fuchsia::MemBufferFromFile(
      base::File(assets_path.AppendASCII(kBindingsJsPath),
                 base::File::FLAG_OPEN | base::File::FLAG_READ));
}

NamedMessagePortConnector::~NamedMessagePortConnector() = default;

void NamedMessagePortConnector::Register(const std::string& id,
                                         PortConnectedCallback handler,
                                         fuchsia::web::Frame* frame) {
  DCHECK(handler);
  DCHECK(!id.empty() && id.find(' ') == std::string::npos);

  if (registrations_.find(frame) == registrations_.end())
    InjectBindings(frame);

  RegistrationEntry entry;
  entry.id = id;
  entry.handler = std::move(handler);
  registrations_.insert(std::make_pair(std::move(frame), std::move(entry)));
}

void NamedMessagePortConnector::Unregister(fuchsia::web::Frame* frame,
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

void NamedMessagePortConnector::NotifyPageLoad(fuchsia::web::Frame* frame) {
  auto registration_range = registrations_.equal_range(frame);

  // Push all bound MessagePorts to the page after every page load.
  for (auto it = registration_range.first; it != registration_range.second;
       ++it) {
    const RegistrationEntry& registration = it->second;

    fuchsia::web::WebMessage message;
    message.set_data(
        cr_fuchsia::MemBufferFromString("connect " + registration.id));

    // Call the handler callback, with the MessagePort client object.
    fuchsia::web::MessagePortPtr message_port;
    std::vector<fuchsia::web::OutgoingTransferable> outgoing_vector(1);
    outgoing_vector[0].set_message_port(message_port.NewRequest());
    message.set_outgoing_transfer(std::move(outgoing_vector));

    // Send the port to the handler once a "connected" message is received from
    // the peer, so that the caller has a stronger guarantee that the content is
    // healthy and capable of processing messages.
    message_port->ReceiveMessage([message_port = std::move(message_port),
                                  handler = registration.handler](
                                     fuchsia::web::WebMessage message) mutable {
      std::string message_str;
      if (!message.has_data() ||
          !cr_fuchsia::StringFromMemBuffer(message.data(), &message_str)) {
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
    it->first->PostMessage("*", std::move(message),
                           [](fuchsia::web::Frame_PostMessage_Result result) {
                             CHECK(result.is_response());
                           });
  }
}

void NamedMessagePortConnector::InjectBindings(fuchsia::web::Frame* frame) {
  DCHECK(frame);

  std::vector<std::string> origins = {"*"};
  frame->AddBeforeLoadJavaScript(
      static_cast<uint64_t>(
          CastPlatformBindingsId::NAMED_MESSAGE_PORT_CONNECTOR),
      std::move(origins), cr_fuchsia::CloneBuffer(bindings_script_),
      [](fuchsia::web::Frame_AddBeforeLoadJavaScript_Result result) {
        CHECK(result.is_response()) << "Couldn't inject bindings.";
      });
}

NamedMessagePortConnector::RegistrationEntry::RegistrationEntry() = default;

NamedMessagePortConnector::RegistrationEntry::~RegistrationEntry() = default;

NamedMessagePortConnector::RegistrationEntry::RegistrationEntry(
    const RegistrationEntry& other) = default;
