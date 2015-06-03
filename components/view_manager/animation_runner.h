// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIEW_MANAGER_ANIMATION_RUNNER_H_
#define COMPONENTS_VIEW_MANAGER_ANIMATION_RUNNER_H_

#include <algorithm>
#include <map>
#include <set>
#include <vector>

#include "base/containers/scoped_ptr_hash_map.h"
#include "base/observer_list.h"
#include "base/time/time.h"

namespace mojo {
class AnimationGroup;
}

namespace view_manager {

class AnimationRunnerObserver;
class ScheduledAnimationGroup;
class ServerView;

// AnimationRunner is responsible for maintaing and running a set of animations.
// The animations are represented as a set of AnimationGroups. New animations
// are scheduled by way of Schedule(). A |view| may only have one animation
// running at a time. Schedule()ing a new animation for a view already animating
// implicitly cancels the current animation for the view. Animations progress
// by way of the Tick() function.
class AnimationRunner {
 public:
  using AnimationId = uint32_t;
  using ViewAndAnimationPair =
      std::pair<ServerView*, const mojo::AnimationGroup*>;

  explicit AnimationRunner(base::TimeTicks now);
  ~AnimationRunner();

  void AddObserver(AnimationRunnerObserver* observer);
  void RemoveObserver(AnimationRunnerObserver* observer);

  // Schedules animations. If any of the groups are not valid no animations are
  // scheuled and 0 is returned. If there is an existing animation in progress
  // for any of the views it is canceled and any properties that were animating
  // but are no longer animating are set to their target value.
  AnimationId Schedule(const std::vector<ViewAndAnimationPair>& views,
                       base::TimeTicks now);

  // Cancels an animation scheduled by an id that was previously returned from
  // Schedule().
  void CancelAnimation(AnimationId id);

  // Cancels the animation scheduled for |view|. Does nothing if there is no
  // animation scheduled for |view|. This does not change |view|. That is, any
  // in progress animations are stopped.
  void CancelAnimationForView(ServerView* view);

  // Advance the animations updating values appropriately.
  void Tick(base::TimeTicks time);

  // Returns true if there are animations currently scheduled.
  bool HasAnimations() const { return !view_to_animation_map_.empty(); }

  // Returns true if the animation identified by |id| is valid and animating.
  bool IsAnimating(AnimationId id) const {
    return id_to_views_map_.count(id) > 0;
  }

  // Returns the views that are currently animating for |id|. Returns an empty
  // set if |id| does not identify a valid animation.
  std::set<ServerView*> GetViewsAnimating(AnimationId id) {
    return IsAnimating(id) ? id_to_views_map_.find(id)->second
                           : std::set<ServerView*>();
  }

 private:
  enum CancelSource {
    // Cancel is the result of scheduling another animation for the view.
    CANCEL_SOURCE_SCHEDULE,

    // Cancel originates from an explicit call to cancel.
    CANCEL_SOURCE_CANCEL,
  };

  using ViewToAnimationMap =
      base::ScopedPtrHashMap<ServerView*, scoped_ptr<ScheduledAnimationGroup>>;
  using IdToViewsMap = std::map<AnimationId, std::set<ServerView*>>;

  void CancelAnimationForViewImpl(ServerView* view, CancelSource source);

  // Removes |view| from both |view_to_animation_map_| and |id_to_views_map_|.
  // Returns true if there are no more views animating with the animation id
  // the view is associated with.
  bool RemoveViewFromMaps(ServerView* view);

  AnimationId next_id_;

  base::TimeTicks last_tick_time_;

  base::ObserverList<AnimationRunnerObserver> observers_;

  ViewToAnimationMap view_to_animation_map_;

  IdToViewsMap id_to_views_map_;

  DISALLOW_COPY_AND_ASSIGN(AnimationRunner);
};

}  // namespace view_manager

#endif  // COMPONENTS_VIEW_MANAGER_ANIMATION_RUNNER_H_
