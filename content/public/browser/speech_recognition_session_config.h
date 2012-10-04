// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_SESSION_CONFIG_H_
#define CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_SESSION_CONFIG_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/public/browser/speech_recognition_session_context.h"
#include "content/public/common/speech_recognition_grammar.h"
#include "net/url_request/url_request_context_getter.h"

namespace content {

class SpeechRecognitionEventListener;

// Configuration params for creating a new speech recognition session.
struct CONTENT_EXPORT SpeechRecognitionSessionConfig {
  SpeechRecognitionSessionConfig();
  ~SpeechRecognitionSessionConfig();

  bool is_legacy_api;
  std::string language;
  SpeechRecognitionGrammarArray grammars;
  std::string origin_url;
  bool filter_profanities;
  bool continuous;
  bool interim_results;
  uint32 max_hypotheses;
  SpeechRecognitionSessionContext initial_context;
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter;
  SpeechRecognitionEventListener* event_listener;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_SESSION_CONFIG_H_
