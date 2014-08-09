// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/application_manager/background_shell_application_loader.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {

namespace {

class DummyLoader : public ApplicationLoader {
 public:
  DummyLoader() : simulate_app_quit_(true) {}
  virtual ~DummyLoader() {}

  // ApplicationLoader overrides:
  virtual void Load(ApplicationManager* manager,
                    const GURL& url,
                    scoped_refptr<LoadCallbacks> callbacks) OVERRIDE {
    if (simulate_app_quit_)
      base::MessageLoop::current()->Quit();
  }

  virtual void OnServiceError(ApplicationManager* manager,
                              const GURL& url) OVERRIDE {}

  void DontSimulateAppQuit() { simulate_app_quit_ = false; }

 private:
  bool simulate_app_quit_;
};

}  // namespace

// Tests that the loader can start and stop gracefully.
TEST(BackgroundShellApplicationLoaderTest, StartStop) {
  scoped_ptr<ApplicationLoader> real_loader(new DummyLoader());
  BackgroundShellApplicationLoader loader(
      real_loader.Pass(), "test", base::MessageLoop::TYPE_DEFAULT);
}

// Tests that the loader can load a service that is well behaved (quits
// itself).
TEST(BackgroundShellApplicationLoaderTest, Load) {
  scoped_ptr<ApplicationLoader> real_loader(new DummyLoader());
  BackgroundShellApplicationLoader loader(
      real_loader.Pass(), "test", base::MessageLoop::TYPE_DEFAULT);
  MessagePipe dummy;
  scoped_refptr<ApplicationLoader::SimpleLoadCallbacks> callbacks(
      new ApplicationLoader::SimpleLoadCallbacks(dummy.handle0.Pass()));
  loader.Load(NULL, GURL(), callbacks);
}

}  // namespace mojo
