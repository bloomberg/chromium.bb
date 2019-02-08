// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_TEST_FAKE_CONTEXT_H_
#define FUCHSIA_ENGINE_TEST_FAKE_CONTEXT_H_

#include <lib/fidl/cpp/binding.h>
#include <lib/fidl/cpp/binding_set.h>

#include <utility>

#include "base/callback.h"
#include "base/macros.h"
#include "fuchsia/fidl/chromium/web/cpp/fidl.h"
#include "fuchsia/fidl/chromium/web/cpp/fidl_test_base.h"

namespace cr_fuchsia {
namespace test {

// A fake Frame implementation that manages its own lifetime.
class FakeFrame : public chromium::web::testing::Frame_TestBase {
 public:
  explicit FakeFrame(fidl::InterfaceRequest<chromium::web::Frame> request);
  ~FakeFrame() override;

  void set_on_set_observer_callback(base::OnceClosure callback) {
    on_set_observer_callback_ = std::move(callback);
  }

  // Tests can provide e.g a mock NavigationController, which the FakeFrame will
  // pass bind GetNavigationController() requests to.
  void set_navigation_controller(
      chromium::web::NavigationController* controller) {
    navigation_controller_ = controller;
  }

  chromium::web::NavigationEventObserver* observer() { return observer_.get(); }

  // chromium::web::Frame implementation.
  void GetNavigationController(
      fidl::InterfaceRequest<chromium::web::NavigationController> controller)
      override;
  void SetNavigationEventObserver(
      fidl::InterfaceHandle<chromium::web::NavigationEventObserver> observer)
      override;

  // chromium::web::testing::Frame_TestBase implementation.
  void NotImplemented_(const std::string& name) override;

 private:
  fidl::Binding<chromium::web::Frame> binding_;
  chromium::web::NavigationEventObserverPtr observer_;
  base::OnceClosure on_set_observer_callback_;

  chromium::web::NavigationController* navigation_controller_ = nullptr;
  fidl::BindingSet<chromium::web::NavigationController>
      navigation_controller_bindings_;

  DISALLOW_COPY_AND_ASSIGN(FakeFrame);
};

// An implementation of Context that creates and binds FakeFrames.
class FakeContext : public chromium::web::testing::Context_TestBase {
 public:
  using CreateFrameCallback = base::RepeatingCallback<void(FakeFrame*)>;

  FakeContext();
  ~FakeContext() override;

  // Sets a callback that is invoked whenever new Frames are bound.
  void set_on_create_frame_callback(CreateFrameCallback callback) {
    on_create_frame_callback_ = callback;
  }

  // chromium::web::Context implementation.
  void CreateFrame(
      fidl::InterfaceRequest<chromium::web::Frame> frame_request) override;

  // chromium::web::testing::Frame_TestBase implementation.
  void NotImplemented_(const std::string& name) override;

 private:
  CreateFrameCallback on_create_frame_callback_;

  DISALLOW_COPY_AND_ASSIGN(FakeContext);
};

}  // namespace test
}  // namespace cr_fuchsia

#endif  // FUCHSIA_ENGINE_TEST_FAKE_CONTEXT_H_
