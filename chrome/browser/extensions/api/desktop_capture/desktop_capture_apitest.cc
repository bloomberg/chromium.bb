// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <queue>

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/browser/extensions/api/desktop_capture/desktop_capture_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/media/fake_desktop_media_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"

namespace extensions {

namespace {

struct TestFlags {
  bool expect_screens;
  bool expect_windows;
  content::DesktopMediaID selected_source;
  bool cancelled;

  // Following flags are set by FakeDesktopMediaPicker when it's created and
  // deleted.
  bool picker_created;
  bool picker_deleted;
};

class FakeDesktopMediaPicker : public DesktopMediaPicker {
 public:
  explicit FakeDesktopMediaPicker(TestFlags* expectation)
      : expectation_(expectation),
        weak_factory_(this) {
    expectation_->picker_created = true;
  }
  virtual ~FakeDesktopMediaPicker() {
    expectation_->picker_deleted = true;
  }

  // DesktopMediaPicker interface.
  virtual void Show(content::WebContents* web_contents,
                    gfx::NativeWindow context,
                    gfx::NativeWindow parent,
                    const base::string16& app_name,
                    const base::string16& target_name,
                    scoped_ptr<DesktopMediaList> model,
                    const DoneCallback& done_callback) OVERRIDE {
    if (!expectation_->cancelled) {
      // Post a task to call the callback asynchronously.
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::Bind(&FakeDesktopMediaPicker::CallCallback,
                     weak_factory_.GetWeakPtr(), done_callback));
    } else {
      // If we expect the dialog to be cancelled then store the callback to
      // retain reference to the callback handler.
      done_callback_ = done_callback;
    }
  }

 private:
  void CallCallback(DoneCallback done_callback) {
    done_callback.Run(expectation_->selected_source);
  }

  TestFlags* expectation_;
  DoneCallback done_callback_;

  base::WeakPtrFactory<FakeDesktopMediaPicker> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeDesktopMediaPicker);
};

class FakeDesktopMediaPickerFactory :
    public DesktopCaptureChooseDesktopMediaFunction::PickerFactory {
 public:
  FakeDesktopMediaPickerFactory() {}
  virtual ~FakeDesktopMediaPickerFactory() {}

  void SetTestFlags(TestFlags* test_flags, int tests_count) {
    test_flags_ = test_flags;
    tests_count_ = tests_count;
    current_test_ = 0;
  }

  // DesktopCaptureChooseDesktopMediaFunction::PickerFactory interface.
  virtual scoped_ptr<DesktopMediaList> CreateModel(
      bool show_screens,
      bool show_windows) OVERRIDE {
    EXPECT_LE(current_test_, tests_count_);
    if (current_test_ >= tests_count_)
      return scoped_ptr<DesktopMediaList>();
    EXPECT_EQ(test_flags_[current_test_].expect_screens, show_screens);
    EXPECT_EQ(test_flags_[current_test_].expect_windows, show_windows);
    return scoped_ptr<DesktopMediaList>(new FakeDesktopMediaList());
  }

  virtual scoped_ptr<DesktopMediaPicker> CreatePicker() OVERRIDE {
    EXPECT_LE(current_test_, tests_count_);
    if (current_test_ >= tests_count_)
      return scoped_ptr<DesktopMediaPicker>();
    ++current_test_;
    return scoped_ptr<DesktopMediaPicker>(
        new FakeDesktopMediaPicker(test_flags_ + current_test_ - 1));
  }

 private:
  TestFlags* test_flags_;
  int tests_count_;
  int current_test_;

  DISALLOW_COPY_AND_ASSIGN(FakeDesktopMediaPickerFactory);
};

class DesktopCaptureApiTest : public ExtensionApiTest {
 public:
  DesktopCaptureApiTest() {
    DesktopCaptureChooseDesktopMediaFunction::
        SetPickerFactoryForTests(&picker_factory_);
  }
  virtual ~DesktopCaptureApiTest() {
    DesktopCaptureChooseDesktopMediaFunction::
        SetPickerFactoryForTests(NULL);
  }

 protected:
  GURL GetURLForPath(const std::string& host, const std::string& path) {
    std::string port = base::IntToString(embedded_test_server()->port());
    GURL::Replacements replacements;
    replacements.SetHostStr(host);
    replacements.SetPortStr(port);
    return embedded_test_server()->GetURL(path).ReplaceComponents(replacements);
  }

  FakeDesktopMediaPickerFactory picker_factory_;
};

}  // namespace

// Flaky on Windows: http://crbug.com/301887
#if defined(OS_WIN)
#define MAYBE_ChooseDesktopMedia DISABLED_ChooseDesktopMedia
#else
#define MAYBE_ChooseDesktopMedia ChooseDesktopMedia
#endif
IN_PROC_BROWSER_TEST_F(DesktopCaptureApiTest, MAYBE_ChooseDesktopMedia) {
  // Each element in the following array corresponds to one test in
  // chrome/test/data/extensions/api_test/desktop_capture/test.js .
  TestFlags test_flags[] = {
    // pickerUiCanceled()
    { true, true,
      content::DesktopMediaID() },
    // chooseMedia()
    { true, true,
      content::DesktopMediaID(content::DesktopMediaID::TYPE_SCREEN, 0) },
    // screensOnly()
    { true, false,
      content::DesktopMediaID() },
    // WindowsOnly()
    { false, true,
      content::DesktopMediaID() },
    // chooseMediaAndGetStream()
    { true, true,
      content::DesktopMediaID(content::DesktopMediaID::TYPE_SCREEN,
                              webrtc::kFullDesktopScreenId) },
    // chooseMediaAndTryGetStreamWithInvalidId()
    { true, true,
      content::DesktopMediaID(content::DesktopMediaID::TYPE_SCREEN,
                              webrtc::kFullDesktopScreenId) },
    // cancelDialog()
    { true, true,
      content::DesktopMediaID(), true },
  };
  picker_factory_.SetTestFlags(test_flags, arraysize(test_flags));
  ASSERT_TRUE(RunExtensionTest("desktop_capture")) << message_;
}

// Test is flaky http://crbug.com/301887.
IN_PROC_BROWSER_TEST_F(DesktopCaptureApiTest, DISABLED_Delegation) {
  // Initialize test server.
  base::FilePath test_data;
  EXPECT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_data));
  embedded_test_server()->ServeFilesFromDirectory(test_data.AppendASCII(
      "extensions/api_test/desktop_capture_delegate"));
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  host_resolver()->AddRule("*", embedded_test_server()->base_url().host());

  // Load extension.
  base::FilePath extension_path =
      test_data_dir_.AppendASCII("desktop_capture_delegate");
  const Extension* extension = LoadExtensionWithFlags(
      extension_path, ExtensionBrowserTest::kFlagNone);
  ASSERT_TRUE(extension);

  ui_test_utils::NavigateToURL(
      browser(), GetURLForPath("example.com", "/example.com.html"));

  TestFlags test_flags[] = {
    { true, true,
      content::DesktopMediaID(content::DesktopMediaID::TYPE_SCREEN, 0) },
    { true, true,
      content::DesktopMediaID(content::DesktopMediaID::TYPE_SCREEN, 0) },
    { true, true,
      content::DesktopMediaID(content::DesktopMediaID::TYPE_SCREEN, 0), true },
  };
  picker_factory_.SetTestFlags(test_flags, arraysize(test_flags));

  bool result;

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents, "getStream()", &result));
  EXPECT_TRUE(result);

  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents, "getStreamWithInvalidId()", &result));
  EXPECT_TRUE(result);

  // Verify that the picker is closed once the tab is closed.
  content::WebContentsDestroyedWatcher destroyed_watcher(web_contents);
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents, "openPickerDialogAndReturn()", &result));
  EXPECT_TRUE(result);
  EXPECT_TRUE(test_flags[2].picker_created);
  EXPECT_FALSE(test_flags[2].picker_deleted);

  web_contents->Close();
  destroyed_watcher.Wait();
  EXPECT_TRUE(test_flags[2].picker_deleted);
}

}  // namespace extensions
