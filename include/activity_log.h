// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GESTURES_ACTIVITY_LOG_H_
#define GESTURES_ACTIVITY_LOG_H_

#include "gestures/include/gestures.h"

#include <string>

#include <base/values.h>
#include <gtest/gtest.h>  // For FRIEND_TEST

// This is a class that circularly buffers all incoming and outgoing activity
// so that end users can report issues and engineers can reproduce them.

namespace gestures {

class PropRegistry;

class ActivityLog {
  FRIEND_TEST(ActivityLogTest, SimpleTest);
  FRIEND_TEST(ActivityLogTest, WrapAroundTest);
  FRIEND_TEST(ActivityLogTest, VersionTest);
  FRIEND_TEST(LoggingFilterInterpreterTest, SimpleTest);
  FRIEND_TEST(PropRegistryTest, PropChangeTest);
 public:
  enum EntryType {
    kHardwareState = 0,
    kTimerCallback,
    kCallbackRequest,
    kGesture,
    kPropChange
  };
  struct PropChangeEntry {
    const char* name;
    enum {
      kBoolProp = 0,
      kDoubleProp,
      kIntProp,
      kShortProp
    } type;
    union {
      GesturesPropBool bool_val;
      double double_val;
      int int_val;
      short short_val;
      // No string because string values can't change
    } value;
  };
  struct Entry {
    EntryType type;
    struct details {
      HardwareState hwstate;  // kHardwareState
      stime_t timestamp;  // kTimerCallback, kCallbackRequest
      Gesture gesture;  // kGesture
      PropChangeEntry prop_change;  // kPropChange
    } details;
  };

  explicit ActivityLog(PropRegistry* prop_reg);
  void SetHardwareProperties(const HardwareProperties& hwprops);

  // Log*() functions record an argument into the buffer
  void LogHardwareState(const HardwareState& hwstate);
  void LogTimerCallback(stime_t now);
  void LogCallbackRequest(stime_t when);
  void LogGesture(const Gesture& gesture);
  void LogPropChange(const PropChangeEntry& prop_change);

  // Dump allocates, and thus must not be called on a signal handler.
  void Dump(const char* filename);
  void Clear() { head_idx_ = size_ = 0; }

  size_t size() const { return size_; }
  size_t MaxSize() const { return kBufferSize; }
  Entry* GetEntry(size_t idx) {
    return &buffer_[(head_idx_ + idx) % kBufferSize];
  }

  static const char kKeyRoot[];
  static const char kKeyType[];
  static const char kKeyHardwareState[];
  static const char kKeyTimerCallback[];
  static const char kKeyCallbackRequest[];
  static const char kKeyGesture[];
  static const char kKeyPropChange[];
  // HardwareState keys:
  static const char kKeyHardwareStateTimestamp[];
  static const char kKeyHardwareStateButtonsDown[];
  static const char kKeyHardwareStateTouchCnt[];
  static const char kKeyHardwareStateFingers[];
  // FingerState keys (part of HardwareState):
  static const char kKeyFingerStateTouchMajor[];
  static const char kKeyFingerStateTouchMinor[];
  static const char kKeyFingerStateWidthMajor[];
  static const char kKeyFingerStateWidthMinor[];
  static const char kKeyFingerStatePressure[];
  static const char kKeyFingerStateOrientation[];
  static const char kKeyFingerStatePositionX[];
  static const char kKeyFingerStatePositionY[];
  static const char kKeyFingerStateTrackingId[];
  static const char kKeyFingerStateFlags[];
  // TimerCallback keys:
  static const char kKeyTimerCallbackNow[];
  // CallbackRequest keys:
  static const char kKeyCallbackRequestWhen[];
  // Gesture keys:
  static const char kKeyGestureType[];
  static const char kValueGestureTypeContactInitiated[];
  static const char kValueGestureTypeMove[];
  static const char kValueGestureTypeScroll[];
  static const char kValueGestureTypePinch[];
  static const char kValueGestureTypeButtonsChange[];
  static const char kValueGestureTypeFling[];
  static const char kValueGestureTypeSwipe[];
  static const char kKeyGestureStartTime[];
  static const char kKeyGestureEndTime[];
  static const char kKeyGestureMoveDX[];
  static const char kKeyGestureMoveDY[];
  static const char kKeyGestureScrollDX[];
  static const char kKeyGestureScrollDY[];
  static const char kKeyGesturePinchDZ[];
  static const char kKeyGestureButtonsChangeDown[];
  static const char kKeyGestureButtonsChangeUp[];
  static const char kKeyGestureFlingVX[];
  static const char kKeyGestureFlingVY[];
  static const char kKeyGestureFlingState[];
  static const char kKeyGestureSwipeDX[];
  // PropChange keys:
  static const char kKeyPropChangeType[];
  static const char kKeyPropChangeName[];
  static const char kKeyPropChangeValue[];
  static const char kValuePropChangeTypeBool[];
  static const char kValuePropChangeTypeDouble[];
  static const char kValuePropChangeTypeInt[];
  static const char kValuePropChangeTypeShort[];

  // Hardware Properties keys:
  static const char kKeyHardwarePropRoot[];
  static const char kKeyHardwarePropLeft[];
  static const char kKeyHardwarePropTop[];
  static const char kKeyHardwarePropRight[];
  static const char kKeyHardwarePropBottom[];
  static const char kKeyHardwarePropXResolution[];
  static const char kKeyHardwarePropYResolution[];
  static const char kKeyHardwarePropXDpi[];
  static const char kKeyHardwarePropYDpi[];
  static const char kKeyHardwarePropMaxFingerCount[];
  static const char kKeyHardwarePropMaxTouchCount[];
  static const char kKeyHardwarePropSupportsT5R2[];
  static const char kKeyHardwarePropSemiMt[];
  static const char kKeyHardwarePropIsButtonPad[];

  static const char kKeyProperties[];

 private:
  // Extends the tail of the buffer by one element and returns that new element.
  // This may cause an older element to be overwritten if the buffer is full.
  Entry* PushBack();

  size_t TailIdx() const { return (head_idx_ + size_ - 1) % kBufferSize; }

  // JSON-encoders for various types
  ::Value* EncodeHardwareProperties() const;
  ::Value* EncodeHardwareState(const HardwareState& hwstate);
  ::Value* EncodeTimerCallback(stime_t timestamp);
  ::Value* EncodeCallbackRequest(stime_t timestamp);
  ::Value* EncodeGesture(const Gesture& gesture);
  ::Value* EncodePropChange(const PropChangeEntry& prop_change);

  // Encode user-configurable properties
  ::Value* EncodePropRegistry();

  // Returns a JSON string representing all the state in the buffer
  std::string Encode();

  static const size_t kBufferSize = 16384;

  Entry buffer_[kBufferSize];
  size_t head_idx_;
  size_t size_;

  // We allocate this to be number of entries * max fingers/entry, and
  // if buffer_[i] is a kHardwareState type, then the fingers for it are
  // at finger_states_[i * (max fingers/entry)].
  scoped_array<FingerState> finger_states_;
  size_t max_fingers_;

  HardwareProperties hwprops_;
  PropRegistry* prop_reg_;
};

}  // namespace gestures

#endif  // GESTURES_ACTIVITY_LOG_H_
