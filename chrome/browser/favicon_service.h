// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAVICON_SERVICE_H__
#define CHROME_BROWSER_FAVICON_SERVICE_H__
#pragma once

#include <vector>

#include "base/ref_counted.h"
#include "base/ref_counted_memory.h"
#include "base/task.h"
#include "chrome/browser/cancelable_request.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/common/ref_counted_util.h"

class GURL;
class Profile;

// The favicon service provides methods to access favicons. It calls the history
// backend behind the scenes.
//
// This service is thread safe. Each request callback is invoked in the
// thread that made the request.
class FaviconService : public CancelableRequestProvider,
                       public base::RefCountedThreadSafe<FaviconService> {
 public:
  explicit FaviconService(Profile* profile);

  // Callback for GetFavicon. If we have previously inquired about the favicon
  // for this URL, |know_favicon| will be true, and the rest of the fields will
  // be valid (otherwise they will be ignored).
  //
  // On |know_favicon| == true, |data| will either contain the PNG encoded
  // favicon data, or it will be NULL to indicate that the site does not have
  // a favicon (in other words, we know the site doesn't have a favicon, as
  // opposed to not knowing anything). |expired| will be set to true if we
  // refreshed the favicon "too long" ago and should be updated if the page
  // is visited again.
  typedef Callback5<Handle,                           // handle
                    bool,                             // know_favicon
                    scoped_refptr<RefCountedMemory>,  // data
                    bool,                             // expired
                    GURL>::Type                       // url of the favicon
                    FaviconDataCallback;

  typedef CancelableRequest<FaviconDataCallback> GetFaviconRequest;

  // Requests the favicon. |consumer| is notified when the bits have been
  // fetched.
  Handle GetFavicon(const GURL& icon_url,
                    CancelableRequestConsumerBase* consumer,
                    FaviconDataCallback* callback);

  // Fetches the favicon at |icon_url|, sending the results to the given
  // |callback|. If the favicon has previously been set via SetFavicon(), then
  // the favicon URL for |page_url| and all redirects is set to |icon_url|. If
  // the favicon has not been set, the database is not updated.
  Handle UpdateFaviconMappingAndFetch(const GURL& page_url,
                                      const GURL& icon_url,
                                      CancelableRequestConsumerBase* consumer,
                                      FaviconDataCallback* callback);

  // Requests a favicon for a web page URL. |consumer| is notified
  // when the bits have been fetched.
  //
  // Note: this version is intended to be used to retrieve the favicon of a
  // page that has been browsed in the past. |expired| in the callback is
  // always false.
  Handle GetFaviconForURL(const GURL& page_url,
                          CancelableRequestConsumerBase* consumer,
                          FaviconDataCallback* callback);

  // Marks the favicon for the page as being out of date.
  void SetFaviconOutOfDateForPage(const GURL& page_url);

  // Allows the importer to set many favicons for many pages at once. The pages
  // must exist, any favicon sets for unknown pages will be discarded. Existing
  // favicons will not be overwritten.
  void SetImportedFavicons(
      const std::vector<history::ImportedFavIconUsage>& favicon_usage);

  // Sets the favicon for a page.
  void SetFavicon(const GURL& page_url,
                  const GURL& icon_url,
                  const std::vector<unsigned char>& image_data);

 private:
  friend class base::RefCountedThreadSafe<FaviconService>;

  ~FaviconService();

  Profile* profile_;

  // Helper to forward an empty result if we cannot get the history service.
  void ForwardEmptyResultAsync(GetFaviconRequest* request);

  DISALLOW_COPY_AND_ASSIGN(FaviconService);
};

#endif  // CHROME_BROWSER_FAVICON_SERVICE_H__
