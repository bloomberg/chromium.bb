// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_SESSION_SESSION_H_
#define MASH_SESSION_SESSION_H_

#include "base/macros.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/mojom/service.mojom.h"

namespace mash {
namespace session {

class Session : public service_manager::Service {
 public:
  explicit Session(service_manager::mojom::ServiceRequest request);
  ~Session() override;

 private:
  // service_manager::Service:
  void OnStart() override;

  void StartWindowManager();

  service_manager::ServiceBinding service_binding_;

  DISALLOW_COPY_AND_ASSIGN(Session);
};

}  // namespace session
}  // namespace mash

#endif  // MASH_SESSION_SESSION_H_
