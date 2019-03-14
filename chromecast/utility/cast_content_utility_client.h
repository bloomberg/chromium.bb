// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_UTILITY_CAST_CONTENT_UTILITY_CLIENT_H_
#define CHROMECAST_UTILITY_CAST_CONTENT_UTILITY_CLIENT_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "build/build_config.h"
#include "content/public/utility/content_utility_client.h"

namespace chromecast {
namespace shell {

class CastContentUtilityClient : public content::ContentUtilityClient {
 public:
  static std::unique_ptr<CastContentUtilityClient> Create();

  CastContentUtilityClient() {}

#if !defined(OS_FUCHSIA)
  // content::ContentUtilityClient implementation:
  bool HandleServiceRequest(
      const std::string& service_name,
      service_manager::mojom::ServiceRequest request) override;
#endif  // !defined(OS_FUCHSIA)

 private:
  DISALLOW_COPY_AND_ASSIGN(CastContentUtilityClient);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_UTILITY_CAST_CONTENT_UTILITY_CLIENT_H_
