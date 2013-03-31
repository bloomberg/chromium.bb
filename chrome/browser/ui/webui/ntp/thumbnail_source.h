// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NTP_THUMBNAIL_SOURCE_H_
#define CHROME_BROWSER_UI_WEBUI_NTP_THUMBNAIL_SOURCE_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/url_data_source.h"

class Profile;

namespace base {
class RefCountedMemory;
}

namespace thumbnails {
class ThumbnailService;
}

// ThumbnailSource is the gateway between network-level chrome: requests for
// thumbnails and the history/top-sites backend that serves these.
class ThumbnailSource : public content::URLDataSource {
 public:
  explicit ThumbnailSource(Profile* profile);

  // content::URLDataSource implementation.
  virtual std::string GetSource() OVERRIDE;
  virtual void StartDataRequest(
      const std::string& path,
      bool is_incognito,
      const content::URLDataSource::GotDataCallback& callback) OVERRIDE;
  virtual std::string GetMimeType(const std::string& path) const OVERRIDE;
  virtual base::MessageLoop* MessageLoopForRequestPath(
      const std::string& path) const OVERRIDE;
  virtual bool ShouldServiceRequest(
      const net::URLRequest* request) const OVERRIDE;

 private:
  virtual ~ThumbnailSource();

  // Raw PNG representation of the thumbnail to show when the thumbnail
  // database doesn't have a thumbnail for a webpage.
  scoped_refptr<base::RefCountedMemory> default_thumbnail_;

  // ThumbnailService.
  scoped_refptr<thumbnails::ThumbnailService> thumbnail_service_;

  // Transient mappings from an ID-based path to an URL-based path.
  // The key is an ID-string, the value is a URL string.
  // Mappings are added in ShouldServiceRequest() and erased once
  // the request is serviced in StartDataRequest().
  // TODO(dhollowa): Consider passing the |request| object through
  // to the StartDataRequest() call.
  mutable std::map<std::string, std::string> id_to_url_map_;

  // Only used when servicing requests on the UI thread.
  Profile* const profile_;

  DISALLOW_COPY_AND_ASSIGN(ThumbnailSource);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_THUMBNAIL_SOURCE_H_
