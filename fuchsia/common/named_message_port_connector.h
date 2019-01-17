// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_COMMON_NAMED_MESSAGE_PORT_CONNECTOR_H_
#define FUCHSIA_COMMON_NAMED_MESSAGE_PORT_CONNECTOR_H_

#include <deque>
#include <map>
#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "fuchsia/common/fuchsia_export.h"
#include "fuchsia/fidl/chromium/web/cpp/fidl.h"

namespace webrunner {

// The implementation actively creates a MessagePort for each registered Id,
// passing them all to the web container every time a page-load occurs. It then
// waits for the page to acknowledge each MessagePort, before invoking its
// registered |handler| to notify the called that it is ready for use.
class FUCHSIA_EXPORT NamedMessagePortConnector {
 public:
  using PortConnectedCallback =
      base::RepeatingCallback<void(chromium::web::MessagePortPtr)>;

  NamedMessagePortConnector();
  ~NamedMessagePortConnector();

  // Registers a |handler| which will be passed a new MessagePort after every
  // page load.
  //
  // The first call to Register() will ensure that the supporting JavaScript
  // bundle for the NamedMessagePortConnector is injected into |frame|.
  // Therefore, any JavaScript bundles which depend on the Connector must be
  // injected after Register is called.
  void Register(const std::string& id,
                PortConnectedCallback handler,
                chromium::web::Frame* frame);

  // Unregisters a handler for |frame|. Should be called before |frame| is
  // deleted.
  void Unregister(chromium::web::Frame* frame, const std::string& id);

  // Client should call this every time a page is loaded.
  void NotifyPageLoad(chromium::web::Frame* frame);

 private:
  struct RegistrationEntry {
    RegistrationEntry();
    ~RegistrationEntry();
    RegistrationEntry(const RegistrationEntry& other);

    std::string id;
    PortConnectedCallback handler;
  };

  void InjectBindings(chromium::web::Frame* frame);

  std::multimap<chromium::web::Frame*, RegistrationEntry> registrations_;
  fuchsia::mem::Buffer bindings_script_;

  DISALLOW_COPY_AND_ASSIGN(NamedMessagePortConnector);
};

}  // namespace webrunner

#endif  // FUCHSIA_COMMON_NAMED_MESSAGE_PORT_CONNECTOR_H_
