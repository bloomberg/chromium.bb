// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/runner/android/background_application_loader.h"

#include "mojo/shell/public/interfaces/application.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace runner {
namespace {

class DummyLoader : public shell::ApplicationLoader {
 public:
  DummyLoader() : simulate_app_quit_(true) {}
  ~DummyLoader() override {}

  // shell::ApplicationLoader overrides:
  void Load(const GURL& url,
            InterfaceRequest<Application> application_request) override {
    if (simulate_app_quit_)
      base::MessageLoop::current()->QuitWhenIdle();
  }

  void DontSimulateAppQuit() { simulate_app_quit_ = false; }

 private:
  bool simulate_app_quit_;
};

// Tests that the loader can start and stop gracefully.
TEST(BackgroundApplicationLoaderTest, StartStop) {
  scoped_ptr<shell::ApplicationLoader> real_loader(new DummyLoader());
  BackgroundApplicationLoader loader(real_loader.Pass(), "test",
                                     base::MessageLoop::TYPE_DEFAULT);
}

// Tests that the loader can load a service that is well behaved (quits
// itself).
TEST(BackgroundApplicationLoaderTest, Load) {
  scoped_ptr<shell::ApplicationLoader> real_loader(new DummyLoader());
  BackgroundApplicationLoader loader(real_loader.Pass(), "test",
                                     base::MessageLoop::TYPE_DEFAULT);
  ApplicationPtr application;
  loader.Load(GURL(), GetProxy(&application));
}

}  // namespace
}  // namespace runner
}  // namespace mojo
