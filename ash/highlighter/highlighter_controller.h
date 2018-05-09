// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HIGHLIGHTER_HIGHLIGHTER_CONTROLLER_H_
#define ASH_HIGHLIGHTER_HIGHLIGHTER_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/components/fast_ink/fast_ink_pointer_controller.h"
#include "ash/public/interfaces/highlighter_controller.mojom.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace base {
class OneShotTimer;
}

namespace ash {

class HighlighterResultView;
class HighlighterView;

// Controller for the highlighter functionality.
// Enables/disables highlighter as well as receives points
// and passes them off to be rendered.
class ASH_EXPORT HighlighterController
    : public fast_ink::FastInkPointerController,
      public mojom::HighlighterController {
 public:
  // Interface for classes that wish to be notified with highlighter status.
  class Observer {
   public:
    // Called when highlighter enabled status changes.
    // TODO(warx): add a reason enum to distinguish the case of deselecting the
    // tool and done with a stylus selection.
    virtual void OnHighlighterEnabledChanged(bool enabled) {}

   protected:
    virtual ~Observer() = default;
  };

  HighlighterController();
  ~HighlighterController() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Set the callback to exit the highlighter mode. If |require_success| is
  // true, the callback will be called only after a successful gesture
  // recognition. If |require_success| is false, the callback will be  called
  // after the first complete gesture, regardless of the recognition result.
  void SetExitCallback(base::OnceClosure callback, bool require_success);

  // fast_ink::FastInkPointerController:
  void SetEnabled(bool enabled) override;

  void BindRequest(mojom::HighlighterControllerRequest request);

  // mojom::HighlighterController:
  void SetClient(mojom::HighlighterControllerClientPtr client) override;
  void ExitHighlighterMode() override;

 private:
  friend class HighlighterControllerTestApi;

  // fast_ink::FastInkPointerController:
  views::View* GetPointerView() const override;
  void CreatePointerView(base::TimeDelta presentation_delay,
                         aura::Window* root_window) override;
  void UpdatePointerView(ui::TouchEvent* event) override;
  void DestroyPointerView() override;
  bool CanStartNewGesture(ui::TouchEvent* event) override;

  // Performs gesture recognition, initiates appropriate visual effects,
  // notifies the observer if necessary.
  void RecognizeGesture();

  // Destroys |highlighter_view_|, if it exists.
  void DestroyHighlighterView();

  // Destroys |result_view_|, if it exists.
  void DestroyResultView();

  // Called when the mojo connection with the client is closed.
  void OnClientConnectionLost();

  // Calls and clears the mode exit callback, if it is set.
  void CallExitCallback();

  void FlushMojoForTesting();

  // |highlighter_view_| will only hold an instance when the highlighter is
  // enabled and activated (pressed or dragged) and until the fade out
  // animation is done.
  std::unique_ptr<HighlighterView> highlighter_view_;

  // |result_view_| will only hold an instance when the selection result
  // animation is in progress.
  std::unique_ptr<HighlighterResultView> result_view_;

  // Time of the session start (e.g. when the controller was enabled).
  base::TimeTicks session_start_;

  // Time of the previous gesture end, valid after the first gesture
  // within the session is complete.
  base::TimeTicks previous_gesture_end_;

  // Gesture counter withing a session.
  int gesture_counter_ = 0;

  // Recognized gesture counter withing a session.
  int recognized_gesture_counter_ = 0;

  // Not null while waiting for the next event to continue an interrupted
  // stroke.
  std::unique_ptr<base::OneShotTimer> interrupted_stroke_timer_;

  // The callback to exit the mode in the UI.
  base::OnceClosure exit_callback_;

  // If true, the mode is not exited until a valid selection is made.
  bool require_success_ = true;

  // Binding for mojom::HighlighterController interface.
  mojo::Binding<ash::mojom::HighlighterController> binding_;

  // Interface to highlighter controller client (chrome).
  mojom::HighlighterControllerClientPtr client_;

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<HighlighterController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(HighlighterController);
};

}  // namespace ash

#endif  // ASH_HIGHLIGHTER_HIGHLIGHTER_CONTROLLER_H_
