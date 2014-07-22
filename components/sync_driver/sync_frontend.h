// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_SYNC_FRONTEND_H_
#define COMPONENTS_SYNC_DRIVER_SYNC_FRONTEND_H_

#include "base/basictypes.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/sync_encryption_handler.h"
#include "sync/internal_api/public/sync_manager.h"
#include "sync/internal_api/public/util/weak_handle.h"
#include "sync/protocol/sync_protocol_error.h"

namespace syncer {
class DataTypeDebugInfoListener;
class JsBackend;
class ProtocolEvent;
struct CommitCounters;
struct StatusCounters;
struct UpdateCounters;
}  // namespace syncer

namespace sync_pb {
class EncryptedData;
}  // namespace sync_pb

namespace sync_driver {

// SyncFrontend is the interface used by SyncBackendHost to communicate with
// the entity that created it and, presumably, is interested in sync-related
// activity.
// NOTE: All methods will be invoked by a SyncBackendHost on the same thread
// used to create that SyncBackendHost.
class SyncFrontend {
 public:
  SyncFrontend();
  virtual ~SyncFrontend();

  // The backend has completed initialization and it is now ready to
  // accept and process changes.  If success is false, initialization
  // wasn't able to be completed and should be retried.
  //
  // |js_backend| is what about:sync interacts with; it's different
  // from the 'Backend' in 'OnBackendInitialized' (unfortunately).  It
  // is initialized only if |success| is true.
  virtual void OnBackendInitialized(
      const syncer::WeakHandle<syncer::JsBackend>& js_backend,
      const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>&
          debug_info_listener,
      const std::string& cache_guid,
      bool success) = 0;

  // The backend queried the server recently and received some updates.
  virtual void OnSyncCycleCompleted() = 0;

  // Configure ran into some kind of error. But it is scheduled to be
  // retried.
  virtual void OnSyncConfigureRetry() = 0;

  // Informs the frontned of some network event.  These notifications are
  // disabled by default and must be enabled through an explicit request to the
  // SyncBackendHost.
  //
  // It's disabld by default to avoid copying data across threads when no one
  // is listening for it.
  virtual void OnProtocolEvent(const syncer::ProtocolEvent& event) = 0;

  // Called when we receive an updated commit counter for a directory type.
  //
  // Disabled by default.  Enable by calling
  // EnableDirectoryTypeDebugInfoForwarding() on the backend.
  virtual void OnDirectoryTypeCommitCounterUpdated(
      syncer::ModelType type,
      const syncer::CommitCounters& counters) = 0;

  // Called when we receive an updated update counter for a directory type.
  //
  // Disabled by default.  Enable by calling
  // EnableDirectoryTypeDebugInfoForwarding() on the backend.
  virtual void OnDirectoryTypeUpdateCounterUpdated(
      syncer::ModelType type,
      const syncer::UpdateCounters& counters) = 0;

  // Called when we receive an updated status counter for a directory type.
  //
  // Disabled by default.  Enable by calling
  // EnableDirectoryTypeDebugInfoForwarding() on the backend.
  virtual void OnDirectoryTypeStatusCounterUpdated(
      syncer::ModelType type,
      const syncer::StatusCounters& counters) = 0;

  // The status of the connection to the sync server has changed.
  virtual void OnConnectionStatusChange(
      syncer::ConnectionStatus status) = 0;

  // The syncer requires a passphrase to decrypt sensitive updates. This is
  // called when the first sensitive data type is setup by the user and anytime
  // the passphrase is changed by another synced client. |reason| denotes why
  // the passphrase was required. |pending_keys| is a copy of the
  // cryptographer's pending keys to be passed on to the frontend in order to
  // be cached.
  virtual void OnPassphraseRequired(
      syncer::PassphraseRequiredReason reason,
      const sync_pb::EncryptedData& pending_keys) = 0;

  // Called when the passphrase provided by the user is
  // accepted. After this is called, updates to sensitive nodes are
  // encrypted using the accepted passphrase.
  virtual void OnPassphraseAccepted() = 0;

  // Called when the set of encrypted types or the encrypt everything
  // flag has been changed.  Note that encryption isn't complete until
  // the OnEncryptionComplete() notification has been sent (see
  // below).
  //
  // |encrypted_types| will always be a superset of
  // syncer::Cryptographer::SensitiveTypes().  If |encrypt_everything| is
  // true, |encrypted_types| will be the set of all known types.
  //
  // Until this function is called, observers can assume that the set
  // of encrypted types is syncer::Cryptographer::SensitiveTypes() and that
  // the encrypt everything flag is false.
  virtual void OnEncryptedTypesChanged(
      syncer::ModelTypeSet encrypted_types,
      bool encrypt_everything) = 0;

  // Called after we finish encrypting the current set of encrypted
  // types.
  virtual void OnEncryptionComplete() = 0;

  // Called to perform migration of |types|.
  virtual void OnMigrationNeededForTypes(syncer::ModelTypeSet types) = 0;

  // Inform the Frontend that new datatypes are available for registration.
  virtual void OnExperimentsChanged(
      const syncer::Experiments& experiments) = 0;

  // Called when the sync cycle returns there is an user actionable error.
  virtual void OnActionableError(const syncer::SyncProtocolError& error) = 0;
};

}  // namespace sync_driver

#endif  // COMPONENTS_SYNC_DRIVER_SYNC_FRONTEND_H_
