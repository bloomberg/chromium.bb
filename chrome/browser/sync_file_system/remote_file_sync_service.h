// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_REMOTE_FILE_SYNC_SERVICE_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_REMOTE_FILE_SYNC_SERVICE_H_

#include <string>

#include "base/basictypes.h"
#include "chrome/browser/sync_file_system/conflict_resolution_policy.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/syncable/sync_callbacks.h"

class GURL;

namespace sync_file_system {

class FileStatusObserver;
class LocalChangeProcessor;
class RemoteChangeProcessor;

enum RemoteServiceState {
  // Remote service is up and running, or has not seen any errors yet.
  // The consumer of this service can make new requests while the
  // service is in this state.
  REMOTE_SERVICE_OK,

  // Remote service is temporarily unavailable due to network,
  // authentication or some other temporary failure.
  // This state may be automatically resolved when the underlying
  // network condition or service condition changes.
  // The consumer of this service can still make new requests but
  // they may fail (with recoverable error code).
  REMOTE_SERVICE_TEMPORARY_UNAVAILABLE,

  // Remote service is temporarily unavailable due to authentication failure.
  // This state may be automatically resolved when the authentication token
  // has been refreshed internally (e.g. when the user signed in etc).
  // The consumer of this service can still make new requests but
  // they may fail (with recoverable error code).
  REMOTE_SERVICE_AUTHENTICATION_REQUIRED,

  // Remote service is disabled by configuration change or due to some
  // unrecoverable errors, e.g. local database corruption.
  // Any new requests will immediately fail when the service is in
  // this state.
  REMOTE_SERVICE_DISABLED,
};

// This class represents a backing service of the sync filesystem.
// This also maintains conflict information, i.e. a list of conflicting files
// (at least in the current design).
// Owned by SyncFileSystemService.
class RemoteFileSyncService {
 public:
  class Observer {
   public:
    Observer() {}
    virtual ~Observer() {}

    // This is called when RemoteFileSyncService updates its internal queue
    // of pending remote changes.
    // |pending_changes_hint| indicates the pending queue length to help sync
    // scheduling but the value may not be accurately reflect the real-time
    // value.
    virtual void OnRemoteChangeQueueUpdated(int64 pending_changes_hint) = 0;

    // This is called when RemoteFileSyncService updates its state.
    virtual void OnRemoteServiceStateUpdated(
        RemoteServiceState state,
        const std::string& description) {}

   private:
    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  RemoteFileSyncService() {}
  virtual ~RemoteFileSyncService() {}

  // Adds and removes observers.
  virtual void AddServiceObserver(Observer* observer) = 0;
  virtual void AddFileStatusObserver(FileStatusObserver* observer) = 0;

  // Registers |origin| to track remote side changes for the |origin|.
  // Upon completion, invokes |callback|.
  // The caller may call this method again when the remote service state
  // migrates to REMOTE_SERVICE_OK state if the error code returned via
  // |callback| was retriable ones.
  virtual void RegisterOriginForTrackingChanges(
      const GURL& origin,
      const SyncStatusCallback& callback) = 0;

  // Unregisters |origin| to track remote side changes for the |origin|.
  // Upon completion, invokes |callback|.
  // The caller may call this method again when the remote service state
  // migrates to REMOTE_SERVICE_OK state if the error code returned via
  // |callback| was retriable ones.
  virtual void UnregisterOriginForTrackingChanges(
      const GURL& origin,
      const SyncStatusCallback& callback) = 0;

  virtual void EnableOriginForTrackingChanges(
      const GURL& origin,
      const SyncStatusCallback& callback) = 0;

  virtual void DisableOriginForTrackingChanges(
      const GURL& origin,
      const SyncStatusCallback& callback) = 0;

  // Deletes the |origin| directory from the remote backing service.
  virtual void DeleteOriginDirectory(
      const GURL& origin,
      const SyncStatusCallback& callback) = 0;

  // Called by the sync engine to process one remote change.
  // After a change is processed |callback| will be called (to return
  // the control to the sync engine).
  // It is invalid to call this before calling SetRemoteChangeProcessor().
  virtual void ProcessRemoteChange(const SyncFileCallback& callback) = 0;

  // Sets a remote change processor.  This must be called before any
  // ProcessRemoteChange().
  virtual void SetRemoteChangeProcessor(
      RemoteChangeProcessor* processor) = 0;

  // Returns a LocalChangeProcessor that applies a local change to the remote
  // storage backed by this service.
  virtual LocalChangeProcessor* GetLocalChangeProcessor() = 0;

  // Returns true if the file |url| is marked conflicted in the remote service.
  virtual bool IsConflicting(const fileapi::FileSystemURL& url) = 0;

  // Returns the metadata of a remote file pointed by |url|.
  virtual void GetRemoteFileMetadata(
      const fileapi::FileSystemURL& url,
      const SyncFileMetadataCallback& callback) = 0;

  // Returns the current remote service state (should equal to the value
  // returned by the last OnRemoteServiceStateUpdated notification.
  virtual RemoteServiceState GetCurrentState() const = 0;

  // Returns the service name that backs this remote_file_sync_service.
  virtual const char* GetServiceName() const = 0;

  // Enables or disables the background sync.
  // Setting this to false should disable the synchronization (and make
  // the service state to REMOTE_SERVICE_DISABLED), while setting this to
  // true does not necessarily mean the service is actually turned on
  // (for example if Chrome is offline the service state will become
  // REMOTE_SERVICE_TEMPORARY_UNAVAILABLE).
  virtual void SetSyncEnabled(bool enabled) = 0;

  // Sets the conflict resolution policy. Returns SYNC_STATUS_OK on success,
  // or returns an error code if the given policy is not supported or had
  // an error.
  virtual SyncStatusCode SetConflictResolutionPolicy(
      ConflictResolutionPolicy policy) = 0;

  // Gets the conflict resolution policy.
  virtual ConflictResolutionPolicy GetConflictResolutionPolicy() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(RemoteFileSyncService);
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_REMOTE_FILE_SYNC_SERVICE_H_
