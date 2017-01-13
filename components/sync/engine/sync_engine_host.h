// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_SYNC_ENGINE_HOST_H_
#define COMPONENTS_SYNC_ENGINE_SYNC_ENGINE_HOST_H_

#include <string>

#include "components/sync/base/model_type.h"
#include "components/sync/base/weak_handle.h"
#include "components/sync/engine/sync_encryption_handler.h"
#include "components/sync/engine/sync_manager.h"
#include "components/sync/protocol/sync_protocol_error.h"

namespace sync_pb {
class EncryptedData;
}  // namespace sync_pb

namespace syncer {

class DataTypeDebugInfoListener;
class JsBackend;
class ProtocolEvent;
struct CommitCounters;
struct StatusCounters;
struct UpdateCounters;

// SyncEngineHost is the interface used by SyncEngine to communicate with the
// entity that created it. It's essentially an observer interface except the
// SyncEngine always has exactly one.
class SyncEngineHost {
 public:
  SyncEngineHost();
  virtual ~SyncEngineHost();

  // The engine has completed initialization and it is now ready to accept and
  // process changes. If success is false, initialization wasn't able to be
  // completed and should be retried.
  //
  // |js_backend| is what about:sync interacts with. It is initialized only if
  // |success| is true.
  virtual void OnEngineInitialized(
      ModelTypeSet initial_types,
      const WeakHandle<JsBackend>& js_backend,
      const WeakHandle<DataTypeDebugInfoListener>& debug_info_listener,
      const std::string& cache_guid,
      bool success) = 0;

  // The engine queried the server recently and received some updates.
  virtual void OnSyncCycleCompleted(const SyncCycleSnapshot& snapshot) = 0;

  // Informs the host of some network event. These notifications are disabled by
  // default and must be enabled through an explicit request to the SyncEngine.
  //
  // It's disabled by default to avoid copying data across threads when no one
  // is listening for it.
  virtual void OnProtocolEvent(const ProtocolEvent& event) = 0;

  // Called when we receive an updated commit counter for a directory type.
  //
  // Disabled by default.  Enable by calling
  // EnableDirectoryTypeDebugInfoForwarding() on the engine.
  virtual void OnDirectoryTypeCommitCounterUpdated(
      ModelType type,
      const CommitCounters& counters) = 0;

  // Called when we receive an updated update counter for a directory type.
  //
  // Disabled by default.  Enable by calling
  // EnableDirectoryTypeDebugInfoForwarding() on the engine.
  virtual void OnDirectoryTypeUpdateCounterUpdated(
      ModelType type,
      const UpdateCounters& counters) = 0;

  // Called when we receive an updated status counter for a datatype.
  //
  // Disabled by default.  Enable by calling
  // EnableDirectoryTypeDebugInfoForwarding() on the engine.
  virtual void OnDatatypeStatusCounterUpdated(
      ModelType type,
      const StatusCounters& counters) = 0;

  // The status of the connection to the sync server has changed.
  virtual void OnConnectionStatusChange(ConnectionStatus status) = 0;

  // The syncer requires a passphrase to decrypt sensitive updates. This is
  // called when the first sensitive data type is setup by the user and anytime
  // the passphrase is changed by another synced client. |reason| denotes why
  // the passphrase was required. |pending_keys| is a copy of the
  // cryptographer's pending keys to be passed on to the host in order to
  // be cached.
  virtual void OnPassphraseRequired(
      PassphraseRequiredReason reason,
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
  // Cryptographer::SensitiveTypes().  If |encrypt_everything| is
  // true, |encrypted_types| will be the set of all known types.
  //
  // Until this function is called, observers can assume that the set
  // of encrypted types is Cryptographer::SensitiveTypes() and that
  // the encrypt everything flag is false.
  virtual void OnEncryptedTypesChanged(ModelTypeSet encrypted_types,
                                       bool encrypt_everything) = 0;

  // Called after we finish encrypting the current set of encrypted
  // types.
  virtual void OnEncryptionComplete() = 0;

  // Called to perform migration of |types|.
  virtual void OnMigrationNeededForTypes(ModelTypeSet types) = 0;

  // Called when new datatypes are available for registration.
  virtual void OnExperimentsChanged(const Experiments& experiments) = 0;

  // Called when the sync cycle returns there is an user actionable error.
  virtual void OnActionableError(const SyncProtocolError& error) = 0;

  // Called when the user of this device enables passphrase encryption.
  //
  // |nigori_state| contains the new (post custom passphrase) encryption keys
  // and can be used to restore SyncEncryptionHandler's state across sync
  // engine instances. See also SyncEncryptionHandlerImpl::RestoreNigori.
  virtual void OnLocalSetPassphraseEncryption(
      const SyncEncryptionHandler::NigoriState& nigori_state) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_SYNC_ENGINE_HOST_H_
