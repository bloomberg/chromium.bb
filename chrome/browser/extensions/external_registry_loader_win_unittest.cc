// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_registry_loader_win.h"

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

class TestExternalRegistryLoader : public ExternalRegistryLoader {
 public:
  TestExternalRegistryLoader() {}

  using ExternalRegistryLoader::StartLoading;

  void WaitForTwoLoadsToFinished() {
    // Run() returns immediately if Quit() has already been called.
    run_loop_.Run();
  }

 private:
  ~TestExternalRegistryLoader() override {}

  void LoadFinished() override {
    ExternalRegistryLoader::LoadFinished();
    ++load_finished_count_;
    ASSERT_LE(load_finished_count_, 2);
    if (load_finished_count_ == 2)
      run_loop_.Quit();
  }

  base::RunLoop run_loop_;
  int load_finished_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestExternalRegistryLoader);
};

}  // namespace

class ExternalRegistryLoaderUnittest : public testing::Test {
 public:
  ExternalRegistryLoaderUnittest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {}
  ~ExternalRegistryLoaderUnittest() override {}

 private:
  content::TestBrowserThreadBundle thread_bundle_;

  DISALLOW_COPY_AND_ASSIGN(ExternalRegistryLoaderUnittest);
};

// Tests that calling StartLoading() more than once doesn't fail DCHECK.
// Regression test for https://crbug.com/653045.
TEST_F(ExternalRegistryLoaderUnittest, TwoStartLoadingDoesNotCrash) {
  scoped_refptr<TestExternalRegistryLoader> test_loader =
      make_scoped_refptr(new TestExternalRegistryLoader());

  test_loader->StartLoading();
  test_loader->StartLoading();

  test_loader->WaitForTwoLoadsToFinished();
  // Let registry watcher code complete.
  base::RunLoop().RunUntilIdle();
}

}  // namespace extensions
