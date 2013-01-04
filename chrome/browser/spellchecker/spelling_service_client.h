// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPELLCHECKER_SPELLING_SERVICE_CLIENT_H_
#define CHROME_BROWSER_SPELLCHECKER_SPELLING_SERVICE_CLIENT_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "net/url_request/url_fetcher_delegate.h"

class GURL;
class Profile;
class TextCheckClientDelegate;
struct SpellCheckResult;

namespace net {
class URLFetcher;
}  // namespace net

// A class that encapsulates a JSON-RPC call to the Spelling service to check
// text there. This class creates a JSON-RPC request, sends the request to the
// service with URLFetcher, parses a response from the service, and calls a
// provided callback method. When a user deletes this object before it finishes
// a JSON-RPC call, this class cancels the JSON-RPC call without calling the
// callback method. A simple usage is creating a SpellingServiceClient and
// calling its RequestTextCheck method as listed in the following snippet.
//
//   class MyClient {
//    public:
//     MyClient();
//     virtual ~MyClient();
//
//     void OnTextCheckComplete(
//         int tag,
//         bool success,
//         const std::vector<SpellCheckResult>& results) {
//       ...
//     }
//
//     void MyTextCheck(Profile* profile, const string16& text) {
//        client_.reset(new SpellingServiceClient);
//        client_->RequestTextCheck(profile, 0, text,
//            base::Bind(&MyClient::OnTextCheckComplete,
//                       base::Unretained(this));
//     }
//    private:
//     scoped_ptr<SpellingServiceClient> client_;
//   };
//
class SpellingServiceClient : public net::URLFetcherDelegate {
 public:
  // Service types provided by the Spelling service. The Spelling service
  // consists of a couple of backends:
  // * SUGGEST: Retrieving suggestions for a word (used by Google Search), and;
  // * SPELLCHECK: Spellchecking text (used by Google Docs).
  // This type is used for choosing a backend when sending a JSON-RPC request to
  // the service.
  enum ServiceType {
    SUGGEST = 1,
    SPELLCHECK = 2,
  };
  typedef base::Callback<void(
      bool /* success */,
      const string16& /* text */,
      const std::vector<SpellCheckResult>& /* results */)>
          TextCheckCompleteCallback;

  SpellingServiceClient();
  virtual ~SpellingServiceClient();

  // net::URLFetcherDelegate implementation.
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // Sends a text-check request to the Spelling service. When we send a request
  // to the Spelling service successfully, this function returns true. (This
  // does not mean the service finishes checking text successfully.) We will
  // call |callback| when we receive a text-check response from the service.
  bool RequestTextCheck(Profile* profile,
                        ServiceType type,
                        const string16& text,
                        const TextCheckCompleteCallback& callback);

  // Returns whether the specified service is available for the given profile.
  static bool IsAvailable(Profile* profile, ServiceType type);

 private:
  // Creates a URLFetcher object used for sending a JSON-RPC request. This
  // function is overriden by unit tests to prevent them from actually sending
  // requests to the Spelling service.
  virtual net::URLFetcher* CreateURLFetcher(const GURL& url);

  // Parses a JSON-RPC response from the Spelling service.
  bool ParseResponse(const std::string& data,
                     std::vector<SpellCheckResult>* results);

  // The URLFetcher object used for sending a JSON-RPC request.
  scoped_ptr<net::URLFetcher> fetcher_;

  // The callback function to be called when we receive a response from the
  // Spelling service and parse it.
  TextCheckCompleteCallback callback_;

  // The text checked by the Spelling service.
  string16 text_;
};

#endif  // CHROME_BROWSER_SPELLCHECKER_SPELLING_SERVICE_CLIENT_H_
