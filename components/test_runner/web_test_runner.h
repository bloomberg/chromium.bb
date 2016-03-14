// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TEST_RUNNER_WEB_TEST_RUNNER_H_
#define COMPONENTS_TEST_RUNNER_WEB_TEST_RUNNER_H_

#include <string>
#include <vector>

namespace base {
class DictionaryValue;
}

namespace blink {
class WebContentSettingsClient;
}

namespace test_runner {

class LayoutDumpFlags;

class WebTestRunner {
 public:
  // Returns a mock WebContentSettings that is used for layout tests. An
  // embedder should use this for all WebViews it creates.
  virtual blink::WebContentSettingsClient* GetWebContentSettings() const = 0;

  // After WebTestDelegate::TestFinished was invoked, the following methods
  // can be used to determine what kind of dump the main WebTestProxy can
  // provide.

  // If true, WebTestDelegate::audioData returns an audio dump and no text
  // or pixel results are available.
  virtual bool ShouldDumpAsAudio() const = 0;
  virtual void GetAudioData(std::vector<unsigned char>* buffer_view) const = 0;

  // Gets layout dump flags (i.e. dump-as-text or dump-as-markup) requested
  // by the test (i.e. via testRunner.dumpAsText() called from javascript).
  virtual const LayoutDumpFlags& GetLayoutDumpFlags() = 0;

  // Replicates changes to layout dump flags.
  virtual void ReplicateLayoutDumpFlagsChanges(
      const base::DictionaryValue& changed_values) = 0;

  // If custom text dump is present (i.e. if testRunner.setCustomTextOutput has
  // been called from javascript), then returns |true| and populates the
  // |custom_text_dump| argument.  Otherwise returns |false|.
  virtual bool HasCustomTextDump(std::string* custom_text_dump) const = 0;

  // Returns true if the call to WebTestProxy::captureTree will invoke
  // WebTestDelegate::captureHistoryForWindow.
  virtual bool ShouldDumpBackForwardList() const = 0;

  // Returns true if WebTestProxy::capturePixels should be invoked after
  // capturing text results.
  virtual bool ShouldGeneratePixelResults() = 0;
};

}  // namespace test_runner

#endif  // COMPONENTS_TEST_RUNNER_WEB_TEST_RUNNER_H_
