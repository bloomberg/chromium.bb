// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPEECH_SPEECH_RECOGNITION_REQUEST_H_
#define CONTENT_BROWSER_SPEECH_SPEECH_RECOGNITION_REQUEST_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/common/net/url_fetcher.h"
#include "chrome/common/speech_input_result.h"
#include "googleurl/src/gurl.h"

class URLFetcher;
class URLRequestContextGetter;

namespace speech_input {

// Provides a simple interface for sending recorded speech data to the server
// and get back recognition results.
class SpeechRecognitionRequest : public URLFetcher::Delegate {
 public:
  // ID passed to URLFetcher::Create(). Used for testing.
  static int url_fetcher_id_for_tests;

  // Interface for receiving callbacks from this object.
  class Delegate {
   public:
    virtual void SetRecognitionResult(
        bool error, const SpeechInputResultArray& result) = 0;

   protected:
    virtual ~Delegate() {}
  };

  // |url| is the server address to which the request wil be sent.
  SpeechRecognitionRequest(URLRequestContextGetter* context,
                           Delegate* delegate);

  virtual ~SpeechRecognitionRequest();

  // Sends a new request with the given audio data, returns true if successful.
  // The same object can be used to send multiple requests but only after the
  // previous request has completed.
  bool Send(const std::string& language,
            const std::string& grammar,
            const std::string& hardware_info,
            const std::string& origin_url,
            const std::string& content_type,
            const std::string& audio_data);

  bool HasPendingRequest() { return url_fetcher_ != NULL; }

  // URLFetcher::Delegate methods.
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const net::URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);

 private:
  scoped_refptr<URLRequestContextGetter> url_context_;
  Delegate* delegate_;
  scoped_ptr<URLFetcher> url_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(SpeechRecognitionRequest);
};

// This typedef is to workaround the issue with certain versions of
// Visual Studio where it gets confused between multiple Delegate
// classes and gives a C2500 error. (I saw this error on the try bots -
// the workaround was not needed for my machine).
typedef SpeechRecognitionRequest::Delegate SpeechRecognitionRequestDelegate;

}  // namespace speech_input

#endif  // CONTENT_BROWSER_SPEECH_SPEECH_RECOGNITION_REQUEST_H_
