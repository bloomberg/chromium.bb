// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CLD_HELPER_H_
#define CHROME_BROWSER_CLD_HELPER_H_

#include <string>

#include "base/ref_counted.h"

// The CLDHelper class detects the language of a page, using the toolbar CLD
// detection library. (CLD stands for Compact Language Detection.)
// It should be created and the detection process started on the same thread.
// The detection happens on the file thread and a TAB_LANGUAGE_DETERMINED
// notification is sent (on the UI thread) when the language has been detected.
class CLDHelper : public base::RefCountedThreadSafe<CLDHelper> {
 public:
  // |process_id| and |routing_id| should reference the RenderViewHost this page
  // is related to. |page_id| is the id of the page for the specified
  // |page_contents|.
  CLDHelper(int process_id,
            int routing_id,
            int page_id,
            const std::wstring& page_content);

  // Starts the process of detecting the language of the page.
  void DetectLanguage();

  int page_id() const { return page_id_; }

  std::string language() const { return language_; }

 private:
  // Private because the class is ref counted.
  friend class base::RefCountedThreadSafe<CLDHelper>;
  ~CLDHelper() {}

  // Performs the actual work of detecting the language.
  void DoDetectLanguage();

  // Invoked on the UI thread once the language has been detected.
  void DetectionDone();

  // The process id and routing id of the RenderViewHost for which the language
  // is being detected.
  int process_id_;
  int routing_id_;

  // The id and content of the page for which the language is being detected.
  int page_id_;
  std::wstring page_content_;

  // The language that was detected.
  std::string language_;

  DISALLOW_COPY_AND_ASSIGN(CLDHelper);
};

#endif  // CHROME_BROWSER_CLD_HELPER_H_

