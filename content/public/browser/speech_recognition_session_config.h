// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_SESSION_CONFIG_H_
#define CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_SESSION_CONFIG_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/public/browser/speech_recognition_session_context.h"

namespace net {
class URLRequestContextGetter;
}

namespace content {

// Configuration params for creating a new speech recognition session.
struct CONTENT_EXPORT SpeechRecognitionSessionConfig {
  SpeechRecognitionSessionConfig();
  ~SpeechRecognitionSessionConfig();

  std::string language;
  std::string grammar;
  std::string origin_url;
  bool filter_profanities;
  SpeechRecognitionSessionContext initial_context;
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_SESSION_CONFIG_H_
