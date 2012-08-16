// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/memory/scoped_ptr.h>
#include <gtest/gtest.h>  // for FRIEND_TEST

#include "gestures/include/filter_interpreter.h"
#include "gestures/include/finger_metrics.h"
#include "gestures/include/gestures.h"
#include "gestures/include/map.h"
#include "gestures/include/set.h"
#include "gestures/include/tracer.h"

#ifndef GESTURES_PALM_CLASSIFYING_FILTER_INTERPRETER_H_
#define GESTURES_PALM_CLASSIFYING_FILTER_INTERPRETER_H_

namespace gestures {

// This interpreter looks at the location and pressure of contacts and may
// optionally classify each as a palm. Sometimes a contact is classified as
// a palm in some frames, and then in subsequent frames it is not.

class PalmClassifyingFilterInterpreter : public FilterInterpreter {
  FRIEND_TEST(PalmClassifyingFilterInterpreterTest, PalmAtEdgeTest);
  FRIEND_TEST(PalmClassifyingFilterInterpreterTest, PalmReevaluateTest);
  FRIEND_TEST(PalmClassifyingFilterInterpreterTest, PalmTest);
  FRIEND_TEST(PalmClassifyingFilterInterpreterTest, StationaryPalmTest);
 public:
  // Takes ownership of |next|:
  PalmClassifyingFilterInterpreter(PropRegistry* prop_reg, Interpreter* next,
                                   FingerMetrics* finger_metrics,
                                   Tracer* tracer);
  virtual ~PalmClassifyingFilterInterpreter() {}

 protected:
  virtual Gesture* SyncInterpretImpl(HardwareState* hwstate,
                                     stime_t* timeout);

  virtual void SetHardwarePropertiesImpl(const HardwareProperties& hwprops);

 private:
  void FillOriginInfo(const HardwareState& hwstate);

  // Part of palm detection. Returns true if the finger indicated by
  // |finger_idx| is near another finger, which must not be a palm, in the
  // hwstate.
  bool FingerNearOtherFinger(const HardwareState& hwstate,
                             size_t finger_idx);

  // Returns true iff fs represents a contact that may be a palm. It's a palm
  // if it's in the edge of the pad with sufficiently large pressure. The
  // pressure required depends on exactly how close to the edge the contact is.
  bool FingerInPalmEnvelope(const FingerState& fs);

  // Updates *palm_, pointing_ below.
  void UpdatePalmState(const HardwareState& hwstate);

  // Updates the hwstate based on the local state.
  void UpdatePalmFlags(HardwareState* hwstate);

  // Returns the length of time the contact has been on the pad. Returns -1
  // on error.
  stime_t FingerAge(short finger_id, stime_t now) const;

  // Time when a contact arrived. Persists even when fingers change.
  map<short, stime_t, kMaxFingers> origin_timestamps_;
  // FingerStates from when a contact first arrived. Persists even when fingers
  // change.
  map<short, FingerState, kMaxFingers> origin_fingerstates_;

  // Same fingers state. This state is accumulated as fingers remain the same
  // and it's reset when fingers change.
  set<short, kMaxFingers> palm_;  // tracking ids of known palms
  // These contacts have moved significantly and shouldn't be considered
  // stationary palms:
  set<short, kMaxFingers> non_stationary_palm_;
  // tracking ids of known fingers that are not palms.
  set<short, kMaxFingers> pointing_;

  FingerMetrics* finger_metrics_;
  scoped_ptr<FingerMetrics> test_finger_metrics_;

  // Previously input timestamp
  stime_t prev_time_;

  HardwareProperties hw_props_;

  // Maximum pressure above which a finger is considered a palm
  DoubleProperty palm_pressure_;
  // Maximum width_major above which a finger is considered a palm
  DoubleProperty palm_width_;
  // The smaller of two widths around the edge for palm detection. Any contact
  // in this edge zone may be a palm, regardless of pressure
  DoubleProperty palm_edge_min_width_;
  // The larger of the two widths. Palms between this and the previous are
  // expected to have pressure linearly increase from 0 to palm_pressure_
  // as they approach this border.
  DoubleProperty palm_edge_width_;
  // Palms in edge are allowed to point if they move fast enough
  DoubleProperty palm_edge_point_speed_;
  // A finger can be added to the palm envelope (and thus not point) after
  // it touches down and until palm_eval_timeout_ [s] time.
  DoubleProperty palm_eval_timeout_;
  // A potential palm (ambiguous contact in the edge of the pad) will be marked
  // a palm if it travels less than palm_stationary_distance_ mm after it's
  // been on the pad for palm_stationary_time_ seconds.
  DoubleProperty palm_stationary_time_;
  DoubleProperty palm_stationary_distance_;

  DISALLOW_COPY_AND_ASSIGN(PalmClassifyingFilterInterpreter);
};

}  // namespace gestures

#endif  // GESTURES_PALM_CLASSIFYING_FILTER_INTERPRETER_H_
