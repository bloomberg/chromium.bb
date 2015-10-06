// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_NON_BLOCKING_DATA_TYPE_CONTROLLER_H_
#define COMPONENTS_SYNC_DRIVER_NON_BLOCKING_DATA_TYPE_CONTROLLER_H_

#include "base/memory/ref_counted_delete_on_message_loop.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/sync_context_proxy.h"

namespace syncer_v2 {
struct ActivationContext;
class ModelTypeProcessorImpl;
}

namespace sync_driver_v2 {

// Lives on the UI thread and manages the interactions between many sync
// components.
//
// There are three main parts to this controller:
// - The SyncContextProxy, represening the sync thread.
// - The ModelTypeProcessor, representing the model type thread.
// - The user-set state for this type (prefs), which lives on the UI thread.
//
// The ModelTypeProcessor can exist in three different states.  Those
// states are:
// - Enabled: Changes are being synced.
// - Disconnected: Changes would be synced, but there is no connection between
//                 the sync thread and the model thread.
// - Disabled: Syncing is intentionally disabled.  The type proxy may clear any
//             of its local state associated with sync when this happens, since
//             this is expected to last a while.
//
// This controller is responsible for transitioning the type proxy into and out
// of these states.  It does this by posting tasks to the model type thread.
//
// The type proxy is enabled when the user has indicated a desire to sync this
// type, and the ModelTypeProcessor and SyncContextProxy are available.
//
// The type proxy is disconnected during initialization, or when either the
// NonBlockingDataTypeController or SyncContextProxy have not yet registered.
// It can also be disconnected later on if the sync backend becomes
// unavailable.
//
// The type proxy is disabled if the user has disabled sync for this type.  The
// signal indicating this state will be sent to the type proxy when the pref is
// first changed, or when the type proxy first connects to the controller.
//
// This class is structured using some state machine patterns.  It's a bit
// awkward at times, but this seems to be the clearest way to express the
// behaviors this class must implement.
class NonBlockingDataTypeController
    : public base::RefCountedDeleteOnMessageLoop<
          NonBlockingDataTypeController> {
 public:
  NonBlockingDataTypeController(
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_thread,
      syncer::ModelType type,
      bool is_preferred);

  // Connects the ModelTypeProcessor to this controller.
  //
  // There is no "undo" for this operation.  The NonBlockingDataTypeController
  // will only ever deal with a single type proxy.
  void InitializeType(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      const base::WeakPtr<syncer_v2::ModelTypeProcessorImpl>& type_processor);

  // Initialize the connection to the SyncContextProxy.
  //
  // This process may be reversed with ClearSyncContextProxy().
  void InitializeSyncContext(
      scoped_ptr<syncer_v2::SyncContextProxy> sync_context_proxy);

  // Disconnect from the current SyncContextProxy.
  void ClearSyncContext();

  // Sets the current preferred state.
  //
  // Represents a choice to sync or not sync this type.  This is expected to
  // last a long time, so local state may be deleted if this setting is toggled
  // to false.
  void SetIsPreferred(bool is_preferred);

 private:
  friend class base::RefCountedDeleteOnMessageLoop<
      NonBlockingDataTypeController>;
  friend class base::DeleteHelper<NonBlockingDataTypeController>;

  ~NonBlockingDataTypeController();

  enum TypeState { ENABLED, DISABLED, DISCONNECTED };

  // Figures out which signals need to be sent then send then sends them.
  void UpdateState();

  // Sends an enable signal to the ModelTypeProcessorImpl.
  void SendEnableSignal();

  // Sends a disable signal to the ModelTypeProcessorImpl.
  void SendDisableSignal();

  // Sends a disconnect signal to the ModelTypeProcessorImpl.
  void SendDisconnectSignal();

  // Returns true if this type should be synced.
  bool IsPreferred() const;

  // Returns true if this object has access to the ModelTypeProcessorImpl.
  bool IsSyncProxyConnected() const;

  // Returns true if this object has access to the SyncContextProxy.
  bool IsSyncBackendConnected() const;

  // Returns the state that the type sync proxy should be in.
  TypeState GetDesiredState() const;

  // Called on the model thread to start the processor. The processor is
  // expected to invoke OnProcessorStarted when it is ready to be activated
  // with the sycn backend.
  void StartProcessor();

  // Callback passed to the processor to be invoked when the processor has
  // started. This is called on the model thread.
  void OnProcessorStarted(
      /*syncer::SyncError error,*/
      scoped_ptr<syncer_v2::ActivationContext> activation_context);

  // Called on the model thread to stop the processor.
  void StopProcessor();

  // The ModelType we're controlling.  Kept mainly for debugging.
  const syncer::ModelType type_;

  // Returns the state that the type sync proxy is actually in, from this
  // class' point of view.
  //
  // This state is inferred based on the most recently sent signals, and is
  // intended to represent the state the sync proxy will be in by the time any
  // tasks we post to it now will be run.  Due to threading / queueing effects,
  // this may or may not be the actual state at this point in time.
  TypeState current_state_;

  // Whether or not the user wants to sync this type.
  bool is_preferred_;

  // The ModelTypeProcessorImpl and its associated thread.  May be NULL.
  scoped_refptr<base::SingleThreadTaskRunner> model_task_runner_;
  base::WeakPtr<syncer_v2::ModelTypeProcessorImpl> type_processor_;

  // The SyncContextProxy that connects to the current sync backend.  May be
  // NULL.
  scoped_ptr<syncer_v2::SyncContextProxy> sync_context_proxy_;

  DISALLOW_COPY_AND_ASSIGN(NonBlockingDataTypeController);
};

}  // namespace sync_driver_v2

#endif  // COMPONENTS_SYNC_DRIVER_NON_BLOCKING_DATA_TYPE_CONTROLLER_H_
