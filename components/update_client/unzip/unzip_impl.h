// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_UNZIP_UNZIP_IMPL_H_
#define COMPONENTS_UPDATE_CLIENT_UNZIP_UNZIP_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/update_client/unzipper.h"

namespace service_manager {
class Connector;
}

namespace update_client {

class UnzipChromiumFactory : public UnzipperFactory {
 public:
  explicit UnzipChromiumFactory(
      std::unique_ptr<service_manager::Connector> connector);

  std::unique_ptr<Unzipper> Create() const override;

 protected:
  ~UnzipChromiumFactory() override;

 private:
  std::unique_ptr<service_manager::Connector> connector_;

  DISALLOW_COPY_AND_ASSIGN(UnzipChromiumFactory);
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_UNZIP_UNZIP_IMPL_H_
