// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_NET_TEST_URL_FETCHER_FACTORY_H_
#define CHROME_COMMON_NET_TEST_URL_FETCHER_FACTORY_H_
#pragma once

#include <list>
#include <map>
#include <string>
#include <utility>

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
//
// Note: if you don't know when your request objects will be created you
// might want to use the FakeUrlFetcher and FakeUrlFetcherFactory classes
// below.

class TestURLFetcher : public URLFetcher {
 public:
  TestURLFetcher(int id,
                 const GURL& url,
                 RequestType request_type,
                 Delegate* d);
  ~TestURLFetcher();

  // Overriden to do nothing. It is assumed the caller will notify the delegate.
  virtual void Start() {}

  // Overriden to cache the chunks uploaded. Caller can read back the uploaded
  // chunks with the upload_data() accessor.
  virtual void AppendChunkToUpload(const std::string& data, bool is_last_chunk);

  // Unique ID in our factory.
  int id() const { return id_; }

  // URL we were created with. Because of how we're using URLFetcher url()
  // always returns an empty URL. Chances are you'll want to use original_url()
  // in your tests.
  const GURL& original_url() const { return original_url_; }

  // Returns the data uploaded on this URLFetcher.
  const std::string& upload_data() const { return URLFetcher::upload_data(); }

  // Returns the chunks of data uploaded on this URLFetcher.
  const std::list<std::string>& upload_chunks() const { return chunks_; }

  // Returns the delegate installed on the URLFetcher.
  Delegate* delegate() const { return URLFetcher::delegate(); }

 private:
  const int id_;
  const GURL original_url_;
  std::list<std::string> chunks_;
  bool did_receive_last_chunk_;

  DISALLOW_COPY_AND_ASSIGN(TestURLFetcher);
};

// Simple URLFetcher::Factory method that creates TestURLFetchers. All fetchers
// are registered in a map by the id passed to the create method.
class TestURLFetcherFactory : public URLFetcher::Factory {
 public:
  TestURLFetcherFactory();
  virtual ~TestURLFetcherFactory();

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

// The FakeUrlFetcher and FakeUrlFetcherFactory classes are similar to the
// ones above but don't require you to know when exactly the URLFetcher objects
// will be created.
//
// These classes let you set pre-baked HTTP responses for particular URLs.
// E.g., if the user requests http://a.com/ then respond with an HTTP/500.
//
// We assume that the thread that is calling Start() on the URLFetcher object
// has a message loop running.
//
// This class is not thread-safe.  You should not call SetFakeResponse or
// ClearFakeResponse at the same time you call CreateURLFetcher.  However, it is
// OK to start URLFetcher objects while setting or clearning fake responses
// since already created URLFetcher objects will not be affected by any changes
// made to the fake responses (once a URLFetcher object is created you cannot
// change its fake response).
//
// Example usage:
//  FakeURLFetcherFactory factory;
//  URLFetcher::set_factory(&factory);
//
//  // You know that class SomeService will request url http://a.com/ and you
//  // want to test the service class by returning an error.
//  factory.SetFakeResponse("http://a.com/", "", false);
//  // But if the service requests http://b.com/asdf you want to respond with
//  // a simple html page and an HTTP/200 code.
//  factory.SetFakeResponse("http://b.com/asdf",
//                          "<html><body>hello world</body></html>",
//                          true);
//
//  SomeService service;
//  service.Run();  // Will eventually request these two URLs.

class FakeURLFetcherFactory : public URLFetcher::Factory {
 public:
  FakeURLFetcherFactory();
  virtual ~FakeURLFetcherFactory();

  // If no fake response is set for the given URL this method will return NULL.
  // Otherwise, it will return a URLFetcher object which will respond with the
  // pre-baked response that the client has set by calling SetFakeResponse().
  virtual URLFetcher* CreateURLFetcher(int id,
                                       const GURL& url,
                                       URLFetcher::RequestType request_type,
                                       URLFetcher::Delegate* d);

  // Sets the fake response for a given URL.  If success is true we will serve
  // an HTTP/200 and an HTTP/500 otherwise.  The |response_data| may be empty.
  void SetFakeResponse(const std::string& url,
                       const std::string& response_data,
                       bool success);

  // Clear all the fake responses that were previously set via
  // SetFakeResponse().
  void ClearFakeReponses();

 private:
  typedef std::map<GURL, std::pair<std::string, bool> > FakeResponseMap;
  FakeResponseMap fake_responses_;

  DISALLOW_COPY_AND_ASSIGN(FakeURLFetcherFactory);
};

#endif  // CHROME_COMMON_NET_TEST_URL_FETCHER_FACTORY_H_
