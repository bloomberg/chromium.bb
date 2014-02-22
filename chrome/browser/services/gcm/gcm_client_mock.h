// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SERVICES_GCM_GCM_CLIENT_MOCK_H_
#define CHROME_BROWSER_SERVICES_GCM_GCM_CLIENT_MOCK_H_

#include "base/compiler_specific.h"
#include "google_apis/gcm/gcm_client.h"

namespace gcm {

class GCMClientMock : public GCMClient {
 public:
  enum Status {
    NOT_READY,
    READY
  };

  enum ErrorSimulation {
    ALWAYS_SUCCEED,
    FORCE_ERROR
  };

  // |status| denotes if the fake client is ready or not at the beginning.
  // |error_simulation| denotes if we should simulate server error.
  GCMClientMock(Status status, ErrorSimulation error_simulation);
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
  virtual void CheckOut() OVERRIDE;
  virtual void Register(const std::string& app_id,
                        const std::string& cert,
                        const std::vector<std::string>& sender_ids) OVERRIDE;
  virtual void Unregister(const std::string& app_id) OVERRIDE;
  virtual void Send(const std::string& app_id,
                    const std::string& receiver_id,
                    const OutgoingMessage& message) OVERRIDE;
  virtual bool IsReady() const OVERRIDE;

  // Simulate receiving something from the server.
  // Called on UI thread.
  void ReceiveMessage(const std::string& app_id,
                      const IncomingMessage& message);
  void DeleteMessages(const std::string& app_id);

  // Can only transition from non-ready to ready.
  void SetReady();

  static std::string GetRegistrationIdFromSenderIds(
      const std::vector<std::string>& sender_ids);

 private:
  // Called on IO thread.
  void RegisterFinished(const std::string& app_id,
                        const std::string& registrion_id);
  void SendFinished(const std::string& app_id, const std::string& message_id);
  void MessageReceived(const std::string& app_id,
                       const IncomingMessage& message);
  void MessagesDeleted(const std::string& app_id);
  void MessageSendError(const std::string& app_id,
                        const std::string& message_id);
  void SetReadyOnIO();

  Delegate* delegate_;
  Status status_;
  ErrorSimulation error_simulation_;

  DISALLOW_COPY_AND_ASSIGN(GCMClientMock);
};

}  // namespace gcm

#endif  // CHROME_BROWSER_SERVICES_GCM_GCM_CLIENT_MOCK_H_
