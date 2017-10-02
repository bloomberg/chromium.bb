// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_PAGE_PASSWORDS_ANALYSER_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_PAGE_PASSWORDS_ANALYSER_H_

#include "third_party/WebKit/public/web/WebConsoleMessage.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace autofill {

using ConsoleLevel = blink::WebConsoleMessage::Level;

// An interface for logging warnings relating to the Password Manager.
// Abstracted for testing purposes.
class PagePasswordsAnalyserLogger {
 public:
  static const ConsoleLevel kError = blink::WebConsoleMessage::kLevelError;
  static const ConsoleLevel kWarning = blink::WebConsoleMessage::kLevelWarning;
  static const ConsoleLevel kVerbose = blink::WebConsoleMessage::kLevelVerbose;

  virtual void Send(const std::string& message,
                    ConsoleLevel level,
                    const blink::WebNode& node) = 0;

  virtual void Send(const std::string& message,
                    ConsoleLevel level,
                    const std::vector<blink::WebNode>& nodes) = 0;

  virtual void Flush() = 0;
};

// This class provides feedback to web developers about the password forms on
// their webpages, in order to increase the accessibility of web forms to the
// Password Manager. This is achieved by crawling the DOM whenever new forms are
// added to the page and checking for common mistakes, or ways in which the form
// could be improved (for example, with autocomplete attributes). See
// |AnalyseDocumentDOM| for more specific information about these warnings.
class PagePasswordsAnalyser {
 public:
  PagePasswordsAnalyser();

  ~PagePasswordsAnalyser();

  // Clear the set of nodes that have already been analysed, so that they will
  // be analysed again next time |AnalyseDocumentDOM| is called. This is called
  // upon page load, for instance.
  void Reset();

  // |AnalyseDocumentDOM| traverses the DOM, logging potential issues in the
  // DevTools console. Errors are logged for those issues that conflict with the
  // HTML specification. Warnings are logged for issues that cause problems with
  // identification of fields on the web-page for the Password Manager.
  // Warning and error messages are logged to |logger|.
  void AnalyseDocumentDOM(blink::WebLocalFrame* frame,
                          PagePasswordsAnalyserLogger* logger);

  // By default, the analyser will log to the DevTools console.
  void AnalyseDocumentDOM(blink::WebLocalFrame* frame);

  std::set<blink::WebNode> skip_nodes_;
  // This is true when new DOM content is available since the last time
  // the page was analysed, meaning the page needs to be reanalysed.
  bool page_dirty_;
};

}  // namespace autofill

#endif
   // COMPONENTS_AUTOFILL_CONTENT_RENDERER_PAGE_PASSWORDS_ANALYSER_H_
