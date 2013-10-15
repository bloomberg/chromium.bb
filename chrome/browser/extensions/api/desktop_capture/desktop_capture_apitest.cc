// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <queue>

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/browser/extensions/api/desktop_capture/desktop_capture_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/media/desktop_media_picker.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/webrtc/modules/desktop_capture/screen_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/window_capturer.h"

namespace extensions {

namespace {

class FakeDesktopMediaPicker : public DesktopMediaPicker {
 public:
  explicit FakeDesktopMediaPicker(const content::DesktopMediaID& source,
                                  bool expect_cancelled)
      : source_(source),
        expect_cancelled_(expect_cancelled),
        weak_factory_(this) {
  }
  virtual ~FakeDesktopMediaPicker() {}

  // DesktopMediaPicker interface.
  virtual void Show(gfx::NativeWindow context,
                    gfx::NativeWindow parent,
                    const string16& app_name,
                    scoped_ptr<DesktopMediaPickerModel> model,
                    const DoneCallback& done_callback) OVERRIDE {
    if (!expect_cancelled_) {
      // Post a task to call the callback asynchronously.
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::Bind(&FakeDesktopMediaPicker::CallCallback,
                     weak_factory_.GetWeakPtr(), done_callback));
    } else {
      // If we expect the dialog to be canceled then store the callback to
      // retain reference to the callback handler.
      done_callback_ = done_callback;
    }
  }

 private:
  void CallCallback(DoneCallback done_callback) {
    done_callback.Run(source_);
  }

  content::DesktopMediaID source_;
  bool expect_cancelled_;
  DoneCallback done_callback_;

  base::WeakPtrFactory<FakeDesktopMediaPicker> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeDesktopMediaPicker);
};

class FakeDesktopMediaPickerFactory :
    public DesktopCaptureChooseDesktopMediaFunction::PickerFactory {
 public:
  struct Expectation {
    bool screens;
    bool windows;
    content::DesktopMediaID selected_source;
    bool cancelled;
  };

  FakeDesktopMediaPickerFactory() {}
  virtual ~FakeDesktopMediaPickerFactory() {}

  void SetExpectations(const Expectation* expectation_array, int size) {
    for (int i = 0; i < size; ++i) {
      expectations_.push(expectation_array[i]);
    }
  }

  // DesktopCaptureChooseDesktopMediaFunction::PickerFactory interface.
  virtual scoped_ptr<DesktopMediaPickerModel> CreateModel(
      scoped_ptr<webrtc::ScreenCapturer> screen_capturer,
      scoped_ptr<webrtc::WindowCapturer> window_capturer) OVERRIDE {
    EXPECT_TRUE(!expectations_.empty());
    if (!expectations_.empty()) {
      EXPECT_EQ(expectations_.front().screens, !!screen_capturer.get());
      EXPECT_EQ(expectations_.front().windows, !!window_capturer.get());
    }
    return scoped_ptr<DesktopMediaPickerModel>(
      new DesktopMediaPickerModelImpl(screen_capturer.Pass(),
                                      window_capturer.Pass()));
  }
  virtual scoped_ptr<DesktopMediaPicker> CreatePicker() OVERRIDE {
    content::DesktopMediaID next_source;
    bool expect_cancelled = false;
    if (!expectations_.empty()) {
      next_source = expectations_.front().selected_source;
      expect_cancelled = expectations_.front().cancelled;
      expectations_.pop();
    }
    return scoped_ptr<DesktopMediaPicker>(
        new FakeDesktopMediaPicker(next_source, expect_cancelled));
  }

 private:
  std::queue<Expectation> expectations_;

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

IN_PROC_BROWSER_TEST_F(DesktopCaptureApiTest, ChooseDesktopMedia) {
  // Each of the following expectations corresponds to one test in
  // chrome/test/data/extensions/api_test/desktop_capture/test.js .
  FakeDesktopMediaPickerFactory::Expectation picker_expectations[] = {
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
      content::DesktopMediaID(content::DesktopMediaID::TYPE_SCREEN, 0) },
    // chooseMediaAndTryGetStreamWithInvalidId()
    { true, true,
      content::DesktopMediaID(content::DesktopMediaID::TYPE_SCREEN, 0) },
    // cancelDialog()
    { true, true,
      content::DesktopMediaID(), true },
  };
  picker_factory_.SetExpectations(picker_expectations,
                                  arraysize(picker_expectations));
  ASSERT_TRUE(RunExtensionTest("desktop_capture")) << message_;
}

IN_PROC_BROWSER_TEST_F(DesktopCaptureApiTest, Delegation) {
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

  FakeDesktopMediaPickerFactory::Expectation picker_expectations[] = {
    { true, true,
      content::DesktopMediaID(content::DesktopMediaID::TYPE_SCREEN, 0) },
    { true, true,
      content::DesktopMediaID(content::DesktopMediaID::TYPE_SCREEN, 0) },
  };
  picker_factory_.SetExpectations(picker_expectations,
                                  arraysize(picker_expectations));

  bool result;

  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "getStream()", &result));
  EXPECT_TRUE(result);

  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "getStreamWithInvalidId()", &result));
  EXPECT_TRUE(result);
}

}  // namespace extensions
