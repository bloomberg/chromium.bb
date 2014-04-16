// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SERVICES_GCM_GCM_CLIENT_MOCK_H_
#define CHROME_BROWSER_SERVICES_GCM_GCM_CLIENT_MOCK_H_

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "google_apis/gcm/gcm_client.h"

namespace gcm {

class GCMClientMock : public GCMClient {
 public:
  enum Status {
    UNINITIALIZED,
    LOADED,
    STOPPED,
    CHECKED_OUT
  };

  enum LoadingDelay {
    NO_DELAY_LOADING,
    DELAY_LOADING,
  };

  // |loading_delay| denotes if the check-in should be delayed.
  explicit GCMClientMock(LoadingDelay loading_delay);
  virtual ~GCMClientMock();

  // Overridden from GCMClient:
  // Called on IO thread.
  virtual void Initialize(
      const checkin_proto::ChromeBuildProto& chrome_build_proto,
      const base::FilePath& store_path,
      const std::vector<std::string>& account_ids,
      const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner,
      const scoped_refptr<net::URLRequestContextGetter>&
          url_request_context_getter,
      Delegate* delegate) OVERRIDE;
  virtual void Load() OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void CheckOut() OVERRIDE;
  virtual void Register(const std::string& app_id,
                        const std::vector<std::string>& sender_ids) OVERRIDE;
  virtual void Unregister(const std::string& app_id) OVERRIDE;
  virtual void Send(const std::string& app_id,
                    const std::string& receiver_id,
                    const OutgoingMessage& message) OVERRIDE;
  virtual GCMStatistics GetStatistics() const OVERRIDE;

  // Initiate the loading that has been delayed.
  // Called on UI thread.
  void PerformDelayedLoading();

  // Simulate receiving something from the server.
  // Called on UI thread.
  void ReceiveMessage(const std::string& app_id,
                      const IncomingMessage& message);
  void DeleteMessages(const std::string& app_id);

  static std::string GetRegistrationIdFromSenderIds(
      const std::vector<std::string>& sender_ids);

  Status status() const { return status_; }

 private:
  // Called on IO thread.
  void DoLoading();
  void CheckinFinished();
  void RegisterFinished(const std::string& app_id,
                        const std::string& registrion_id);
  void UnregisterFinished(const std::string& app_id);
  void SendFinished(const std::string& app_id, const OutgoingMessage& message);
  void MessageReceived(const std::string& app_id,
                       const IncomingMessage& message);
  void MessagesDeleted(const std::string& app_id);
  void MessageSendError(const std::string& app_id,
                        const SendErrorDetails& send_error_details);

  Delegate* delegate_;
  Status status_;
  LoadingDelay loading_delay_;
  base::WeakPtrFactory<GCMClientMock> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GCMClientMock);
};

}  // namespace gcm

#endif  // CHROME_BROWSER_SERVICES_GCM_GCM_CLIENT_MOCK_H_
