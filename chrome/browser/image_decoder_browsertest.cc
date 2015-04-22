// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/image_decoder.h"

#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/browser_thread.h"
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
