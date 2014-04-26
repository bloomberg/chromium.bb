// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_NON_BLOCKING_DATA_TYPE_CONTROLLER_H_
#define CHROME_BROWSER_SYNC_NON_BLOCKING_DATA_TYPE_CONTROLLER_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/sync_core_proxy.h"

namespace syncer {
class NonBlockingTypeProcessor;
}

namespace browser_sync {

// Lives on the UI thread and manages the interactions between many sync
// components.
//
// There are three main parts to this controller:
// - The SyncCoreProxy, represening the sync thread.
// - The NonBlockingTypeProcessor, representing the model type thread.
// - The user-set state for this type (prefs), which lives on the UI thread.
//
// The NonBlockingSyncTypeProcessor can exist in three different states.  Those
// states are:
// - Enabled: Changes are being synced.
// - Disconnected: Changes would be synced, but there is no connection between
//                 the sync thread and the model thread.
// - Disabled: Syncing is intentionally disabled.  The processor may clear any
//             of its local state associated with sync when this happens, since
//             this is expected to last a while.
//
// This controller is responsible for transitioning the processor into and out
// of these states.  It does this by posting tasks to the model type thread.
//
// The processor is enabled when the user has indicated a desire to sync this
// type, and the NonBlockingTypeProcessor and SyncCoreProxy are available.
//
// The processor is disconnected during initialization, or when either the
// NonBlockingDataTypeController or SyncCoreProxy have not yet registered.  It
// can also be disconnected later on if the sync backend becomes unavailable.
//
// The processor is disabled if the user has disabled sync for this type.  The
// signal indicating this state will be sent to the processor when the pref is
// first changed, or when the processor first connects to the controller.
//
// This class is structured using some state machine patterns.  It's a bit
// awkward at times, but this seems to be the clearest way to express the
// behaviors this class must implement.
class NonBlockingDataTypeController {
 public:
  NonBlockingDataTypeController(syncer::ModelType type, bool is_preferred);
  ~NonBlockingDataTypeController();

  // Connects the NonBlockingTypeProcessor to this controller.
  //
  // There is no "undo" for this operation.  The NonBlockingDataTypeController
  // will only ever deal with a single processor.
  void InitializeProcessor(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      base::WeakPtr<syncer::NonBlockingTypeProcessor> processor);

  // Initialize the connection to the SyncCoreProxy.
  //
  // This process may be reversed with ClearSyncCoreProxy().
  void InitializeSyncCoreProxy(scoped_ptr<syncer::SyncCoreProxy> proxy);

  // Disconnect from the current SyncCoreProxy.
  void ClearSyncCoreProxy();

  // Sets the current preferred state.
  //
  // Represents a choice to sync or not sync this type.  This is expected to
  // last a long time, so local state may be deleted if this setting is toggled
  // to false.
  void SetIsPreferred(bool is_preferred);

 private:
  enum TypeProcessorState { ENABLED, DISABLED, DISCONNECTED };

  // Figures out which signals need to be sent then send then sends them.
  void UpdateState();

  // Sends an enable signal to the NonBlockingTypeProcessor.
  void SendEnableSignal();

  // Sends a disable signal to the NonBlockingTypeProcessor.
  void SendDisableSignal();

  // Sends a disconnect signal to the NonBlockingTypeProcessor.
  void SendDisconnectSignal();

  // Returns true if this type should be synced.
  bool IsPreferred() const;

  // Returns true if this object has access to the NonBlockingTypeProcessor.
  bool IsTypeProcessorConnected() const;

  // Returns true if this object has access to the SyncCoreProxy.
  bool IsSyncBackendConnected() const;

  // Returns the state that the processor *should* be in.
  TypeProcessorState GetDesiredState() const;

  // The ModelType we're controlling.  Kept mainly for debugging.
  const syncer::ModelType type_;

  // Returns the state that the processor is actually in, from this class'
  // point of view.
  //
  // This state is inferred based on the most recently sent signals, and is
  // intended to represent the state the processor will be in by the time any
  // tasks we post to it now will be run.  Due to threading / queueing effects,
  // this may or may not be the actual state at this point in time.
  TypeProcessorState current_state_;

  // Whether or not the user wants to sync this type.
  bool is_preferred_;

  // The NonBlockingTypeProcessor and its associated thread.  May be NULL.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::WeakPtr<syncer::NonBlockingTypeProcessor> processor_;

  // The SyncCoreProxy that connects to the current sync backend.  May be NULL.
  scoped_ptr<syncer::SyncCoreProxy> proxy_;

  DISALLOW_COPY_AND_ASSIGN(NonBlockingDataTypeController);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_NON_BLOCKING_DATA_TYPE_CONTROLLER_H_
