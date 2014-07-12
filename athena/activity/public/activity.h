// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_ACTIVITY_PUBLIC_ACTIVITY_H_
#define ATHENA_ACTIVITY_PUBLIC_ACTIVITY_H_

#include <string>

#include "athena/athena_export.h"

namespace athena {

class ActivityViewModel;

// This class is a high level abstraction of an activity (which could be either
// a web page or a V1/V2 app/extension). Through this class the activity can
// be controlled (e.g. loaded / unloaded).
// An Activity gets created with state |ACTIVITY_UNLOADED|.
// Requesting |ACTIVITY_VISIBLE| or |ACTIVITY_INVISIBLE| will load it.
// Once an activity was |ACTIVITY_INVISIBLE| for a while it can be transitioned
// into |ACTIVITY_BACKGROUND_LOW_PRIORITY| to surrender more resources. After
// more time it can be transitions to |ACTIVITY_PERSISTENT| in which it only
// has it's runtime state left. At any time it can be transitioned back to one
// of the higher levels or unloaded via |ACTIVITY_UNLOADED|.
// Note that the resource manager will also query the media state before
// deciding if an activity can put into a lower state then |ACTIVITY_INVISIBLE|.
class ATHENA_EXPORT Activity {
 public:
  // The state of an activity which could either be set or requested by e.g. the
  // resource management system.
  enum ActivityState {
    // The activity is allowed to have gpu compositor layers and can be visible.
    ACTIVITY_VISIBLE,
    // The activity does not have gpu compositing layers, will not be visible
    // and will be treated as a background priority task.
    ACTIVITY_INVISIBLE,
    // The activity should surrender additional resources. This has only an
    // effect when the activity is in a loaded state (Visible, Active, Hidden).
    ACTIVITY_BACKGROUND_LOW_PRIORITY,
    // The activity will only keep a minimum set of resources to get back to the
    // running state. It will get stalled however. Note that it is not possible
    // to get into this state from the |ACTIVITY_UNLOADED| state.
    ACTIVITY_PERSISTENT,
    // Unloads the activity and can be called in any state - but unloaded.
    ACTIVITY_UNLOADED
  };

  // This enum declares the media state the activity is in.
  // TODO(skuhne): Move the |TabMediaState| out of chrome and combine it in a
  // media library within content and then use that enum instead.
  enum ActivityMediaState {
    ACTIVITY_MEDIA_STATE_NONE,
    ACTIVITY_MEDIA_STATE_RECORDING,  // Audio/Video being recorded by activity.
    ACTIVITY_MEDIA_STATE_CAPTURING,  // Activity is being captured.
    ACTIVITY_MEDIA_STATE_AUDIO_PLAYING  // Audible audio is playing in activity.
  };

  virtual ~Activity();

  // The Activity retains ownership of the returned view-model.
  virtual ActivityViewModel* GetActivityViewModel() = 0;

  // Transition the activity into a new state.
  virtual void SetCurrentState(ActivityState state) = 0;

  // Returns the current state of the activity.
  virtual ActivityState GetCurrentState() = 0;

  // Returns if the activity is visible or not.
  virtual bool IsVisible() = 0;

  // Returns the current media state.
  virtual ActivityMediaState GetMediaState() = 0;
};

}  // namespace athena

#endif  // ATHENA_ACTIVITY_PUBLIC_ACTIVITY_H_
