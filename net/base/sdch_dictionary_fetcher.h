// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(rdsmith): This class needs to be moved out to the net/ embedder and
// hooked into whatever mechanisms the embedder uses for authentication.
// Specifically, this class needs methods overriding
// URLRequest::Delegate::{OnAuthRequired,OnCertificateRequested} and can't
// implement them at the net/ layer.

#ifndef NET_BASE_SDCH_DICTIONARY_FETCHER_H_
#define NET_BASE_SDCH_DICTIONARY_FETCHER_H_

#include <queue>
#include <set>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "net/base/sdch_manager.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request.h"

namespace net {

class URLRequest;
class URLRequestThrottlerEntryInterface;

// This class implements the SdchFetcher interface. It queues requests
// for dictionaries and dispatches them serially, implementing
// the URLRequest::Delegate interface to handle callbacks (but see above
// TODO). It tracks all requests, only attempting to fetch each dictionary
// once.
class NET_EXPORT SdchDictionaryFetcher
    : public SdchFetcher,
      public URLRequest::Delegate,
      public base::NonThreadSafe {
 public:
  // The consumer must guarantee that |*consumer| and |*context| outlive
  // this object.
  SdchDictionaryFetcher(SdchFetcher::Delegate* consumer,
                        URLRequestContext* context);
  virtual ~SdchDictionaryFetcher();

  // Implementation of SdchFetcher methods.
  virtual void Schedule(const GURL& dictionary_url) OVERRIDE;
  virtual void Cancel() OVERRIDE;

  // Implementation of URLRequest::Delegate methods.
  virtual void OnResponseStarted(URLRequest* request) OVERRIDE;
  virtual void OnReadCompleted(URLRequest* request, int bytes_read) OVERRIDE;

 private:
  enum State {
    STATE_NONE,
    STATE_IDLE,
    STATE_REQUEST_STARTED,
    STATE_REQUEST_READING,
    STATE_REQUEST_COMPLETE,
  };

  // State machine implementation.
  int DoLoop(int rv);
  int DoDispatchRequest(int rv);
  int DoRequestStarted(int rv);
  int DoRead(int rv);
  int DoCompleteRequest(int rv);

  State next_state_;
  bool in_loop_;

  SdchFetcher::Delegate* const consumer_;

  // A queue of URLs that are being used to download dictionaries.
  std::queue<GURL> fetch_queue_;

  // The request and buffer used for getting the current dictionary
  // Both are null when a fetch is not in progress.
  scoped_ptr<URLRequest> current_request_;
  scoped_refptr<IOBuffer> buffer_;

  // The currently accumulating dictionary.
  std::string dictionary_;

  // Althought the SDCH spec does not preclude a server from using a single URL
  // to load several distinct dictionaries (by telling a client to load a
  // dictionary from an URL several times), current implementations seem to have
  // that 1-1 relationship (i.e., each URL points at a single dictionary, and
  // the dictionary content does not change over time, and hence is not worth
  // trying to load more than once). In addition, some dictionaries prove
  // unloadable only after downloading them (because they are too large?  ...or
  // malformed?). As a protective element, Chromium will *only* load a
  // dictionary at most once from a given URL (so that it doesn't waste
  // bandwidth trying repeatedly).
  // The following set lists all the dictionary URLs that we've tried to load,
  // so that we won't try to load from an URL more than once.
  // TODO(jar): Try to augment the SDCH proposal to include this restiction.
  std::set<GURL> attempted_load_;

  // Store the URLRequestContext associated with the owning SdchManager for
  // use while fetching.
  URLRequestContext* context_;

  base::WeakPtrFactory<SdchDictionaryFetcher> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SdchDictionaryFetcher);
};

}  // namespace net

#endif  // NET_BASE_SDCH_DICTIONARY_FETCHER_H_
