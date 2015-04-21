// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/view_manager/animation_runner.h"

#include "base/memory/scoped_vector.h"
#include "components/view_manager/animation_runner_observer.h"
#include "components/view_manager/scheduled_animation_group.h"
#include "components/view_manager/server_view.h"

namespace view_manager {
namespace {

bool ConvertViewAndAnimationPairToScheduledAnimationGroup(
    const std::vector<AnimationRunner::ViewAndAnimationPair>& views,
    AnimationRunner::AnimationId id,
    base::TimeTicks now,
    ScopedVector<ScheduledAnimationGroup>* groups) {
  for (const auto& view_animation_pair : views) {
    DCHECK(view_animation_pair.second);
    scoped_ptr<ScheduledAnimationGroup> group(ScheduledAnimationGroup::Create(
        view_animation_pair.first, now, id, *(view_animation_pair.second)));
    if (!group.get())
      return false;
    groups->push_back(group.release());
  }
  return true;
}

}  // namespace

AnimationRunner::AnimationRunner(base::TimeTicks now)
    : next_id_(1), last_tick_time_(now) {
}

AnimationRunner::~AnimationRunner() {
}

void AnimationRunner::AddObserver(AnimationRunnerObserver* observer) {
  observers_.AddObserver(observer);
}

void AnimationRunner::RemoveObserver(AnimationRunnerObserver* observer) {
  observers_.RemoveObserver(observer);
}

AnimationRunner::AnimationId AnimationRunner::Schedule(
    const std::vector<ViewAndAnimationPair>& views,
    base::TimeTicks now) {
  DCHECK_GE(now, last_tick_time_);

  const AnimationId animation_id = next_id_++;
  ScopedVector<ScheduledAnimationGroup> groups;
  if (!ConvertViewAndAnimationPairToScheduledAnimationGroup(views, animation_id,
                                                            now, &groups)) {
    return 0;
  }

  // Cancel any animations for the views.
  for (auto* group : groups) {
    ScheduledAnimationGroup* current_group =
        view_to_animation_map_.get(group->view());
    if (current_group)
      current_group->SetValuesToTargetValuesForPropertiesNotIn(*group);

    CancelAnimationForViewImpl(group->view(), CANCEL_SOURCE_SCHEDULE);
  }

  for (auto* group : groups) {
    group->ObtainStartValues();
    view_to_animation_map_.set(group->view(), make_scoped_ptr(group));
    DCHECK(!id_to_views_map_[animation_id].count(group->view()));
    id_to_views_map_[animation_id].insert(group->view());
  }
  // |view_to_animation_map_| owns the groups.
  groups.weak_clear();

  FOR_EACH_OBSERVER(AnimationRunnerObserver, observers_,
                    OnAnimationScheduled(animation_id));
  return animation_id;
}

void AnimationRunner::CancelAnimation(AnimationId id) {
  if (id_to_views_map_.count(id) == 0)
    return;

  std::set<ServerView*> views(id_to_views_map_[id]);
  for (ServerView* view : views)
    CancelAnimationForView(view);
}

void AnimationRunner::CancelAnimationForView(ServerView* view) {
  CancelAnimationForViewImpl(view, CANCEL_SOURCE_CANCEL);
}

void AnimationRunner::Tick(base::TimeTicks time) {
  DCHECK(time >= last_tick_time_);
  last_tick_time_ = time;
  if (view_to_animation_map_.empty())
    return;

  // The animation ids of any views whose animation completes are added here. We
  // notify after processing all views so that if an observer mutates us in some
  // way we're aren't left in a weird state.
  std::set<AnimationId> animations_completed;
  for (ViewToAnimationMap::iterator i = view_to_animation_map_.begin();
       i != view_to_animation_map_.end();) {
    if (i->second->Tick(time)) {
      const AnimationId animation_id = i->second->id();
      ServerView* view = i->first;
      ++i;
      if (RemoveViewFromMaps(view))
        animations_completed.insert(animation_id);
    } else {
      ++i;
    }
  }
  for (const AnimationId& id : animations_completed) {
    FOR_EACH_OBSERVER(AnimationRunnerObserver, observers_, OnAnimationDone(id));
  }
}

void AnimationRunner::CancelAnimationForViewImpl(ServerView* view,
                                                 CancelSource source) {
  if (!view_to_animation_map_.contains(view))
    return;

  const AnimationId animation_id = view_to_animation_map_.get(view)->id();
  if (RemoveViewFromMaps(view)) {
    // This was the last view in the group.
    if (source == CANCEL_SOURCE_CANCEL) {
      FOR_EACH_OBSERVER(AnimationRunnerObserver, observers_,
                        OnAnimationCanceled(animation_id));
    } else {
      FOR_EACH_OBSERVER(AnimationRunnerObserver, observers_,
                        OnAnimationInterrupted(animation_id));
    }
  }
}

bool AnimationRunner::RemoveViewFromMaps(ServerView* view) {
  DCHECK(view_to_animation_map_.contains(view));

  const AnimationId animation_id = view_to_animation_map_.get(view)->id();
  view_to_animation_map_.erase(view);

  DCHECK(id_to_views_map_.count(animation_id));
  id_to_views_map_[animation_id].erase(view);
  if (!id_to_views_map_[animation_id].empty())
    return false;

  id_to_views_map_.erase(animation_id);
  return true;
}

}  // namespace view_manager
