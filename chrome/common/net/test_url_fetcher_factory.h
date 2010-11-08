// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_NET_TEST_URL_FETCHER_FACTORY_H_
#define CHROME_COMMON_NET_TEST_URL_FETCHER_FACTORY_H_
#pragma once

#include <map>
#include <string>

#include "chrome/common/net/url_fetcher.h"
#include "googleurl/src/gurl.h"

// TestURLFetcher and TestURLFetcherFactory are used for testing consumers of
// URLFetcher. TestURLFetcherFactory is a URLFetcher::Factory that creates
// TestURLFetchers. TestURLFetcher::Start is overriden to do nothing. It is
// expected that you'll grab the delegate from the TestURLFetcher and invoke
// the callback method when appropriate. In this way it's easy to mock a
// URLFetcher.
// Typical usage:
//   // TestURLFetcher requires a MessageLoop:
//   MessageLoopForUI message_loop;
//   // And io_thread to release URLRequestContextGetter in URLFetcher::Core.
//   BrowserThread io_thread(BrowserThread::IO, &message_loop);
//   // Create and register factory.
//   TestURLFetcherFactory factory;
//   URLFetcher::set_factory(&factory);
//   // Do something that triggers creation of a URLFetcher.
//   TestURLFetcher* fetcher = factory.GetFetcherByID(expected_id);
//   DCHECK(fetcher);
//   // Notify delegate with whatever data you want.
//   fetcher->delegate()->OnURLFetchComplete(...);
//   // Make sure consumer of URLFetcher does the right thing.
//   ...
//   // Reset factory.
//   URLFetcher::set_factory(NULL);


class TestURLFetcher : public URLFetcher {
 public:
  TestURLFetcher(int id,
                 const GURL& url,
                 RequestType request_type,
                 Delegate* d);

  // Overriden to do nothing. It is assumed the caller will notify the delegate.
  virtual void Start() {}

  // Unique ID in our factory.
  int id() const { return id_; }

  // URL we were created with. Because of how we're using URLFetcher url()
  // always returns an empty URL. Chances are you'll want to use original_url()
  // in your tests.
  const GURL& original_url() const { return original_url_; }

  // Returns the data uploaded on this URLFetcher.
  const std::string& upload_data() const { return URLFetcher::upload_data(); }

  // Returns the delegate installed on the URLFetcher.
  Delegate* delegate() const { return URLFetcher::delegate(); }

 private:
  const int id_;
  const GURL original_url_;

  DISALLOW_COPY_AND_ASSIGN(TestURLFetcher);
};

// Simple URLFetcher::Factory method that creates TestURLFetchers. All fetchers
// are registered in a map by the id passed to the create method.
class TestURLFetcherFactory : public URLFetcher::Factory {
 public:
  TestURLFetcherFactory() {}

  virtual URLFetcher* CreateURLFetcher(int id,
                                       const GURL& url,
                                       URLFetcher::RequestType request_type,
                                       URLFetcher::Delegate* d);
  TestURLFetcher* GetFetcherByID(int id) const;
  void RemoveFetcherFromMap(int id);

 private:
  // Maps from id passed to create to the returned URLFetcher.
  typedef std::map<int, TestURLFetcher*> Fetchers;
  Fetchers fetchers_;

  DISALLOW_COPY_AND_ASSIGN(TestURLFetcherFactory);
};

#endif  // CHROME_COMMON_NET_TEST_URL_FETCHER_FACTORY_H_
