// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_TEXT_INPUT_CONTROLLER_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_TEXT_INPUT_CONTROLLER_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"

namespace blink {
class WebFrame;
class WebView;
}

namespace content {

// TextInputController is bound to window.textInputController in Javascript
// when content_shell is running. Layout tests use it to exercise various
// corners of text input.
class TextInputController : public base::SupportsWeakPtr<TextInputController> {
 public:
  TextInputController();
  ~TextInputController();

  void Install(blink::WebFrame* frame);

  void SetWebView(blink::WebView* view);

 private:
  friend class TextInputControllerBindings;

  void InsertText(const std::string& text);
  void UnmarkText();
  void DoCommand(const std::string& text);
  void SetMarkedText(const std::string& text, int start, int length);
  bool HasMarkedText();
  std::vector<int> MarkedRange();
  std::vector<int> SelectedRange();
  std::vector<int> FirstRectForCharacterRange(unsigned location,
                                              unsigned length);
  void SetComposition(const std::string& text);

  blink::WebView* view_;

  base::WeakPtrFactory<TextInputController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(TextInputController);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_TEST_RUNNER_TEXT_INPUT_CONTROLLER_H_
