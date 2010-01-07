// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CLD_HELPER_H_
#define CHROME_BROWSER_CLD_HELPER_H_

#include <string>

#include "base/ref_counted.h"

class TabContents;

// The CLDHelper class detects the language of a page, using the toolbar CLD
// library. (CLD stands for Compact Language Detection.)
// It should be created and the detection process started on the UI thread.
// The detection happens on the file thread and
// TabContents::PageLanguageDetected() is then called on the associated
// TabContents when the language has been detected.
class CLDHelper : public base::RefCountedThreadSafe<CLDHelper> {
 public:
  // |tab_contents| should reference the TabContents the page is related to.
  // |renderer_process_id|, |page_id| and |page_contents| are the id of the
  // render process, the id and contents of the page the language is queried.
  CLDHelper(TabContents* tab_contents,
            int renderer_process_id,
            int page_id,
            const std::wstring& page_content);

  // Starts the process of detecting the language of the page.
  void DetectLanguage();

  // Cancel any pending language detection, meaning that the TabContents will
  // not be notified.
  // This should be on the same thread as DetectLanguage was called.
  void CancelLanguageDetection();

  int renderer_process_id() const { return renderer_process_id_; }

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

  // The tab contents for which the language is being detected.
  TabContents* tab_contents_;

  // The id for the render process the page belongs to.
  int renderer_process_id_;

  // The id and content of the page for which the language is being detected.
  int page_id_;
  std::wstring page_content_;

  // The language that was detected.
  std::string language_;

  DISALLOW_COPY_AND_ASSIGN(CLDHelper);
};

#endif  // CHROME_BROWSER_CLD_HELPER_H_

