// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIEW_MANAGER_GESTURE_MANAGER_H_
#define COMPONENTS_VIEW_MANAGER_GESTURE_MANAGER_H_

#include <map>
#include <set>

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"

namespace mojo {
class Event;
}

namespace view_manager {

class GestureManagerDelegate;
class GestureManagerTest;
class ServerView;

struct GestureStateChange {
  GestureStateChange();
  ~GestureStateChange();

  uint32_t chosen_gesture;
  std::set<uint32_t> canceled_gestures;
};

using ChangeMap = std::map<const ServerView*, GestureStateChange>;

// GestureManager handles incoming pointer events. It determines the set of
// views (at most one per connection) that are interested in the event and
// informs the delegate at the appropriate time.
//
// Each pointer may have any number of views associated with it, and each view
// may have any number of gestures associated with it. Gesture are identified
// the by the pair of the connection id of the view and the client supplied
// gesture.  The same gesture may be used in multiple pointers (see example
// below).
//
// Gestures have the following states:
// . initial: Initial state for new gestures. From this state the gesture may
//   become chosen or canceled. Once a gesture moves out of this state it can
//   never go back.
// . chosen: the gesture has been chosen. From this state the gesture may be
//   canceled.
// . canceled: the gesture has been canceled.
// Gestures are active as long as they are included in the set of
// |possible_gesture_ids|. Gestures can be removed at any time by removing the
// gesture from |possible_gesture_ids|.
//
// A particular pointer has two distinct states:
// . initial: none of the gestures associated with the pointer have been
//   chosen.
// . chosen: when a gesture associated with the pointer has been chosen.
// Pointers are removed when a POINTER_UP or POINTER_CANCEL event is received.
//
// When a pointer is chosen all other gestures associated with the pointer are
// implicitly canceled. If the chosen gesture is canceled the pointer remains
// in the chosen state and no gestures can be chosen.
//
// Event propagation (ProcessEvent()) is handled in two distinct ways:
// . Until a gesture has been chosen for the pointer, views are notified in
//   order (deepest first). The next view (ancestor) is notified once
//   SetGestures() has been invoked for the previous (descendant) view.
// . Once a gesture has been chosen, then incoming events are immediately
//   dispatched.
//
// The following example highlights various states and transitions:
// . A POINTER_DOWN event is received for the pointer p1. The views that
//   contain the location of the event (starting with the deepest) are v1, v2,
//   v3 and v4. Both v1 and v2 have the property kViewManagerKeyWantsTouchEvents
//   set, so only v1 and v2 are considered. v1 is the deepest view, so the
//   touch event is set to it and only it first.
// . v1 responds with possible gestures g1 and g2. v1 does not specify either
//   of the gestures as chosen.
// . As the response from v1 has been received and there is no chosen gesture
//   the POINTER_DOWN event is sent to v2.
// . v2 responds with gestures g3 and g4, neither of which are chosen.
// . A POINTER_MOVE for p1 is received. As no gestures have been chosen event
//   of the POINTER_MOVE continues with v1 first.
// . v1 returns g1 and g2 as possible gestures and does not choose one.
// . The POINTER_MOVE is sent to v2.
// . v2 returns g3 and g4 as possible gestures and does not choose one.
// At this point p1 has the possible gestures g1, g2, g3, g4. Gestures g1 and
// g2 are associated with v1. Gestures g3 and g4 are associated with v2.
// . A POINTER_DOWN event is received for the pointer p2. v1 and v2 again
//   contain the location of the pointer. v1 is handed the event first.
// . A POINTER_MOVE event is received for the pointer p2. As the response from
//   v1 has not been received yet, the event is not sent yet (it is queued up).
// . v1 responds to the POINTER_DOWN for p2 with g1 and g2 and chooses g1.
// At this point g2, g3 and g4 are all canceled with g1 chosen. p2 is in the
// chosen state, as is p1 (p1 enters the chosen state as it contains the chosen
// gesture as well).
// . The POINTER_DOWN event for p2 is sent to v2. As p2 is in the chosen state
//   the POINTER_MOVE event that was queued up is sent to both v1 and v2 at the
//   same time (no waiting for response).
//
// TODO(sky): add some sort of timeout to deal with hung processes.
class GestureManager {
 public:
  using GestureAndConnectionId = uint64_t;

  static const uint32_t kInvalidGestureId;

  GestureManager(GestureManagerDelegate* delegate, const ServerView* root);
  ~GestureManager();

  // Processes the current event. See GestureManager description for details.
  bool ProcessEvent(const mojo::Event& event);

  // Sets the gestures for the specified view and pointer.
  scoped_ptr<ChangeMap> SetGestures(
      const ServerView* view,
      int32_t pointer_id,
      uint32_t chosen_gesture_id,
      const std::set<uint32_t>& possible_gesture_ids,
      const std::set<uint32_t>& canceled_gesture_ids);

 private:
  friend class GestureManagerTest;

  class Gesture;
  class Pointer;
  struct PointerAndView;
  class ScheduledDeleteProcessor;

  // Returns the Pointer for |pointer_id|, or null if one doesn't exist.
  Pointer* GetPointerById(int32_t pointer_id);

  // Notification that |pointer| has no gestures. This deletes |pointer|.
  void PointerHasNoGestures(Pointer* pointer);

  // Returns the Gesture for the specified arguments, creating if necessary.
  Gesture* GetGesture(const ServerView* view, uint32_t gesture_id);

  // Called from Pointer when a gesture is associated with a pointer.
  void AttachGesture(Gesture* gesture,
                     Pointer* pointer,
                     const ServerView* view);

  // Called from Pointer when a gesture is no longer associated with a
  // pointer.
  void DetachGesture(Gesture* gesture,
                     Pointer* pointer,
                     const ServerView* view);

  // Cancels the supplied gesture (if it isn't canceled already). Notifies all
  // pointers containing |gesture| that |gesture| has been canceled.
  void CancelGesture(Gesture* gesture,
                     Pointer* pointer,
                     const ServerView* view);

  // Chooses the supplied gesture. Notifies all pointers containing |gesture|
  // that |gesture| has been chosen.
  void ChooseGesture(Gesture* gesture,
                     Pointer* pointer,
                     const ServerView* view);

  // Deletes |pointer| after processing the current event. We delay deletion
  // until after the event as immediate deletion can cause problems for Pointer
  // (this is because the same Pointer may be on multiple frames of the stack).
  void ScheduleDelete(Pointer* pointer);

  GestureManagerDelegate* delegate_;
  const ServerView* root_view_;

  // Map for looking up gestures. Gestures are identified by the pair of
  // connection id and supplied gesture id.
  std::map<GestureAndConnectionId, Gesture*> gesture_map_;

  ScopedVector<Pointer> active_pointers_;

  // See comment in ScheduleDelete() for details.
  ScopedVector<Pointer> pointers_to_delete_;

  // Accumulates changes as the result of SetGestures().
  scoped_ptr<ChangeMap> current_change_;

  DISALLOW_COPY_AND_ASSIGN(GestureManager);
};

}  // namespace view_manager

#endif  // COMPONENTS_VIEW_MANAGER_GESTURE_MANAGER_H_
