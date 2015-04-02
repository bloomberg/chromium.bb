// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WINDOW_MANAGER_WINDOW_MANAGER_TEST_UTIL_H_
#define SERVICES_WINDOW_MANAGER_WINDOW_MANAGER_TEST_UTIL_H_

#include <set>

#include "mojo/services/window_manager/view_target.h"
#include "third_party/mojo_services/src/view_manager/public/cpp/lib/view_private.h"
#include "third_party/mojo_services/src/view_manager/public/cpp/view.h"

namespace gfx {
class Rect;
}

namespace window_manager {

// A wrapper around View so we can instantiate these directly without a
// ViewManager.
class TestView : public mojo::View {
 public:
  TestView(int id, const gfx::Rect& rect);
  TestView(int id, const gfx::Rect& rect, mojo::View* parent);
  ~TestView();

  // Builds a child view as a pointer. The caller is responsible for making
  // sure that the root of any tree allocated this way is Destroy()ed.
  static TestView* Build(int id, const gfx::Rect& rect);
  static TestView* Build(int id, const gfx::Rect& rect, View* parent);

  ViewTarget* target() { return target_; }

 private:
  ViewTarget* target_;

  DISALLOW_COPY_AND_ASSIGN(TestView);
};

}  // namespace window_manager

#endif  // SERVICES_WINDOW_MANAGER_WINDOW_MANAGER_TEST_UTIL_H_
