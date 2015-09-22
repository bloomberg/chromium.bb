// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/gesture_manager.h"

#include <algorithm>

#include "components/mus/gesture_manager_delegate.h"
#include "components/mus/public/cpp/keys.h"
#include "components/mus/server_view.h"
#include "components/mus/view_coordinate_conversions.h"
#include "components/mus/view_locator.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/mojo/events/input_events.mojom.h"

namespace mus {

using Views = std::vector<const ServerView*>;

namespace {

GestureManager::GestureAndConnectionId MakeGestureAndConnectionId(
    const ServerView* view,
    uint32_t gesture_id) {
  return (static_cast<GestureManager::GestureAndConnectionId>(
              view->id().connection_id)
          << 32) |
         gesture_id;
}

// Returns the views (deepest first) that should receive touch events. This only
// returns one view per connection. If multiple views from the same connection
// are interested in touch events the shallowest view is returned.
Views GetTouchTargets(const ServerView* deepest) {
  Views result;
  const ServerView* view = deepest;
  while (view) {
    if (view->properties().count(kViewManagerKeyWantsTouchEvents)) {
      if (!result.empty() &&
          result.back()->id().connection_id == view->id().connection_id) {
        result.pop_back();
      }
      result.push_back(view);
    }
    view = view->parent();
  }
  // TODO(sky): I'm doing this until things are converted. Seems as though we
  // shouldn't do this long term.
  if (result.empty())
    result.push_back(deepest);
  return result;
}

mojo::EventPtr CloneEventForView(const mojo::Event& event,
                                 const ServerView* view) {
  mojo::EventPtr result(event.Clone());
  if (event.pointer_data && event.pointer_data->location) {
    const gfx::PointF location(event.pointer_data->location->x,
                               event.pointer_data->location->y);
    const gfx::PointF target_location(
        ConvertPointFBetweenViews(view->GetRoot(), view, location));
    result->pointer_data->location->x = target_location.x();
    result->pointer_data->location->y = target_location.y();
  }
  return result.Pass();
}

}  // namespace

// GestureStateChange ----------------------------------------------------------

GestureStateChange::GestureStateChange()
    : chosen_gesture(GestureManager::kInvalidGestureId) {}

GestureStateChange::~GestureStateChange() {}

// ViewIterator ----------------------------------------------------------------

// Used to iterate over a set of views.
class ViewIterator {
 public:
  explicit ViewIterator(const Views& views)
      : views_(views), current_(views_.begin()) {}

  // Advances to the next view. Returns true if there are no more views (at
  // the end).
  bool advance() { return ++current_ != views_.end(); }

  bool at_end() const { return current_ == views_.end(); }

  bool empty() const { return views_.empty(); }

  const ServerView* current() const { return *current_; }

  void reset_to_beginning() { current_ = views_.begin(); }

  void remove(const ServerView* view) {
    Views::iterator iter = std::find(views_.begin(), views_.end(), view);
    DCHECK(iter != views_.end());
    if (iter == current_) {
      current_ = views_.erase(current_);
    } else if (!at_end()) {
      size_t index = current_ - views_.begin();
      if (current_ > iter)
        index--;
      views_.erase(iter);
      current_ = views_.begin() + index;
    } else {
      views_.erase(iter);
      current_ = views_.end();
    }
  }

  bool contains(const ServerView* view) const {
    return std::find(views_.begin(), views_.end(), view) != views_.end();
  }

 private:
  Views views_;
  Views::iterator current_;

  DISALLOW_COPY_AND_ASSIGN(ViewIterator);
};

// PointerAndView --------------------------------------------------------------

struct GestureManager::PointerAndView {
  PointerAndView();
  PointerAndView(Pointer* pointer, const ServerView* view);

  // Compares two PointerAndView instances based on pointer id, then view id.
  // This is really only interesting for unit tests so that they get a known
  // order of events.
  bool operator<(const PointerAndView& other) const;

  Pointer* pointer;
  const ServerView* view;
};

// Gesture ---------------------------------------------------------------------

// Gesture maintains the set of pointers and views it is attached to.
class GestureManager::Gesture {
 public:
  enum State { STATE_INITIAL, STATE_CANCELED, STATE_CHOSEN };

  explicit Gesture(uint32_t id);
  ~Gesture();

  uint32_t id() const { return id_; }

  void Attach(Pointer* pointer, const ServerView* view);
  void Detach(Pointer* pointer, const ServerView* view);

  void set_state(State state) { state_ = state; }
  State state() const { return state_; }

  const std::set<PointerAndView>& pointers_and_views() const {
    return pointers_and_views_;
  }

 private:
  const uint32_t id_;
  State state_;
  std::set<PointerAndView> pointers_and_views_;

  DISALLOW_COPY_AND_ASSIGN(Gesture);
};

GestureManager::Gesture::Gesture(uint32_t id)
    : id_(id), state_(STATE_INITIAL) {}

GestureManager::Gesture::~Gesture() {}

void GestureManager::Gesture::Attach(Pointer* pointer, const ServerView* view) {
  pointers_and_views_.insert(PointerAndView(pointer, view));
}

void GestureManager::Gesture::Detach(Pointer* pointer, const ServerView* view) {
  pointers_and_views_.erase(PointerAndView(pointer, view));
}

// Pointer ---------------------------------------------------------------------

// Pointer manages the state associated with a particular pointer from the time
// of the POINTER_DOWN to the time of the POINTER_UP (or POINTER_CANCEL). This
// state includes a mapping from view to the set of gestures the view is
// interested in. It also manages choosing gestures at the appropriate point as
// well as which view to dispatch to and the events to dispatch.
// See description in GestureManager for more.
class GestureManager::Pointer {
 public:
  Pointer(GestureManager* gesture_manager,
          int32_t pointer_id,
          const mojo::Event& event,
          const Views& views);
  ~Pointer();

  int32_t pointer_id() const { return pointer_id_; }
  bool was_chosen_or_canceled() const { return was_chosen_or_canceled_; }

  // Sets the set of gestures for this pointer.
  void SetGestures(const ServerView* view,
                   uint32_t chosen_gesture_id,
                   const std::set<uint32_t>& possible_gesture_ids,
                   const std::set<uint32_t>& canceled_ids);

  // Called when a Gesture we contain has been canceled.
  void GestureCanceled(Gesture* gesture);

  // Called when a Gesture we contain has been chosen.
  void GestureChosen(Gesture* gesture, const ServerView* view);

  // Process a move or up event. This may delay processing if we're waiting for
  // previous results.
  void ProcessEvent(const mojo::Event& event);

 private:
  // Corresponds to the type of event we're dispatching.
  enum Phase {
    // We're dispatching the initial down.
    PHASE_DOWN,

    // We're dispatching a move.
    PHASE_MOVE,
  };

  // Sends the event for the current phase to the delegate.
  void ForwardCurrentEvent();

  // Moves |pending_event_| to |current_event_| and notifies the delegate.
  void MovePendingToCurrentAndProcess();

  // If |was_chosen_or_canceled_| is false and there is only one possible
  // gesture and it is in the initial state, choose it. Otherwise do nothing.
  void ChooseGestureIfPossible();

  bool ScheduleDeleteIfNecessary();

  GestureManager* gesture_manager_;
  const int32_t pointer_id_;
  Phase phase_;

  // Used to iterate over the set of views that potentially have gestures.
  ViewIterator view_iter_;

  // Maps from the view to the set of possible gestures for the view.
  std::map<const ServerView*, std::set<Gesture*>> view_to_gestures_;

  Gesture* chosen_gesture_;

  bool was_chosen_or_canceled_;

  // The event we're processing. When initially created this is the supplied
  // down event. When in PHASE_MOVE this is a move event.
  mojo::EventPtr current_event_;

  // Incoming events (move or up) are added here while while waiting for
  // responses.
  mojo::EventPtr pending_event_;

  DISALLOW_COPY_AND_ASSIGN(Pointer);
};

GestureManager::Pointer::Pointer(GestureManager* gesture_manager,
                                 int32_t pointer_id,
                                 const mojo::Event& event,
                                 const Views& views)
    : gesture_manager_(gesture_manager),
      pointer_id_(pointer_id),
      phase_(PHASE_DOWN),
      view_iter_(views),
      chosen_gesture_(nullptr),
      was_chosen_or_canceled_(false),
      current_event_(event.Clone()) {
  ForwardCurrentEvent();
}

GestureManager::Pointer::~Pointer() {
  for (auto& pair : view_to_gestures_) {
    for (Gesture* gesture : pair.second)
      gesture_manager_->DetachGesture(gesture, this, pair.first);
  }
}

void GestureManager::Pointer::SetGestures(
    const ServerView* view,
    uint32_t chosen_gesture_id,
    const std::set<uint32_t>& possible_gesture_ids,
    const std::set<uint32_t>& canceled_gesture_ids) {
  if (!view_iter_.contains(view)) {
    // We don't know about this view.
    return;
  }

  // True if this is the view we're waiting for a response from.
  const bool was_waiting_on =
      (!was_chosen_or_canceled_ &&
       (!view_iter_.at_end() && view_iter_.current() == view));

  if (possible_gesture_ids.empty()) {
    // The view no longer wants to be notified.
    for (Gesture* gesture : view_to_gestures_[view])
      gesture_manager_->DetachGesture(gesture, this, view);
    view_to_gestures_.erase(view);
    view_iter_.remove(view);
    if (view_iter_.empty()) {
      gesture_manager_->PointerHasNoGestures(this);
      // WARNING: we've been deleted.
      return;
    }
  } else {
    if (was_waiting_on)
      view_iter_.advance();

    Gesture* to_choose = nullptr;
    std::set<Gesture*> gestures;
    for (auto gesture_id : possible_gesture_ids) {
      Gesture* gesture = gesture_manager_->GetGesture(view, gesture_id);
      gesture_manager_->AttachGesture(gesture, this, view);
      gestures.insert(gesture);
      if (gesture->state() == Gesture::STATE_CHOSEN &&
          !was_chosen_or_canceled_) {
        to_choose = gesture;
      }
    }

    // Give preference to the supplied |chosen_gesture_id|.
    if (!was_chosen_or_canceled_ && chosen_gesture_id != kInvalidGestureId) {
      Gesture* gesture = gesture_manager_->GetGesture(view, chosen_gesture_id);
      if (gesture->state() != Gesture::STATE_CANCELED)
        to_choose = gesture;

      DCHECK(possible_gesture_ids.count(gesture->id()));
      gesture_manager_->AttachGesture(gesture, this, view);
    }

    // Tell GestureManager of any Gestures we're no longer associated with.
    std::set<Gesture*> removed_gestures;
    std::set_difference(
        view_to_gestures_[view].begin(), view_to_gestures_[view].end(),
        gestures.begin(), gestures.end(),
        std::inserter(removed_gestures, removed_gestures.begin()));
    view_to_gestures_[view].swap(gestures);
    for (Gesture* gesture : removed_gestures)
      gesture_manager_->DetachGesture(gesture, this, view);

    if (chosen_gesture_ && removed_gestures.count(chosen_gesture_))
      chosen_gesture_ = nullptr;

    if (to_choose) {
      gesture_manager_->ChooseGesture(to_choose, this, view);
    } else {
      // Choosing a gesture implicitly cancels all other gestures. If we didn't
      // choose a gesture we need to update the state of any newly added
      // gestures.
      for (Gesture* gesture : gestures) {
        if (gesture != chosen_gesture_ &&
            (was_chosen_or_canceled_ ||
             canceled_gesture_ids.count(gesture->id()))) {
          gesture_manager_->CancelGesture(gesture, this, view);
        }
      }
    }
  }

  if (was_waiting_on && !was_chosen_or_canceled_) {
    if (view_iter_.at_end()) {
      if (ScheduleDeleteIfNecessary())
        return;
      // If we're got all the responses, check if there is only one valid
      // gesture.
      ChooseGestureIfPossible();
      if (!was_chosen_or_canceled_) {
        // There is more than one valid gesture and none chosen. Continue
        // synchronous dispatch of move events.
        phase_ = PHASE_MOVE;
        MovePendingToCurrentAndProcess();
      }
    } else {
      ForwardCurrentEvent();
    }
  } else if (!was_chosen_or_canceled_ && phase_ != PHASE_DOWN) {
    // We weren't waiting on this view but we're in the move phase. The set of
    // gestures may have changed such that we only have one valid gesture. Check
    // for that.
    ChooseGestureIfPossible();
  }
}

void GestureManager::Pointer::GestureCanceled(Gesture* gesture) {
  if (was_chosen_or_canceled_ && gesture == chosen_gesture_) {
    chosen_gesture_ = nullptr;
    // No need to cancel other gestures as they are already canceled by virtue
    // of us having been chosen.
  } else if (!was_chosen_or_canceled_ && phase_ == PHASE_MOVE) {
    ChooseGestureIfPossible();
  }
}

void GestureManager::Pointer::GestureChosen(Gesture* gesture,
                                            const ServerView* view) {
  DCHECK(!was_chosen_or_canceled_);
  was_chosen_or_canceled_ = true;
  chosen_gesture_ = gesture;
  for (auto& pair : view_to_gestures_) {
    for (Gesture* g : pair.second) {
      if (g != gesture)
        gesture_manager_->CancelGesture(g, this, pair.first);
    }
  }

  while (!view_iter_.at_end()) {
    ForwardCurrentEvent();
    view_iter_.advance();
  }
  if (ScheduleDeleteIfNecessary())
    return;
  phase_ = PHASE_MOVE;
  MovePendingToCurrentAndProcess();
}

void GestureManager::Pointer::ProcessEvent(const mojo::Event& event) {
  // |event| is either a move or up. In either case it has the new coordinates
  // and is safe to replace the existing one with.
  pending_event_ = event.Clone();
  if (was_chosen_or_canceled_) {
    MovePendingToCurrentAndProcess();
  } else if (view_iter_.at_end()) {
    view_iter_.reset_to_beginning();
    MovePendingToCurrentAndProcess();
  }
  // The else case is we are waiting on a response from a view before we
  // continue dispatching. When we get the response for the last view in the
  // stack we'll move pending to current and start dispatching it.
}

void GestureManager::Pointer::ForwardCurrentEvent() {
  DCHECK(!view_iter_.at_end());
  const ServerView* view = view_iter_.current();
  gesture_manager_->delegate_->ProcessEvent(
      view, CloneEventForView(*current_event_, view), was_chosen_or_canceled_);
}

void GestureManager::Pointer::MovePendingToCurrentAndProcess() {
  if (!pending_event_.get()) {
    current_event_ = nullptr;
    return;
  }
  current_event_ = pending_event_.Pass();
  view_iter_.reset_to_beginning();
  ForwardCurrentEvent();
  if (was_chosen_or_canceled_) {
    while (view_iter_.advance())
      ForwardCurrentEvent();
    if (ScheduleDeleteIfNecessary())
      return;
    current_event_ = nullptr;
  }
}

void GestureManager::Pointer::ChooseGestureIfPossible() {
  if (was_chosen_or_canceled_)
    return;

  Gesture* gesture_to_choose = nullptr;
  const ServerView* view = nullptr;
  for (auto& pair : view_to_gestures_) {
    for (Gesture* gesture : pair.second) {
      if (gesture->state() == Gesture::STATE_INITIAL) {
        if (gesture_to_choose)
          return;
        view = pair.first;
        gesture_to_choose = gesture;
      }
    }
  }
  if (view)
    gesture_manager_->ChooseGesture(gesture_to_choose, this, view);
}

bool GestureManager::Pointer::ScheduleDeleteIfNecessary() {
  if (current_event_ &&
      (current_event_->action == mojo::EVENT_TYPE_POINTER_UP ||
       current_event_->action == mojo::EVENT_TYPE_POINTER_CANCEL)) {
    gesture_manager_->ScheduleDelete(this);
    return true;
  }
  return false;
}

// ScheduledDeleteProcessor ---------------------------------------------------

class GestureManager::ScheduledDeleteProcessor {
 public:
  explicit ScheduledDeleteProcessor(GestureManager* manager)
      : manager_(manager) {}

  ~ScheduledDeleteProcessor() { manager_->pointers_to_delete_.clear(); }

 private:
  GestureManager* manager_;

  DISALLOW_COPY_AND_ASSIGN(ScheduledDeleteProcessor);
};

// PointerAndView --------------------------------------------------------------

GestureManager::PointerAndView::PointerAndView()
    : pointer(nullptr), view(nullptr) {}

GestureManager::PointerAndView::PointerAndView(Pointer* pointer,
                                               const ServerView* view)
    : pointer(pointer), view(view) {}

bool GestureManager::PointerAndView::operator<(
    const PointerAndView& other) const {
  if (other.pointer->pointer_id() == pointer->pointer_id())
    return view->id().connection_id < other.view->id().connection_id;
  return pointer->pointer_id() < other.pointer->pointer_id();
}

// GestureManager --------------------------------------------------------------

// static
const uint32_t GestureManager::kInvalidGestureId = 0u;

GestureManager::GestureManager(GestureManagerDelegate* delegate,
                               const ServerView* root)
    : delegate_(delegate), root_view_(root) {}

GestureManager::~GestureManager() {
  // Explicitly delete the pointers first as this may result in calling back to
  // us to cleanup and delete gestures.
  active_pointers_.clear();
}

bool GestureManager::ProcessEvent(const mojo::Event& event) {
  if (!event.pointer_data || !event.pointer_data->location)
    return false;

  ScheduledDeleteProcessor delete_processor(this);
  const gfx::Point location(static_cast<int>(event.pointer_data->location->x),
                            static_cast<int>(event.pointer_data->location->y));
  switch (event.action) {
    case mojo::EVENT_TYPE_POINTER_DOWN: {
      if (GetPointerById(event.pointer_data->pointer_id)) {
        DVLOG(1) << "received more than one down for a pointer without a "
                 << "corresponding up, id=" << event.pointer_data->pointer_id;
        NOTREACHED();
        return true;
      }

      const ServerView* deepest = FindDeepestVisibleView(root_view_, location);
      Views targets(GetTouchTargets(deepest));
      if (targets.empty())
        return true;

      scoped_ptr<Pointer> pointer(
          new Pointer(this, event.pointer_data->pointer_id, event, targets));
      active_pointers_.push_back(pointer.Pass());
      return true;
    }

    case mojo::EVENT_TYPE_POINTER_CANCEL:
    case mojo::EVENT_TYPE_POINTER_MOVE:
    case mojo::EVENT_TYPE_POINTER_UP: {
      Pointer* pointer = GetPointerById(event.pointer_data->pointer_id);
      // We delete a pointer when it has no gestures, so it's possible to get
      // here with no gestures. Additionally there is no need to explicitly
      // delete |pointer| as it'll tell us when it's ready to be deleted.
      if (pointer)
        pointer->ProcessEvent(event);
      return true;
    }

    default:
      break;
  }
  return false;
}

scoped_ptr<ChangeMap> GestureManager::SetGestures(
    const ServerView* view,
    int32_t pointer_id,
    uint32_t chosen_gesture_id,
    const std::set<uint32_t>& possible_gesture_ids,
    const std::set<uint32_t>& canceled_gesture_ids) {
  // TODO(sky): caller should validate ids and make sure possible contains
  // canceled and chosen.
  DCHECK(!canceled_gesture_ids.count(kInvalidGestureId));
  DCHECK(!possible_gesture_ids.count(kInvalidGestureId));
  DCHECK(chosen_gesture_id == kInvalidGestureId ||
         possible_gesture_ids.count(chosen_gesture_id));
  DCHECK(chosen_gesture_id == kInvalidGestureId ||
         !canceled_gesture_ids.count(chosen_gesture_id));
  ScheduledDeleteProcessor delete_processor(this);
  Pointer* pointer = GetPointerById(pointer_id);
  current_change_.reset(new ChangeMap);
  if (pointer) {
    pointer->SetGestures(view, chosen_gesture_id, possible_gesture_ids,
                         canceled_gesture_ids);
  }
  return current_change_.Pass();
}

GestureManager::Pointer* GestureManager::GetPointerById(int32_t pointer_id) {
  for (Pointer* pointer : active_pointers_) {
    if (pointer->pointer_id() == pointer_id)
      return pointer;
  }
  return nullptr;
}

void GestureManager::PointerHasNoGestures(Pointer* pointer) {
  auto iter =
      std::find(active_pointers_.begin(), active_pointers_.end(), pointer);
  CHECK(iter != active_pointers_.end());
  active_pointers_.erase(iter);
}

GestureManager::Gesture* GestureManager::GetGesture(const ServerView* view,
                                                    uint32_t gesture_id) {
  GestureAndConnectionId gesture_and_connection_id =
      MakeGestureAndConnectionId(view, gesture_id);
  Gesture* gesture = gesture_map_[gesture_and_connection_id];
  if (!gesture) {
    gesture = new Gesture(gesture_id);
    gesture_map_[gesture_and_connection_id] = gesture;
  }
  return gesture;
}

void GestureManager::AttachGesture(Gesture* gesture,
                                   Pointer* pointer,
                                   const ServerView* view) {
  gesture->Attach(pointer, view);
}

void GestureManager::DetachGesture(Gesture* gesture,
                                   Pointer* pointer,
                                   const ServerView* view) {
  gesture->Detach(pointer, view);
  if (gesture->pointers_and_views().empty()) {
    gesture_map_.erase(MakeGestureAndConnectionId(view, gesture->id()));
    delete gesture;
  }
}

void GestureManager::CancelGesture(Gesture* gesture,
                                   Pointer* pointer,
                                   const ServerView* view) {
  if (gesture->state() == Gesture::STATE_CANCELED)
    return;

  gesture->set_state(Gesture::STATE_CANCELED);
  for (auto& pointer_and_view : gesture->pointers_and_views()) {
    (*current_change_)[pointer_and_view.view].canceled_gestures.insert(
        gesture->id());
    if (pointer_and_view.pointer != pointer)
      pointer_and_view.pointer->GestureCanceled(gesture);
  }
}

void GestureManager::ChooseGesture(Gesture* gesture,
                                   Pointer* pointer,
                                   const ServerView* view) {
  if (gesture->state() == Gesture::STATE_CHOSEN) {
    // This happens when |pointer| is supplied a gesture that is already
    // chosen.
    DCHECK((*current_change_)[view].chosen_gesture == kInvalidGestureId ||
           (*current_change_)[view].chosen_gesture == gesture->id());
    (*current_change_)[view].chosen_gesture = gesture->id();
    pointer->GestureChosen(gesture, view);
  } else {
    gesture->set_state(Gesture::STATE_CHOSEN);
    for (auto& pointer_and_view : gesture->pointers_and_views()) {
      DCHECK((*current_change_)[pointer_and_view.view].chosen_gesture ==
                 kInvalidGestureId ||
             (*current_change_)[pointer_and_view.view].chosen_gesture ==
                 gesture->id());
      (*current_change_)[pointer_and_view.view].chosen_gesture = gesture->id();
      pointer_and_view.pointer->GestureChosen(gesture, view);
    }
  }
}

void GestureManager::ScheduleDelete(Pointer* pointer) {
  auto iter =
      std::find(active_pointers_.begin(), active_pointers_.end(), pointer);
  if (iter != active_pointers_.end()) {
    active_pointers_.weak_erase(iter);
    pointers_to_delete_.push_back(pointer);
  }
}

}  // namespace mus
