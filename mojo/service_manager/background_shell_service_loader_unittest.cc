// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/service_manager/background_shell_service_loader.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {

namespace {

class DummyLoader : public ServiceLoader {
 public:
  DummyLoader() : simulate_app_quit_(true) {}
  virtual ~DummyLoader() {}

  // ServiceLoader overrides:
  virtual void LoadService(ServiceManager* manager,
                           const GURL& url,
                           ScopedMessagePipeHandle shell_handle) OVERRIDE {
    if (simulate_app_quit_)
      base::MessageLoop::current()->Quit();
  }

  virtual void OnServiceError(ServiceManager* manager,
                              const GURL& url) OVERRIDE {
  }

  void DontSimulateAppQuit() { simulate_app_quit_ = false; }

 private:
  bool simulate_app_quit_;
};

}  // namespace

// Tests that the loader can start and stop gracefully.
TEST(BackgroundShellServiceLoaderTest, StartStop) {
  scoped_ptr<ServiceLoader> real_loader(new DummyLoader());
  BackgroundShellServiceLoader loader(real_loader.Pass(), "test",
                                      base::MessageLoop::TYPE_DEFAULT);
}

// Tests that the loader can load a service that is well behaved (quits
// itself).
TEST(BackgroundShellServiceLoaderTest, Load) {
  scoped_ptr<ServiceLoader> real_loader(new DummyLoader());
  BackgroundShellServiceLoader loader(real_loader.Pass(), "test",
                                      base::MessageLoop::TYPE_DEFAULT);
  MessagePipe dummy;
  loader.LoadService(NULL, GURL(), dummy.handle0.Pass());
}

// Test that an app that doesn't quit itself can still be handled.
// TODO(tim): Remove this.
TEST(BackgroundShellServiceLoaderTest, LoadMisbehavedService) {
  scoped_ptr<DummyLoader> real_loader(new DummyLoader());
  real_loader->DontSimulateAppQuit();
  BackgroundShellServiceLoader loader(
      real_loader.PassAs<ServiceLoader>(), "test",
      base::MessageLoop::TYPE_DEFAULT);
  // Because this app is mis-behaved (doesn't quit itself), we need to
  // explicitly kill the thread.
  loader.set_quit_on_shutdown();
  MessagePipe dummy;
  loader.LoadService(NULL, GURL(), dummy.handle0.Pass());
}

}  // namespace mojo
