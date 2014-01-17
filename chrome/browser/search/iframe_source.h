// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_IFRAME_SOURCE_H_
#define CHROME_BROWSER_SEARCH_IFRAME_SOURCE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "content/public/browser/url_data_source.h"

// Base class for URL data sources for chrome-search:// iframed content.
class IframeSource : public content::URLDataSource {
 public:
  IframeSource();
  virtual ~IframeSource();

 protected:
  // Overridden from content::URLDataSource:
  virtual std::string GetMimeType(
      const std::string& path_and_query) const OVERRIDE;
  virtual bool ShouldDenyXFrameOptions() const OVERRIDE;
  virtual bool ShouldServiceRequest(
      const net::URLRequest* request) const OVERRIDE;

  // Returns whether this source should serve data for a particular path.
  virtual bool ServesPath(const std::string& path) const = 0;

  // Sends unmodified resource bytes.
  void SendResource(
      int resource_id,
      const content::URLDataSource::GotDataCallback& callback);

  // Sends Javascript with an expected postMessage origin interpolated.
  void SendJSWithOrigin(
      int resource_id,
      int render_process_id,
      int render_frame_id,
      const content::URLDataSource::GotDataCallback& callback);

  // This is exposed for testing and should not be overridden.
  // Sets |origin| to the URL of the render frame identified by |process_id| and
  // |render_frame_id|. Returns true if successful and false if not, for example
  // if the render frame does not exist.
  virtual bool GetOrigin(
      int process_id,
      int render_frame_id,
      std::string* origin) const;

  DISALLOW_COPY_AND_ASSIGN(IframeSource);
};

#endif  // CHROME_BROWSER_SEARCH_IFRAME_SOURCE_H_
