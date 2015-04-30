// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/image_decoder.h"

#if defined(OS_POSIX)
#include <sys/types.h>
#include <signal.h>
#endif

#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/test/test_utils.h"

using content::BrowserThread;

namespace {

class TestImageRequest : public ImageDecoder::ImageRequest {
 public:
  explicit TestImageRequest(const base::Closure& quit_closure)
      : quit_closure_(quit_closure),
        quit_called_(false) {
  }

  ~TestImageRequest() override {
    if (!quit_called_) {
      quit_closure_.Run();
    }
  }

 private:
  void OnImageDecoded(const SkBitmap& decoded_image) override {
    Quit();
  }

  void OnDecodeImageFailed() override {
    Quit();
  }

  void Quit() {
    EXPECT_FALSE(quit_called_);
    quit_called_ = true;
    quit_closure_.Run();
  }

  base::Closure quit_closure_;
  bool quit_called_;

  DISALLOW_COPY_AND_ASSIGN(TestImageRequest);
};

class KillProcessObserver : public content::BrowserChildProcessObserver {
 public:
  KillProcessObserver() {
    Add(this);
  }

  ~KillProcessObserver() override {
    Remove(this);
  }

 private:
  void BrowserChildProcessHostConnected(
      const content::ChildProcessData& data) override {
    base::ProcessHandle handle = data.handle;

    if (handle == base::kNullProcessHandle)
      return;

#if defined(OS_WIN)
    // On windows, duplicate the process handle since base::Process closes it on
    // destruction.
    base::ProcessHandle out_handle;
    if (!::DuplicateHandle(GetCurrentProcess(), handle,
                           GetCurrentProcess(), &out_handle,
                           0, FALSE, DUPLICATE_SAME_ACCESS))
      return;
    handle = out_handle;
#endif

    EXPECT_TRUE(base::Process(handle).Terminate(0, true));
  }
};

#if defined(OS_POSIX)
class FreezeProcessObserver : public content::BrowserChildProcessObserver {
 public:
  FreezeProcessObserver() {
    Add(this);
  }

  ~FreezeProcessObserver() override {
    Remove(this);
  }

 private:
  void BrowserChildProcessHostConnected(
      const content::ChildProcessData& data) override {
    if (data.handle != base::kNullProcessHandle)
      EXPECT_EQ(0, kill(data.handle, SIGSTOP));
  }
};
#endif  // defined(OS_POSIX)

}  // namespace

class ImageDecoderBrowserTest : public InProcessBrowserTest {
};

IN_PROC_BROWSER_TEST_F(ImageDecoderBrowserTest, Basic) {
  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner;
  TestImageRequest test_request(runner->QuitClosure());
  ImageDecoder::Start(&test_request, std::string());
  runner->Run();
}

IN_PROC_BROWSER_TEST_F(ImageDecoderBrowserTest, StartAndDestroy) {
  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner;
  scoped_ptr<TestImageRequest> test_request(
      new TestImageRequest(runner->QuitClosure()));
  ImageDecoder::Start(test_request.get(), std::string());
  test_request.reset();
  runner->Run();
}

IN_PROC_BROWSER_TEST_F(ImageDecoderBrowserTest, StartAndKillProcess) {
  KillProcessObserver observer;
  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner;
  scoped_ptr<TestImageRequest> test_request(
      new TestImageRequest(runner->QuitClosure()));
  ImageDecoder::Start(test_request.get(), std::string());
  runner->Run();
}

#if defined(OS_POSIX)
IN_PROC_BROWSER_TEST_F(ImageDecoderBrowserTest, StartAndFreezeProcess) {
  FreezeProcessObserver observer;
  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner;
  scoped_ptr<TestImageRequest> test_request(
      new TestImageRequest(runner->QuitClosure()));
  ImageDecoder::Start(test_request.get(), std::string());
  runner->Run();
}
#endif  // defined(OS_POSIX)
