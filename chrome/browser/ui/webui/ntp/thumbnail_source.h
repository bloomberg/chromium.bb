// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NTP_THUMBNAIL_SOURCE_H_
#define CHROME_BROWSER_UI_WEBUI_NTP_THUMBNAIL_SOURCE_H_

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
  ThumbnailSource(Profile* profile, bool capture_thumbnails);

  // content::URLDataSource implementation.
  virtual std::string GetSource() const OVERRIDE;
  virtual void StartDataRequest(
      const std::string& path,
      int render_process_id,
      int render_frame_id,
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

  // Only used when servicing requests on the UI thread.
  Profile* const profile_;

  // Indicate that, when a URL for which we don't have a thumbnail is requested
  // from this source, then Chrome should capture a thumbnail next time it
  // navigates to this URL. This is useful when the thumbnail URLs are generated
  // by an external service rather than TopSites, so Chrome can learn about the
  // URLs for which it should get thumbnails. Sources that capture thumbnails
  // are also be more lenient when matching thumbnail URLs by checking for
  // existing thumbnails in the database that contain a URL matching the prefix
  // of the requested URL.
  const bool capture_thumbnails_;

  DISALLOW_COPY_AND_ASSIGN(ThumbnailSource);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_THUMBNAIL_SOURCE_H_
