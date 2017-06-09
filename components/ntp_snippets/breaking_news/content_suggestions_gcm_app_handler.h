// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_BREAKING_NEWS_CONTENT_SUGGESTIONS_GCM_APP_HANDLER_H_
#define COMPONENTS_NTP_SNIPPETS_BREAKING_NEWS_CONTENT_SUGGESTIONS_GCM_APP_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "components/gcm_driver/gcm_app_handler.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "components/ntp_snippets/breaking_news/subscription_manager.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

class PrefRegistrySimple;

namespace gcm {
class GCMDriver;
}

namespace instance_id {
class InstanceIDDriver;
}

namespace ntp_snippets {

// Handler for pushed GCM content suggestions. It retrieves a subscription token
// from the GCM server and registers/unregisters itself with the GCM service to
// be called upon received push content suggestions.
class ContentSuggestionsGCMAppHandler : public gcm::GCMAppHandler {
 public:
  ContentSuggestionsGCMAppHandler(
      gcm::GCMDriver* gcm_driver,
      instance_id::InstanceIDDriver* instance_id_driver,
      PrefService* pref_service_,
      std::unique_ptr<SubscriptionManager> subscription_manager);

  // If still listening, calls StopListening()
  ~ContentSuggestionsGCMAppHandler() override;

  // Subscribe to the GCM service if necessary and start listening for pushed
  // content suggestions. Must not be called if already listening.
  void StartListening();

  // Remove the handler, and stop listening for incoming GCM messages. Any
  // further pushed content suggestions will be ignored. Must be called while
  // listening.
  void StopListening();

  // GCMAppHandler overrides.
  void ShutdownHandler() override;
  void OnStoreReset() override;
  void OnMessage(const std::string& app_id,
                 const gcm::IncomingMessage& message) override;
  void OnMessagesDeleted(const std::string& app_id) override;
  void OnSendError(const std::string& app_id,
                   const gcm::GCMClient::SendErrorDetails& details) override;
  void OnSendAcknowledged(const std::string& app_id,
                          const std::string& message_id) override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

 private:
  // Retrieves a subscription token that allows the content suggestions server
  // to push content via GCM messages. Calling this method multiple times is not
  // necessary but does not harm since the same token is returned everytime.
  void Subscribe();

  // Called after the subscription is obtained from the GCM server.
  void DidSubscribe(const std::string& subscription_id,
                    instance_id::InstanceID::Result result);

  gcm::GCMDriver* const gcm_driver_;
  instance_id::InstanceIDDriver* const instance_id_driver_;
  PrefService* const pref_service_;
  const std::unique_ptr<SubscriptionManager> subscription_manager_;
  base::WeakPtrFactory<ContentSuggestionsGCMAppHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ContentSuggestionsGCMAppHandler);
};
}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_BREAKING_NEWS_CONTENT_SUGGESTIONS_GCM_APP_HANDLER_H_
