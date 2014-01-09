// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/sandboxed_unpacker.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace extensions {

class MockSandboxedUnpackerClient : public SandboxedUnpackerClient {
 public:

  void WaitForUnpack() {
    scoped_refptr<content::MessageLoopRunner> runner =
        new content::MessageLoopRunner;
    quit_closure_ = runner->QuitClosure();
    runner->Run();
  }

  base::FilePath temp_dir() const { return temp_dir_; }

 private:
  virtual ~MockSandboxedUnpackerClient() {}

  virtual void OnUnpackSuccess(const base::FilePath& temp_dir,
                               const base::FilePath& extension_root,
                               const base::DictionaryValue* original_manifest,
                               const Extension* extension,
                               const SkBitmap& install_icon) OVERRIDE {
    temp_dir_ = temp_dir;
    quit_closure_.Run();

  }

  virtual void OnUnpackFailure(const base::string16& error) OVERRIDE {
    ASSERT_TRUE(false);
  }

  base::Closure quit_closure_;
  base::FilePath temp_dir_;
};

class SandboxedUnpackerTest : public testing::Test {
 public:
  virtual void SetUp() {
   ASSERT_TRUE(extensions_dir_.CreateUniqueTempDir());
    browser_threads_.reset(new content::TestBrowserThreadBundle(
        content::TestBrowserThreadBundle::IO_MAINLOOP));
    in_process_utility_thread_helper_.reset(
        new content::InProcessUtilityThreadHelper);
    // It will delete itself.
    client_ = new MockSandboxedUnpackerClient;
  }

  virtual void TearDown() {
    // Need to destruct SandboxedUnpacker before the message loop since
    // it posts a task to it.
    sandboxed_unpacker_ = NULL;
    base::RunLoop().RunUntilIdle();
  }

  void SetupUnpacker(const std::string& crx_name) {
    base::FilePath original_path;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &original_path));
    original_path = original_path.AppendASCII("extensions")
        .AppendASCII("unpacker")
        .AppendASCII(crx_name);
    ASSERT_TRUE(base::PathExists(original_path)) << original_path.value();

    sandboxed_unpacker_ = new SandboxedUnpacker(
        original_path,
        Manifest::INTERNAL,
        Extension::NO_FLAGS,
        extensions_dir_.path(),
        base::MessageLoopProxy::current(),
        client_);

    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(&SandboxedUnpacker::Start, sandboxed_unpacker_.get()));
    client_->WaitForUnpack();
  }

  base::FilePath GetInstallPath() {
    return client_->temp_dir().AppendASCII(kTempExtensionName);
  }

 protected:
  base::ScopedTempDir extensions_dir_;
  MockSandboxedUnpackerClient* client_;
  scoped_refptr<SandboxedUnpacker> sandboxed_unpacker_;
  scoped_ptr<content::TestBrowserThreadBundle> browser_threads_;
  scoped_ptr<content::InProcessUtilityThreadHelper>
      in_process_utility_thread_helper_;
};

TEST_F(SandboxedUnpackerTest, NoCatalogsSuccess) {
  SetupUnpacker("no_l10n.crx");
  // Check that there is no _locales folder.
  base::FilePath install_path =
      GetInstallPath().Append(kLocaleFolder);
  EXPECT_FALSE(base::PathExists(install_path));
}

TEST_F(SandboxedUnpackerTest, WithCatalogsSuccess) {
  SetupUnpacker("good_l10n.crx");
  // Check that there is _locales folder.
  base::FilePath install_path =
      GetInstallPath().Append(kLocaleFolder);
  EXPECT_TRUE(base::PathExists(install_path));
}

}  // namespace extensions
