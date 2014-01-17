// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GCM_GCM_CLIENT_IMPL_H_
#define GOOGLE_APIS_GCM_GCM_CLIENT_IMPL_H_

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "google_apis/gcm/gcm_client.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace gcm {

class GCMStore;
class UserList;

class GCM_EXPORT GCMClientImpl : public GCMClient {
 public:
  GCMClientImpl();
  virtual ~GCMClientImpl();

  void Initialize(
      const base::FilePath& path,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner);

  // Overridden from GCMClient:
  virtual void SetUserDelegate(const std::string& username,
                               Delegate* delegate) OVERRIDE;
  virtual void CheckIn(const std::string& username) OVERRIDE;
  virtual void Register(const std::string& username,
                        const std::string& app_id,
                        const std::string& cert,
                        const std::vector<std::string>& sender_ids) OVERRIDE;
  virtual void Unregister(const std::string& username,
                          const std::string& app_id) OVERRIDE;
  virtual void Send(const std::string& username,
                    const std::string& app_id,
                    const std::string& receiver_id,
                    const OutgoingMessage& message) OVERRIDE;
  virtual bool IsLoading() const OVERRIDE;

 private:
  scoped_ptr<GCMStore> gcm_store_;
  scoped_ptr<UserList> user_list_;

  DISALLOW_COPY_AND_ASSIGN(GCMClientImpl);
};

}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_GCM_CLIENT_IMPL_H_
