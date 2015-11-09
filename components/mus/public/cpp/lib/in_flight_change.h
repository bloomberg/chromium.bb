// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_LIB_IN_FLIGHT_CHANGE_H_
#define COMPONENTS_MUS_PUBLIC_CPP_LIB_IN_FLIGHT_CHANGE_H_

#include "base/macros.h"
#include "components/mus/public/cpp/types.h"
#include "ui/gfx/geometry/rect.h"

namespace mus {

class WindowTreeConnection;

enum class ChangeType {
  BOUNDS,
};

// InFlightChange is used to track function calls to the server and take the
// appropriate action when the call fails, or the same property changes while
// waiting for the response. When a function is called on the server an
// InFlightChange is created. The function call is complete when
// OnChangeCompleted() is sent back from the server. The following may occur
// while waiting for a response:
// . A new value is encountered from the server. For example, the bounds of
//   a window is locally changed and while waiting for the ack from the server
//   OnWindowBoundsChanged() is received.
//   When this occurs the new value from the server is set as the value to
//   revert should the server respond that the call failed.
// . While waiting for the ack the property is again modified locally. When
//   this happens a new InFlightChange is created. Once the ack for the first
//   call is received PreviousChangeCompleted() is invoked with the result from
//   the server on the second InFlightChange. This allows the new change to
//   update the value to revert should the second call fail.
// . If the server responds that the call failed and there is not another
//   InFlightChange for the same window outstanding, then Revert() is called
//   to revert the value.
class InFlightChange {
 public:
  InFlightChange(Id window_id, ChangeType type);
  virtual ~InFlightChange();

  Id window_id() const { return window_id_; }
  ChangeType change_type() const { return change_type_; }

  // Called when a previous change associated with the same window and type
  // has completed. |success| is true if the change completed successfully,
  // false if the change failed.
  virtual void PreviousChangeCompleted(InFlightChange* change,
                                       bool success) = 0;

  // The change failed and there is no pending change of the same type
  // outstanding, revert the value.
  virtual void Revert() = 0;

 private:
  const Id window_id_;
  const ChangeType change_type_;
};

class InFlightBoundsChange : public InFlightChange {
 public:
  InFlightBoundsChange(WindowTreeConnection* tree,
                       Id window_id,
                       const gfx::Rect& revert_bounds);

  void set_revert_bounds(const gfx::Rect& bounds) { revert_bounds_ = bounds; }

  // InFlightChange:
  void PreviousChangeCompleted(InFlightChange* change, bool success) override;
  void Revert() override;

 private:
  WindowTreeConnection* tree_;
  gfx::Rect revert_bounds_;

  DISALLOW_COPY_AND_ASSIGN(InFlightBoundsChange);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_PUBLIC_CPP_LIB_IN_FLIGHT_CHANGE_H_
