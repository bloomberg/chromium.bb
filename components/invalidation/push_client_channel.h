// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_PUSH_CLIENT_CHANNEL_H_
#define COMPONENTS_INVALIDATION_PUSH_CLIENT_CHANNEL_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "components/invalidation/invalidation_export.h"
#include "components/invalidation/sync_system_resources.h"
#include "jingle/notifier/listener/push_client_observer.h"

namespace notifier {
class PushClient;
}  // namespace notifier

namespace syncer {

// A PushClientChannel is an implementation of NetworkChannel that
// routes messages through a PushClient.
class INVALIDATION_EXPORT_PRIVATE PushClientChannel
    : public SyncNetworkChannel,
      public NON_EXPORTED_BASE(notifier::PushClientObserver) {
 public:
  // |push_client| is guaranteed to be destroyed only when this object
  // is destroyed.
  explicit PushClientChannel(scoped_ptr<notifier::PushClient> push_client);

  virtual ~PushClientChannel();

  // invalidation::NetworkChannel implementation.
  virtual void SendMessage(const std::string& message) OVERRIDE;
  virtual void RequestDetailedStatus(
      base::Callback<void(const base::DictionaryValue&)> callback) OVERRIDE;

  // SyncNetworkChannel implementation.
  // If not connected, connects with the given credentials.  If
  // already connected, the next connection attempt will use the given
  // credentials.
  virtual void UpdateCredentials(const std::string& email,
      const std::string& token) OVERRIDE;
  virtual int GetInvalidationClientType() OVERRIDE;

  // notifier::PushClient::Observer implementation.
  virtual void OnNotificationsEnabled() OVERRIDE;
  virtual void OnNotificationsDisabled(
      notifier::NotificationsDisabledReason reason) OVERRIDE;
  virtual void OnIncomingNotification(
      const notifier::Notification& notification) OVERRIDE;

  const std::string& GetServiceContextForTest() const;

  int64 GetSchedulingHashForTest() const;

  static std::string EncodeMessageForTest(const std::string& message,
                                          const std::string& service_context,
                                          int64 scheduling_hash);

  static bool DecodeMessageForTest(const std::string& notification,
                                   std::string* message,
                                   std::string* service_context,
                                   int64* scheduling_hash);

 private:
  static void EncodeMessage(std::string* encoded_message,
                            const std::string& message,
                            const std::string& service_context,
                            int64 scheduling_hash);
  static bool DecodeMessage(const std::string& data,
                            std::string* message,
                            std::string* service_context,
                            int64* scheduling_hash);
  scoped_ptr<base::DictionaryValue> CollectDebugData() const;

  scoped_ptr<notifier::PushClient> push_client_;
  std::string service_context_;
  int64 scheduling_hash_;

  // This count is saved for displaying statatistics.
  int sent_messages_count_;

  DISALLOW_COPY_AND_ASSIGN(PushClientChannel);
};

}  // namespace syncer

#endif  // COMPONENTS_INVALIDATION_PUSH_CLIENT_CHANNEL_H_
