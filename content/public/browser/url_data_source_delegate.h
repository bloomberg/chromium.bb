// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_URL_DATA_SOURCE_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_URL_DATA_SOURCE_DELEGATE_H_

#include <string>

#include "content/common/content_export.h"

class ChromeURLDataManager;
class ChromeURLDataManagerBackend;
class ChromeWebUIDataSource;
class MessageLoop;
class URLDataSource;

namespace content {

// An interface implemented by a creator of URLDataSource to customize its
// behavior.
class CONTENT_EXPORT URLDataSourceDelegate {
 public:
  URLDataSourceDelegate();
  virtual ~URLDataSourceDelegate();

  // The name of this source.
  // E.g., for favicons, this could be "favicon", which results in paths for
  // specific resources like "favicon/34" getting sent to this source.
  virtual std::string GetSource() = 0;

  // Called by URLDataSource to request data at |path|.  The delegate should
  // call URLDataSource::SendResponse() when the data is available or if the
  // request could not be satisfied.
  virtual void StartDataRequest(const std::string& path,
                                bool is_incognito,
                                int request_id) = 0;

  // Return the mimetype that should be sent with this response, or empty
  // string to specify no mime type.
  virtual std::string GetMimeType(const std::string& path) const = 0;

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

  void set_url_data_source_for_testing(URLDataSource* url_data_source) {
    url_data_source_ = url_data_source;
  }

 protected:
  URLDataSource* url_data_source() { return url_data_source_; }

 private:
  friend class ::ChromeURLDataManager;
  friend class ::ChromeURLDataManagerBackend;
  friend class ::ChromeWebUIDataSource;
  URLDataSource* url_data_source_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_URL_DATA_SOURCE_DELEGATE_H_
