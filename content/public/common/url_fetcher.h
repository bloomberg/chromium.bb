// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_URL_FETCHER_H_
#define CONTENT_PUBLIC_COMMON_URL_FETCHER_H_
#pragma once

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/platform_file.h"
#include "content/common/content_export.h"

class FilePath;
class GURL;

namespace base {
class MessageLoopProxy;
class TimeDelta;
}

namespace net {
class HostPortPair;
class HttpRequestHeaders;
class HttpResponseHeaders;
class URLRequestContextGetter;
class URLRequestStatus;
typedef std::vector<std::string> ResponseCookies;
}

namespace content {

class URLFetcherDelegate;

// To use this class, create an instance with the desired URL and a pointer to
// the object to be notified when the URL has been loaded:
//   URLFetcher* fetcher = URLFetcher::Create("http://www.google.com",
//                                            URLFetcher::GET, this);
//
// Then, optionally set properties on this object, like the request context or
// extra headers:
//   fetcher->set_extra_request_headers("X-Foo: bar");
//
// Finally, start the request:
//   fetcher->Start();
//
//
// The object you supply as a delegate must inherit from
// URLFetcherDelegate; when the fetch is completed,
// OnURLFetchComplete() will be called with a pointer to the URLFetcher.  From
// that point until the original URLFetcher instance is destroyed, you may use
// accessor methods to see the result of the fetch. You should copy these
// objects if you need them to live longer than the URLFetcher instance. If the
// URLFetcher instance is destroyed before the callback happens, the fetch will
// be canceled and no callback will occur.
//
// You may create the URLFetcher instance on any thread; OnURLFetchComplete()
// will be called back on the same thread you use to create the instance.
//
//
// NOTE: By default URLFetcher requests are NOT intercepted, except when
// interception is explicitly enabled in tests.
class CONTENT_EXPORT URLFetcher {
 public:
  // Imposible http response code. Used to signal that no http response code
  // was received.
  enum ResponseCode {
    RESPONSE_CODE_INVALID = -1
  };

  enum RequestType {
    GET,
    POST,
    HEAD,
    DELETE_REQUEST,   // DELETE is already taken on Windows.
                      // <winnt.h> defines a DELETE macro.
  };

  // |url| is the URL to send the request to.
  // |request_type| is the type of request to make.
  // |d| the object that will receive the callback on fetch completion.
  static URLFetcher* Create(const GURL& url,
                            RequestType request_type,
                            URLFetcherDelegate* d);

  // Like above, but if there's a URLFetcherFactory registered with the
  // implementation it will be used. |id| may be used during testing to identify
  // who is creating the URLFetcher.
  static URLFetcher* Create(int id,
                            const GURL& url,
                            RequestType request_type,
                            content::URLFetcherDelegate* d);

  // Cancels all existing URLFetchers.  Will notify the URLFetcherDelegates.
  // Note that any new URLFetchers created while this is running will not be
  // cancelled.  Typically, one would call this in the CleanUp() method of an IO
  // thread, so that no new URLRequests would be able to start on the IO thread
  // anyway.  This doesn't prevent new URLFetchers from trying to post to the IO
  // thread though, even though the task won't ever run.
  static void CancelAll();

  // Normally interception is disabled for URLFetcher, but you can use this
  // to enable it for tests. Also see ScopedURLFetcherFactory for another way
  // of testing code that uses an URLFetcher.
  static void SetEnableInterceptionForTests(bool enabled);

  virtual ~URLFetcher() {}

  // Sets data only needed by POSTs.  All callers making POST requests should
  // call this before the request is started.  |upload_content_type| is the MIME
  // type of the content, while |upload_content| is the data to be sent (the
  // Content-Length header value will be set to the length of this data).
  virtual void SetUploadData(const std::string& upload_content_type,
                             const std::string& upload_content) = 0;

  // Indicates that the POST data is sent via chunked transfer encoding.
  // This may only be called before calling Start().
  // Use AppendChunkToUpload() to give the data chunks after calling Start().
  virtual void SetChunkedUpload(const std::string& upload_content_type) = 0;

  // Adds the given bytes to a request's POST data transmitted using chunked
  // transfer encoding.
  // This method should be called ONLY after calling Start().
  virtual void AppendChunkToUpload(const std::string& data,
                                   bool is_last_chunk) = 0;

  // Set one or more load flags as defined in net/base/load_flags.h.  Must be
  // called before the request is started.
  virtual void SetLoadFlags(int load_flags) = 0;

  // Returns the current load flags.
  virtual int GetLoadFlags() const = 0;

  // The referrer URL for the request. Must be called before the request is
  // started.
  virtual void SetReferrer(const std::string& referrer) = 0;

  // Set extra headers on the request.  Must be called before the request
  // is started.
  virtual void SetExtraRequestHeaders(
      const std::string& extra_request_headers) = 0;

  virtual void GetExtraRequestHeaders(net::HttpRequestHeaders* headers) = 0;

  // Set the net::URLRequestContext on the request.  Must be called before the
  // request is started.
  virtual void SetRequestContext(
      net::URLRequestContextGetter* request_context_getter) = 0;

  // If |retry| is false, 5xx responses will be propagated to the observer,
  // if it is true URLFetcher will automatically re-execute the request,
  // after backoff_delay() elapses. URLFetcher has it set to true by default.
  virtual void SetAutomaticallyRetryOn5xx(bool retry) = 0;

  virtual void SetMaxRetries(int max_retries) = 0;
  virtual int GetMaxRetries() const = 0;

  // Returns the back-off delay before the request will be retried,
  // when a 5xx response was received.
  virtual base::TimeDelta GetBackoffDelay() const = 0;

  // By default, the response is saved in a string. Call this method to save the
  // response to a temporary file instead. Must be called before Start().
  // |file_message_loop_proxy| will be used for all file operations.
  virtual void SaveResponseToTemporaryFile(
      scoped_refptr<base::MessageLoopProxy> file_message_loop_proxy) = 0;

  // Retrieve the response headers from the request.  Must only be called after
  // the OnURLFetchComplete callback has run.
  virtual net::HttpResponseHeaders* GetResponseHeaders() const = 0;

  // Retrieve the remote socket address from the request.  Must only
  // be called after the OnURLFetchComplete callback has run and if
  // the request has not failed.
  virtual net::HostPortPair GetSocketAddress() const = 0;

  // Returns true if the request was delivered through a proxy.  Must only
  // be called after the OnURLFetchComplete callback has run and the request
  // has not failed.
  virtual bool WasFetchedViaProxy() const = 0;

  // Start the request.  After this is called, you may not change any other
  // settings.
  virtual void Start() = 0;

  // Restarts the URLFetcher with a new URLRequestContextGetter.
  virtual void StartWithRequestContextGetter(
      net::URLRequestContextGetter* request_context_getter) = 0;

  // Return the URL that we were asked to fetch.
  virtual const GURL& GetOriginalURL() const = 0;

  // Return the URL that this fetcher is processing.
  virtual const GURL& GetURL() const = 0;

  // The status of the URL fetch.
  virtual const net::URLRequestStatus& GetStatus() const = 0;

  // The http response code received. Will return RESPONSE_CODE_INVALID
  // if an error prevented any response from being received.
  virtual int GetResponseCode() const = 0;

  // Cookies recieved.
  virtual const net::ResponseCookies& GetCookies() const = 0;

  // Return true if any file system operation failed.  If so, set |error_code|
  // to the error code. File system errors are only possible if user called
  // SaveResponseToTemporaryFile().
  virtual bool FileErrorOccurred(
      base::PlatformFileError* out_error_code) const = 0;

  // Reports that the received content was malformed.
  virtual void ReceivedContentWasMalformed() = 0;

  // Get the response as a string. Return false if the fetcher was not
  // set to store the response as a string.
  virtual bool GetResponseAsString(std::string* out_response_string) const = 0;

  // Get the path to the file containing the response body. Returns false
  // if the response body was not saved to a file. If take_ownership is
  // true, caller takes responsibility for the temp file, and it will not
  // be removed once the URLFetcher is destroyed.  User should not take
  // ownership more than once, or call this method after taking ownership.
  virtual bool GetResponseAsFilePath(bool take_ownership,
                                     FilePath* out_response_path) const = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_URL_FETCHER_H_
