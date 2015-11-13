// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_LIB_IN_FLIGHT_CHANGE_H_
#define COMPONENTS_MUS_PUBLIC_CPP_LIB_IN_FLIGHT_CHANGE_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/public/cpp/bindings/array.h"
#include "ui/gfx/geometry/rect.h"

namespace mus {

class Window;

enum class ChangeType {
  ADD_TRANSIENT_WINDOW,
  BOUNDS,
  PROPERTY,
  NEW_WINDOW,
  REMOVE_TRANSIENT_WINDOW_FROM_PARENT
};

// InFlightChange is used to track function calls to the server and take the
// appropriate action when the call fails, or the same property changes while
// waiting for the response. When a function is called on the server an
// InFlightChange is created. The function call is complete when
// OnChangeCompleted() is received from the server. The following may occur
// while waiting for a response:
// . A new value is encountered from the server. For example, the bounds of
//   a window is locally changed and while waiting for the ack
//   OnWindowBoundsChanged() is received.
//   When this happens SetRevertValueFrom() is invoked on the InFlightChange.
//   The expectation is SetRevertValueFrom() takes the value to revert from the
//   supplied change.
// . While waiting for the ack the property is again modified locally. When
//   this happens a new InFlightChange is created. Once the ack for the first
//   call is received, and the server rejected the change ChangeFailed() is
//   invoked on the first change followed by SetRevertValueFrom() on the second
//   InFlightChange. This allows the new change to update the value to revert
//   should the second call fail.
// . If the server responds that the call failed and there is not another
//   InFlightChange for the same window outstanding, then ChangeFailed() is
//   invoked followed by Revert(). The expectation is Revert() sets the
//   appropriate value back on the Window.
class InFlightChange {
 public:
  InFlightChange(Window* window, ChangeType type);
  virtual ~InFlightChange();

  Window* window() { return window_; }
  const Window* window() const { return window_; }
  ChangeType change_type() const { return change_type_; }

  // Returns true if the two changes are considered the same. This is only
  // invoked if the window id and ChangeType match.
  virtual bool Matches(const InFlightChange& change) const;

  // Called in two cases:
  // . When the corresponding property on the server changes. In this case
  //   |change| corresponds to the value from the server.
  // . When |change| unsuccesfully completes.
  virtual void SetRevertValueFrom(const InFlightChange& change) = 0;

  // The change failed. Normally you need not take any action here as Revert()
  // is called as appropriate.
  virtual void ChangeFailed();

  // The change failed and there is no pending change of the same type
  // outstanding, revert the value.
  virtual void Revert() = 0;

 private:
  Window* window_;
  const ChangeType change_type_;
};

class InFlightBoundsChange : public InFlightChange {
 public:
  InFlightBoundsChange(Window* window, const gfx::Rect& revert_bounds);

  // InFlightChange:
  void SetRevertValueFrom(const InFlightChange& change) override;
  void Revert() override;

 private:
  Window* window_;
  gfx::Rect revert_bounds_;

  DISALLOW_COPY_AND_ASSIGN(InFlightBoundsChange);
};

// Inflight change that crashes on failure. This is useful for changes that are
// expected to always complete.
class CrashInFlightChange : public InFlightChange {
 public:
  CrashInFlightChange(Window* window, ChangeType type);
  ~CrashInFlightChange() override;

  // InFlightChange:
  void SetRevertValueFrom(const InFlightChange& change) override;
  void ChangeFailed() override;
  void Revert() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CrashInFlightChange);
};

class InFlightPropertyChange : public InFlightChange {
 public:
  InFlightPropertyChange(Window* window,
                         const std::string& property_name,
                         const mojo::Array<uint8_t>& revert_value);
  ~InFlightPropertyChange() override;

  // InFlightChange:
  bool Matches(const InFlightChange& change) const override;
  void SetRevertValueFrom(const InFlightChange& change) override;
  void Revert() override;

 private:
  const std::string property_name_;
  mojo::Array<uint8_t> revert_value_;

  DISALLOW_COPY_AND_ASSIGN(InFlightPropertyChange);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_PUBLIC_CPP_LIB_IN_FLIGHT_CHANGE_H_
