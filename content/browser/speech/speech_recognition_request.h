// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPEECH_SPEECH_RECOGNITION_REQUEST_H_
#define CONTENT_BROWSER_SPEECH_SPEECH_RECOGNITION_REQUEST_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "content/public/common/url_fetcher_delegate.h"
#include "googleurl/src/gurl.h"

class URLFetcher;

namespace content {
struct SpeechInputResult;
}

namespace net {
class URLRequestContextGetter;
}

namespace speech_input {

// Provides a simple interface for sending recorded speech data to the server
// and get back recognition results.
class SpeechRecognitionRequest : public content::URLFetcherDelegate {
 public:
  // ID passed to URLFetcher::Create(). Used for testing.
  CONTENT_EXPORT static int url_fetcher_id_for_tests;

  // Interface for receiving callbacks from this object.
  class CONTENT_EXPORT Delegate {
   public:
    virtual void SetRecognitionResult(
        const content::SpeechInputResult& result) = 0;

   protected:
    virtual ~Delegate() {}
  };

  // |url| is the server address to which the request wil be sent.
  CONTENT_EXPORT SpeechRecognitionRequest(net::URLRequestContextGetter* context,
                                          Delegate* delegate);

  CONTENT_EXPORT virtual ~SpeechRecognitionRequest();

  // Sends a new request with the given audio data, returns true if successful.
  // The same object can be used to send multiple requests but only after the
  // previous request has completed.
  CONTENT_EXPORT void Start(const std::string& language,
                            const std::string& grammar,
                            bool filter_profanities,
                            const std::string& hardware_info,
                            const std::string& origin_url,
                            const std::string& content_type);

  // Send a single chunk of audio immediately to the server.
  CONTENT_EXPORT void UploadAudioChunk(const std::string& audio_data,
                                       bool is_last_chunk);

  CONTENT_EXPORT bool HasPendingRequest() { return url_fetcher_ != NULL; }

  // content::URLFetcherDelegate methods.
  virtual void OnURLFetchComplete(const content::URLFetcher* source) OVERRIDE;

 private:
  scoped_refptr<net::URLRequestContextGetter> url_context_;
  Delegate* delegate_;
  scoped_ptr<content::URLFetcher> url_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(SpeechRecognitionRequest);
};

// This typedef is to workaround the issue with certain versions of
// Visual Studio where it gets confused between multiple Delegate
// classes and gives a C2500 error. (I saw this error on the try bots -
// the workaround was not needed for my machine).
typedef SpeechRecognitionRequest::Delegate SpeechRecognitionRequestDelegate;

}  // namespace speech_input

#endif  // CONTENT_BROWSER_SPEECH_SPEECH_RECOGNITION_REQUEST_H_
