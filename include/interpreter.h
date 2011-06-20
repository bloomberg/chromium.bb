// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GESTURES_INTERPRETER_H__
#define GESTURES_INTERPRETER_H__

#include <vector>

#include "gestures/include/gestures.h"

// This is a collection of supporting structs and an interface for
// Interpreters.

struct HardwareState;

namespace gestures {

enum GestureKind {
  kUnknownKind = 0,
  kPointingKind,
  kScrollingKind
};

struct SpeculativeGesture {
  short first_actor, second_actor;
  GestureKind kind;
  Gesture gesture;
  short confidence;  // [0..100]
};

struct SpeculativeGestures {
 public:
  // Insertion:
  void Add(const SpeculativeGesture* gesture);
  void AddMovement(short first_actor,
                   short second_actor,
                   int dx,
                   int dy,
                   short confidence,
                   const struct HardwareState* hwstate);
  void Clear() { gestures_.clear(); }

  // Retrieval:
  const SpeculativeGesture* GetHighestConfidence() const;
  const SpeculativeGesture* GetOneActor(GestureKind kind, short actor) const;
  // The two actors may be passed in any order:
  const SpeculativeGesture* GetTwoActors(GestureKind kind,
                                         short actor1,
                                         short actor2) const;

 private:
  std::vector<SpeculativeGesture> gestures_;
};

// Interface for all interpreters:

class Interpreter {
 public:
  virtual ~Interpreter() {}
  virtual ustime_t Push(const HardwareState* hwstate) = 0;
  virtual const SpeculativeGestures* Back(ustime_t now) const = 0;
  virtual ustime_t Pop(ustime_t now) = 0;
};

}  // namespace gestures

#endif  // GESTURES_INTERPRETER_H__
