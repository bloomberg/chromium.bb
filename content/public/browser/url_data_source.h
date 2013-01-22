// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_URL_DATA_SOURCE_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_URL_DATA_SOURCE_DELEGATE_H_

#include <string>

#include "content/common/content_export.h"
#include "base/callback.h"

class MessageLoop;

namespace base {
class RefCountedMemory;
}

namespace content {

// A URLDataSource is an object that can answer requests for data
// asynchronously. An implementation of URLDataSource should handle calls to
// StartDataRequest() by starting its (implementation-specific) asynchronous
// request for the data, then running the callback given in that method to
// notify.
class CONTENT_EXPORT URLDataSource {
 public:
  // Used by StartDataRequest. The string parameter is the path of the request.
  // If the callee doesn't want to handle the data, false is returned. Otherwise
  // true is returned and the GotDataCallback parameter is called either then or
  // asynchronously with the response.
  typedef base::Callback<void(base::RefCountedMemory*)> GotDataCallback;

  virtual ~URLDataSource() {}

  // The name of this source.
  // E.g., for favicons, this could be "favicon", which results in paths for
  // specific resources like "favicon/34" getting sent to this source.
  virtual std::string GetSource() = 0;

  // Called by URLDataSource to request data at |path|.  The delegate should
  // run |callback| when the data is available or if the request could not be
  // satisfied.
  virtual void StartDataRequest(const std::string& path,
                                bool is_incognito,
                                const GotDataCallback& callback) = 0;

  // Return the mimetype that should be sent with this response, or empty
  // string to specify no mime type.
  virtual std::string GetMimeType(const std::string& path) const = 0;

  // The following methods are all called on the IO thread.

  // Returns the MessageLoop on which the delegate wishes to have
  // StartDataRequest called to handle the request for |path|. The default
  // implementation returns BrowserThread::UI. If the delegate does not care
  // which thread StartDataRequest is called on, this should return NULL. It may
  // be beneficial to return NULL for requests that are safe to handle directly
  // on the IO thread.  This can improve performance by satisfying such requests
  // more rapidly when there is a large amount of UI thread contention. Or the
  // delegate can return a specific thread's Messageloop if they wish.
  virtual MessageLoop* MessageLoopForRequestPath(const std::string& path) const;

  // Returns true if the URLDataSource should replace an existing URLDataSource
  // with the same name that has already been registered. The default is true.
  //
  // WARNING: this is invoked on the IO thread.
  //
  // TODO: nuke this and convert all callers to not replace.
  virtual bool ShouldReplaceExistingSource() const;

  // Returns true if responses from this URLDataSource can be cached.
  virtual bool AllowCaching() const;

  // If you are overriding this, then you have a bug.
  // It is not acceptable to disable content-security-policy on chrome:// pages
  // to permit functionality excluded by CSP, such as inline script.
  // Instead, you must go back and change your WebUI page so that it is
  // compliant with the policy. This typically involves ensuring that all script
  // is delivered through the data manager backend. Talk to tsepez for more
  // info.
  virtual bool ShouldAddContentSecurityPolicy() const;

  // It is OK to override the following two methods to a custom CSP directive
  // thereby slightly reducing the protection applied to the page.

  // By default, "object-src 'none';" is added to CSP. Override to change this.
  virtual std::string GetContentSecurityPolicyObjectSrc() const;
  // By default, "frame-src 'none';" is added to CSP. Override to change this.
  virtual std::string GetContentSecurityPolicyFrameSrc() const;

  // By default, the "X-Frame-Options: DENY" header is sent. To stop this from
  // happening, return false. It is OK to return false as needed.
  virtual bool ShouldDenyXFrameOptions() const;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_URL_DATA_SOURCE_DELEGATE_H_
