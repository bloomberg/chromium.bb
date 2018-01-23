// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_CLIENT_SET_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_CLIENT_SET_H_

#include <map>
#include <memory>
#include <set>

#include "base/macros.h"
#include "components/download/public/background_service/clients.h"

namespace download {

// Helper class to hold a list of Clients and associates them with their
// respective DownloadClient identifier.
class ClientSet {
 public:
  explicit ClientSet(std::unique_ptr<DownloadClientMap> clients);
  virtual ~ClientSet();

  std::set<DownloadClient> GetRegisteredClients() const;
  Client* GetClient(DownloadClient id) const;

 private:
  std::unique_ptr<DownloadClientMap> clients_;

  DISALLOW_COPY_AND_ASSIGN(ClientSet);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_CLIENT_SET_H_
