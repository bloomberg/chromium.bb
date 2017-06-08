// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_GCM_APP_HANDLER_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_GCM_APP_HANDLER_H_

#include <string>

#include "components/gcm_driver/gcm_app_handler.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "components/offline_pages/core/prefetch/prefetch_gcm_handler.h"

namespace offline_pages {

extern const char kPrefetchingOfflinePagesAppId[];

// Receives GCM messages and other channel status messages on behalf of the
// prefetch system.
class PrefetchGCMAppHandler : public gcm::GCMAppHandler,
                              public PrefetchGCMHandler {
 public:
  class TokenFactory {
   public:
    virtual ~TokenFactory() = default;

    virtual void GetGCMToken(
        instance_id::InstanceID::GetTokenCallback callback) = 0;
  };

  PrefetchGCMAppHandler(std::unique_ptr<TokenFactory> token_factory);
  ~PrefetchGCMAppHandler() override;

  // PrefetchGCMHandler implementation.
  void GetGCMToken(instance_id::InstanceID::GetTokenCallback callback) override;

  // gcm::GCMAppHandler implementation.
  void ShutdownHandler() override;
  void OnStoreReset() override;
  void OnMessage(const std::string& app_id,
                 const gcm::IncomingMessage& message) override;
  void OnMessagesDeleted(const std::string& app_id) override;
  void OnSendError(
      const std::string& app_id,
      const gcm::GCMClient::SendErrorDetails& send_error_details) override;
  void OnSendAcknowledged(const std::string& app_id,
                          const std::string& message_id) override;
  bool CanHandle(const std::string& app_id) const override;

  // offline_pages::PrefetchGCMHandler implementation.
  gcm::GCMAppHandler* AsGCMAppHandler() override;
  std::string GetAppId() const override;

 private:
  std::unique_ptr<TokenFactory> token_factory_;

  DISALLOW_COPY_AND_ASSIGN(PrefetchGCMAppHandler);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_GCM_APP_HANDLER_H_
