// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/seat.h"

#include "base/memory/scoped_refptr.h"
#include "components/exo/seat_observer.h"
#include "components/exo/test/exo_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace exo {
namespace {

using SeatTest = test::ExoTestBase;

class MockSeatObserver : public SeatObserver {
 public:
  int on_surface_focused_count() { return on_surface_focused_count_; }

  // Overridden from SeatObserver:
  void OnSurfaceFocused(Surface* surface) override {
    on_surface_focused_count_++;
  }

 private:
  int on_surface_focused_count_ = 0;
};

TEST_F(SeatTest, OnSurfaceFocused) {
  Seat seat;
  MockSeatObserver observer;

  seat.AddObserver(&observer);
  seat.OnWindowFocused(nullptr, nullptr);
  ASSERT_EQ(1, observer.on_surface_focused_count());

  seat.RemoveObserver(&observer);
  seat.OnWindowFocused(nullptr, nullptr);
  ASSERT_EQ(1, observer.on_surface_focused_count());
}

}  // namespace
}  // namespace exo
