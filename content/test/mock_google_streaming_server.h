// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_SPEECH_MOCK_GOOGLE_STREAMING_SERVER_H_
#define CONTENT_TEST_SPEECH_MOCK_GOOGLE_STREAMING_SERVER_H_

#include "base/compiler_specific.h"
#include "content/public/common/speech_recognition_result.h"
#include "net/url_request/test_url_fetcher_factory.h"

namespace content {

struct SpeechRecognitionResult;

// Provides a mock implementation of the Google remote streaming speech
// recognition webservice, exploiting the TestURLFetcher to extract request
// parameters and provide forged JSON responses back to the client.
// It is intended for closing the server-side loop in speech tests that involve
// the GoogleStreamingRemoteEngine client.
class MockGoogleStreamingServer : public net::TestURLFetcherDelegateForTests {
 public:
  class Delegate {
   public:
    virtual void OnClientConnected() = 0;
    virtual void OnClientAudioUpload() = 0;
    virtual void OnClientAudioUploadComplete() = 0;
    virtual void OnClientDisconnected() = 0;
  };

  explicit MockGoogleStreamingServer(Delegate* delegate);
  virtual ~MockGoogleStreamingServer();

  // net::TestURLFetcherDelegateForTests implementation.
  virtual void OnRequestStart(int fetcher_id) OVERRIDE;
  virtual void OnChunkUpload(int fetcher_id) OVERRIDE;
  virtual void OnRequestEnd(int fetcher_id) OVERRIDE;

  void SimulateResult(const content::SpeechRecognitionResult& result);
  void SimulateServerFailure();
  void SimulateMalformedResponse();

  // Retrieves the language parmeter for the request (e.g., lang=en-US).
  const std::string& GetRequestLanguage() const;

  // Retrieves the grammar parmeter for the request (e.g., lm=http://url/g.xml).
  const std::string& GetRequestGrammar() const;

 private:
  void SimulateServerResponse(bool success, const std::string& http_response);
  net::TestURLFetcher* GetURLFetcher(bool downstream) const;

  Delegate* delegate_;
  int kDownstreamUrlFetcherId;
  int kUpstreamUrlFetcherId;
  net::TestURLFetcherFactory url_fetcher_factory_;

  // Request arguments extracted by the HTTP query string.
  std::string request_language;
  std::string request_grammar;

  DISALLOW_COPY_AND_ASSIGN(MockGoogleStreamingServer);
};

}  // namespace content

#endif  // CONTENT_TEST_SPEECH_MOCK_GOOGLE_STREAMING_SERVER_H_
