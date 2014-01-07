// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GCM_ENGINE_GCM_STORE_IMPL_H_
#define GOOGLE_APIS_GCM_ENGINE_GCM_STORE_IMPL_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "google_apis/gcm/base/gcm_export.h"
#include "google_apis/gcm/engine/gcm_store.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace gcm {

// An implementation of GCM Store that uses LevelDB for persistence.
// It performs all blocking operations on the blocking task runner, and posts
// all callbacks to the thread on which the GCMStoreImpl is created.
class GCM_EXPORT GCMStoreImpl : public GCMStore {
 public:
  GCMStoreImpl(const base::FilePath& path,
               scoped_refptr<base::SequencedTaskRunner> blocking_task_runner);
  virtual ~GCMStoreImpl();

  // Load the directory and pass the initial state back to caller.
  virtual void Load(const LoadCallback& callback) OVERRIDE;

  // Clears the GCM store of all data and destroys any LevelDB files associated
  // with this store.
  // WARNING: this will permanently destroy any pending outgoing messages
  // and require the device to re-create credentials and serial number mapping
  // tables.
  virtual void Destroy(const UpdateCallback& callback) OVERRIDE;

  // Sets this device's messaging credentials.
  virtual void SetDeviceCredentials(uint64 device_android_id,
                                    uint64 device_security_token,
                                    const UpdateCallback& callback) OVERRIDE;

  // Unacknowledged incoming message handling.
  virtual void AddIncomingMessage(const std::string& persistent_id,
                                  const UpdateCallback& callback) OVERRIDE;
  virtual void RemoveIncomingMessage(const std::string& persistent_id,
                                     const UpdateCallback& callback) OVERRIDE;
  virtual void RemoveIncomingMessages(const PersistentIdList& persistent_ids,
                                      const UpdateCallback& callback) OVERRIDE;

  // Unacknowledged outgoing messages handling.
  // TODO(zea): implement per-app limits on the number of outgoing messages.
  virtual void AddOutgoingMessage(const std::string& persistent_id,
                                  const MCSMessage& message,
                                  const UpdateCallback& callback) OVERRIDE;
  virtual void RemoveOutgoingMessage(const std::string& persistent_id,
                                     const UpdateCallback& callback) OVERRIDE;
  virtual void RemoveOutgoingMessages(const PersistentIdList& persistent_ids,
                                      const UpdateCallback& callback) OVERRIDE;

  // User serial number handling.
  virtual void SetNextSerialNumber(int64 next_serial_number,
                                   const UpdateCallback& callback) OVERRIDE;
  virtual void AddUserSerialNumber(const std::string& username,
                                   int64 serial_number,
                                   const UpdateCallback& callback) OVERRIDE;
  virtual void RemoveUserSerialNumber(const std::string& username,
                                      const UpdateCallback& callback) OVERRIDE;

 private:
  class Backend;

  scoped_refptr<Backend> backend_;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(GCMStoreImpl);
};

}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_ENGINE_GCM_STORE_IMPL_H_
