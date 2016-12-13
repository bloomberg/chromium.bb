// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_SESSION_SESSION_H_
#define MASH_SESSION_SESSION_H_

#include <map>
#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "services/service_manager/public/cpp/interface_factory.h"
#include "services/service_manager/public/cpp/service.h"

namespace service_manager {
class Connection;
}

namespace mash {
namespace session {

class Session : public service_manager::Service {
 public:
  Session();
  ~Session() override;

 private:
  // service_manager::Service:
  void OnStart() override;
  bool OnConnect(const service_manager::ServiceInfo& remote_info,
                 service_manager::InterfaceRegistry* registry) override;

  void StartWindowManager();
  void StartQuickLaunch();

  // Starts the application at |url|, running |restart_callback| if the
  // connection to the application is closed.
  void StartRestartableService(const std::string& url,
                               const base::Closure& restart_callback);

  std::map<std::string, std::unique_ptr<service_manager::Connection>>
      connections_;

  DISALLOW_COPY_AND_ASSIGN(Session);
};

}  // namespace session
}  // namespace mash

#endif  // MASH_SESSION_SESSION_H_
