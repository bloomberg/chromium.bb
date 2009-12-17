// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cld_helper.h"

#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/common/notification_service.h"

#if defined(OS_WIN)
// TODO(port): The compact language detection library works only for Windows.
#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_unicodetext.h"
#endif

CLDHelper::CLDHelper(int process_id, int routing_id, int page_id,
                     const std::wstring& page_content)
    : process_id_(process_id),
      routing_id_(routing_id),
      page_id_(page_id),
      page_content_(page_content) {
}

void CLDHelper::DetectLanguage() {
  // Balanced in DetectionDone.
  AddRef();

  // Do the actual work on the file thread.
  ChromeThread::PostTask(ChromeThread::FILE, FROM_HERE,
                         NewRunnableMethod(this, &CLDHelper::DoDetectLanguage));
}

void CLDHelper::DoDetectLanguage() {
#if defined(OS_WIN)  // Only for windows.
// TODO(jcampan): http://crbug.com/30662 the detection language library crashes
//                on some sites and has been temporarily disabled.
#if 0
  int num_languages = 0;
  bool is_reliable = false;
  const char* language_iso_code = LanguageCodeISO639_1(
      DetectLanguageOfUnicodeText(page_content_.c_str(), true, &is_reliable,
                                  &num_languages, NULL));
  language_ = language_iso_code;
#endif
#else
  // TODO(jcampan): port the compact language detection library.
  NOTIMPLEMENTED();
#endif

  // Work is done, notify the RenderViewHost on the UI thread.
  ChromeThread::PostTask(ChromeThread::UI, FROM_HERE,
                         NewRunnableMethod(this, &CLDHelper::DetectionDone));
}

void CLDHelper::DetectionDone() {
  RenderViewHost* rvh = RenderViewHost::FromID(process_id_, routing_id_);
  // The RenderViewHost might be gone (tab closed for example) while we were
  // performing the detection.
  if (rvh) {
    NotificationService::current()->Notify(
        NotificationType::TAB_LANGUAGE_DETERMINED,
        Source<RenderViewHost>(rvh),
        Details<std::string>(&language_));
  }

  // Remove the ref we added to ourself in DetectLanguage().
  Release();
}
