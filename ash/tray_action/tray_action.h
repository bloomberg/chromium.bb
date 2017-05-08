// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TRAY_ACTION_TRAY_ACTION_H_
#define ASH_TRAY_ACTION_TRAY_ACTION_H_

#include "ash/ash_export.h"
#include "ash/public/interfaces/tray_action.mojom.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "mojo/public/cpp/bindings/binding_set.h"

namespace ash {

class TrayActionObserver;

// Controller that ash can use to request a predefined set of actions to be
// performed by clients.
// The controller provides an interface to:
//  * Send a request to the client to handle an action.
//  * Observe the state of support for an action as reported by the current ash
//    client.
// Currently, only single action is supported - creating new note on the lock
// screen - Chrome handles this action by launching an app (if any) that is
// registered as a lock screen enabled action handler for the new note action.
class ASH_EXPORT TrayAction : public NON_EXPORTED_BASE(mojom::TrayAction) {
 public:
  TrayAction();
  ~TrayAction() override;

  void AddObserver(TrayActionObserver* observer);
  void RemoveObserver(TrayActionObserver* observer);

  void BindRequest(mojom::TrayActionRequest request);

  // Gets last known handler state for the lock screen note action.
  // It will return kNotAvailable if an action handler has not been set using
  // |SetClient|.
  mojom::TrayActionState GetLockScreenNoteState();

  // If the client is set, sends it a request to handle the lock screen note
  // action.
  void RequestNewLockScreenNote();

  // mojom::TrayAction:
  void SetClient(mojom::TrayActionClientPtr action_handler,
                 mojom::TrayActionState lock_screen_note_state) override;
  void UpdateLockScreenNoteState(mojom::TrayActionState state) override;

 private:
  // Notifies the observers that state for the lock screen note action has been
  // updated.
  void NotifyLockScreenNoteStateChanged();

  // Last known state for lock screen note action.
  mojom::TrayActionState lock_screen_note_state_ =
      mojom::TrayActionState::kNotAvailable;

  base::ObserverList<TrayActionObserver> observers_;

  mojo::BindingSet<mojom::TrayAction> bindings_;

  mojom::TrayActionClientPtr tray_action_client_;

  DISALLOW_COPY_AND_ASSIGN(TrayAction);
};

}  // namespace ash

#endif  // ASH_TRAY_ACTION_TRAY_ACTION_H_
