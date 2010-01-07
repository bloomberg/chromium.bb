// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cld_helper.h"

#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/notification_service.h"

#if defined(OS_WIN)
// TODO(port): The compact language detection library works only for Windows.
#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_unicodetext.h"
#endif

CLDHelper::CLDHelper(TabContents* tab_contents, int renderer_process_id,
                     int page_id, const std::wstring& page_content)
    : tab_contents_(tab_contents),
      renderer_process_id_(renderer_process_id),
      page_id_(page_id),
      page_content_(page_content) {
}

void CLDHelper::DetectLanguage() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  // Balanced in DetectionDone.
  AddRef();

  // Do the actual work on the file thread.
  ChromeThread::PostTask(ChromeThread::FILE, FROM_HERE,
                         NewRunnableMethod(this, &CLDHelper::DoDetectLanguage));
}

void CLDHelper::CancelLanguageDetection() {
  tab_contents_ = NULL;
}

void CLDHelper::DoDetectLanguage() {
  if (!tab_contents_) {
    // If we have already been canceled, no need to perform the detection.
    Release();  // Balance AddRef from DetectLanguage().
    return;
  }

#if defined(OS_WIN)  // Only for windows.
  int num_languages = 0;
  bool is_reliable = false;

  const char* language_iso_code = LanguageCodeISO639_1(
      DetectLanguageOfUnicodeText(NULL, page_content_.c_str(), true,
                                  &is_reliable, &num_languages, NULL));
  language_ = language_iso_code;
#else
  // TODO(jcampan): port the compact language detection library.
  NOTIMPLEMENTED();
#endif

  // Work is done, notify the RenderViewHost on the UI thread.
  ChromeThread::PostTask(ChromeThread::UI, FROM_HERE,
                         NewRunnableMethod(this, &CLDHelper::DetectionDone));
}

void CLDHelper::DetectionDone() {
  if (tab_contents_)
    tab_contents_->PageLanguageDetected();

  // Remove the ref we added to ourself in DetectLanguage().
  Release();
}
