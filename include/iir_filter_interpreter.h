// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/scoped_ptr.h>

#include "gestures/include/gestures.h"
#include "gestures/include/immediate_interpreter.h"
#include "gestures/include/interpreter.h"
#include "gestures/include/map.h"
#include "gestures/include/prop_registry.h"

#ifndef GESTURES_IIR_FILTER_INTERPRETER_H_
#define GESTURES_IIR_FILTER_INTERPRETER_H_

namespace gestures {

// This filter interpreter applies a low-pass infinite impulse response (iir)
// filter to each incoming finger. The default filter is a low-pass 2nd order
// Butterworth IIR filter with a normalized cutoff frequency of 0.2. It can be
// configured via properties to use other formulae or
// different coefficients for the Butterworth filter.

class IirFilterInterpreter : public Interpreter, public PropertyDelegate {
 public:
  // We'll maintain one IOHistory record per active finger
  class IoHistory {
   public:
    IoHistory() : in_head(0), out_head(0) {}
    explicit IoHistory(const FingerState& fs) : in_head(0), out_head(0) {
      for (size_t i = 0; i < kInSize; i++)
        in[i] = fs;
      for (size_t i = 0; i < kOutSize; i++)
        out[i] = fs;
    }
    // Note: NextOut() and the oldest PrevOut() point to the same object.
    FingerState* NextOut() { return &out[NextOutHead()]; }
    FingerState* PrevOut(size_t idx) {
      return &out[(out_head + idx) % kOutSize];
    }
    // Note: NextIn() and the oldest PrevIn() point to the same object.
    FingerState* NextIn() { return &in[NextInHead()]; }
    FingerState* PrevIn(size_t idx) { return &in[(in_head + idx) % kInSize]; }
    void Increment();

    bool operator==(const IoHistory& that) const;
    bool operator!=(const IoHistory& that) const { return !(*this == that); }

   private:
    size_t NextOutHead() const {
      return (out_head + kOutSize - 1) % kOutSize;
    }
    size_t NextInHead() const {
      return (in_head + kInSize - 1) % kInSize;
    }

    static const size_t kInSize = 3;
    static const size_t kOutSize = 2;
    FingerState in[kInSize];  // previous input values
    size_t in_head;
    FingerState out[kOutSize];  // previous output values
    size_t out_head;
  };

  // Takes ownership of |next|:
  IirFilterInterpreter(PropRegistry* prop_reg, Interpreter* next);
  virtual ~IirFilterInterpreter() {}

  virtual Gesture* SyncInterpret(HardwareState* hwstate,
                                 stime_t* timeout);

  virtual Gesture* HandleTimer(stime_t now, stime_t* timeout);

  virtual void SetHardwareProperties(const HardwareProperties& hwprops);

  virtual void DoubleWasWritten(DoubleProperty* prop);

 private:
  scoped_ptr<Interpreter> next_;

  // y[0] = b[0]*x[0] + b[1]*x[1] + b[2]*x[2] + b[3]*x[3]
  //        - (a[1]*y[1] + a[2]*y[2])
  DoubleProperty b0_, b1_, b2_, b3_, a1_, a2_;

  map<short, IoHistory, kMaxFingers> histories_;
};

}  // namespace gestures

#endif  // GESTURES_IIR_FILTER_INTERPRETER_H_
