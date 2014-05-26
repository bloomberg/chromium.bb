// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_WEBTESTRUNNER_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_WEBTESTRUNNER_H_

#include <vector>

namespace blink {
class WebPermissionClient;
}

namespace content {

class WebTestRunner {
 public:
  // Returns a mock WebPermissionClient that is used for layout tests. An
  // embedder should use this for all WebViews it creates.
  virtual blink::WebPermissionClient* GetWebPermissions() const = 0;

  // After WebTestDelegate::testFinished was invoked, the following methods
  // can be used to determine what kind of dump the main WebTestProxy can
  // provide.

  // If true, WebTestDelegate::audioData returns an audio dump and no text
  // or pixel results are available.
  virtual bool ShouldDumpAsAudio() const = 0;
  virtual void GetAudioData(std::vector<unsigned char>* buffer_view) const = 0;

  // Returns true if the call to WebTestProxy::captureTree will invoke
  // WebTestDelegate::captureHistoryForWindow.
  virtual bool ShouldDumpBackForwardList() const = 0;

  // Returns true if WebTestProxy::capturePixels should be invoked after
  // capturing text results.
  virtual bool ShouldGeneratePixelResults() = 0;
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_TEST_RUNNER_WEBTESTRUNNER_H_
