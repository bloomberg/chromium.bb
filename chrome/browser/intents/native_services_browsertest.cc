// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <iterator>
#include <string>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/browser_thread.h"
#include "chrome/browser/intents/intent_service_host.h"
#include "chrome/browser/intents/native_services.h"
#include "chrome/browser/intents/web_intents_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_intents_dispatcher.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"
#include "webkit/glue/web_intent_data.h"
#include "webkit/glue/web_intent_reply_data.h"
#include "webkit/glue/web_intent_service_data.h"

using content::BrowserThread;

namespace {

const std::string kPoodlePath = "/home/poodles/skippy.png";
const int64 kTestFileSize = 193;

base::FilePath CreateTestFile() {
  base::FilePath file;
  PathService::Get(chrome::DIR_TEST_DATA, &file);
  file = file.AppendASCII("web_intents").AppendASCII("test.png");
  return file;
}

class TestIntentsDispatcher : public content::WebIntentsDispatcher {
 public:
  TestIntentsDispatcher() {
    intent_.action = ASCIIToUTF16(web_intents::kActionPick);
    intent_.type = ASCIIToUTF16("image/*");
  }

  virtual const webkit_glue::WebIntentData& GetIntent() OVERRIDE {
    return intent_;
  }

  virtual void DispatchIntent(content::WebContents* web_contents) OVERRIDE {}
  virtual void ResetDispatch() OVERRIDE {}

  virtual void SendReply(const webkit_glue::WebIntentReply& reply) OVERRIDE {
    reply_.reset(new webkit_glue::WebIntentReply(reply));
  }

  virtual void RegisterReplyNotification(
      const base::Callback<void(webkit_glue::WebIntentReplyType)>&) OVERRIDE {
  }

  webkit_glue::WebIntentData intent_;
  scoped_ptr<webkit_glue::WebIntentReply> reply_;
};

class FakeSelectFileDialog : public ui::SelectFileDialog {
 public:
  FakeSelectFileDialog(
      Listener* listener, ui::SelectFilePolicy* policy, bool should_succeed)
      : SelectFileDialog(listener, policy), should_succeed_(should_succeed),
        test_file_(CreateTestFile()) {
  }
  virtual bool IsRunning(gfx::NativeWindow parent_window) const OVERRIDE {
    return false;
  }
  virtual void ListenerDestroyed() OVERRIDE {}
  virtual bool HasMultipleFileTypeChoicesImpl() OVERRIDE {
    return false;
  }

 protected:
  virtual void SelectFileImpl(
      Type type,
      const string16& title,
      const base::FilePath& default_path,
      const FileTypeInfo* file_types,
      int file_type_index,
      const base::FilePath::StringType& default_extension,
      gfx::NativeWindow owning_window,
      void* params) OVERRIDE {
    if (should_succeed_)
      listener_->FileSelected(test_file_, kTestFileSize, params);
    else
      listener_->FileSelectionCanceled(params);
  }

 private:
  bool should_succeed_;
  base::FilePath test_file_;
  DISALLOW_COPY_AND_ASSIGN(FakeSelectFileDialog);
};

class TestSelectFileDialogFactory : public ui::SelectFileDialogFactory {
 public:
  virtual ui::SelectFileDialog* Create(
      ui::SelectFileDialog::Listener* listener,
      ui::SelectFilePolicy* policy) OVERRIDE {
    return new FakeSelectFileDialog(listener, policy, should_succeed_);
  }
  bool should_succeed_;
};

class NativeServicesBrowserTest : public InProcessBrowserTest {
 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    // We start the test server now instead of in
    // SetUpInProcessBrowserTestFixture so that we can get its port number.
    ASSERT_TRUE(test_server()->Start());

    command_line->AppendSwitch(switches::kWebIntentsNativeServicesEnabled);
    test_file_ = CreateTestFile();
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    dispatcher_.reset(new TestIntentsDispatcher());
    factory_.reset(new TestSelectFileDialogFactory());
    ui::SelectFileDialog::SetFactory(factory_.get());
  }

  void DispatchIntent(bool should_succeed) {
    factory_->should_succeed_ = should_succeed;

    content::WebContents* tab =
        browser()->tab_strip_model()->GetActiveWebContents();

    web_intents::NativeServiceFactory factory;
    GURL url(web_intents::kNativeFilePickerUrl);
    scoped_ptr<web_intents::IntentServiceHost> service(
        factory.CreateServiceInstance(url, dispatcher_->intent_, tab));
    service->HandleIntent(dispatcher_.get());

    // Reads of file size are done on the FILE thread, then posted
    // back to the UI thread. So these are necessary for the
    // should_succeed case and noop for the !should_succeed
    content::RunAllPendingInMessageLoop(BrowserThread::FILE);
    content::RunAllPendingInMessageLoop(BrowserThread::UI);
  }

  scoped_ptr<TestIntentsDispatcher> dispatcher_;
  scoped_ptr<TestSelectFileDialogFactory> factory_;
  base::FilePath test_file_;
};

IN_PROC_BROWSER_TEST_F(NativeServicesBrowserTest, PickFileSelected) {
  DispatchIntent(true);
  ASSERT_TRUE(dispatcher_->reply_);
  EXPECT_EQ(
      webkit_glue::WebIntentReply(
            webkit_glue::WEB_INTENT_REPLY_SUCCESS,
            test_file_,
            kTestFileSize),
      *dispatcher_->reply_.get());
}

IN_PROC_BROWSER_TEST_F(NativeServicesBrowserTest, PickFileCancelled) {
  DispatchIntent(false);
  ASSERT_TRUE(dispatcher_->reply_);
  EXPECT_EQ(
      webkit_glue::WebIntentReply(
            webkit_glue::WEB_INTENT_REPLY_FAILURE,
            string16()),
      *dispatcher_->reply_.get());
}

}  // namespace
