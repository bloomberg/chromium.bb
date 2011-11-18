// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAVICON_FAVICON_SERVICE_H_
#define CHROME_BROWSER_FAVICON_FAVICON_SERVICE_H_
#pragma once

#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/task.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/common/ref_counted_util.h"
#include "content/browser/cancelable_request.h"

class GURL;
class Profile;

// The favicon service provides methods to access favicons. It calls the history
// backend behind the scenes.
//
// This service is thread safe. Each request callback is invoked in the
// thread that made the request.
class FaviconService : public CancelableRequestProvider {
 public:
  explicit FaviconService(Profile* profile);

  virtual ~FaviconService();

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
  typedef base::Callback<
      void(Handle,  // handle
           history::FaviconData)>  // the type of favicon
      FaviconDataCallback;

  typedef CancelableRequest<FaviconDataCallback> GetFaviconRequest;

  // Requests the |icon_type| of favicon. |consumer| is notified when the bits
  // have been fetched. |icon_url| is the URL of the icon itself, e.g.
  // <http://www.google.com/favicon.ico>.
  Handle GetFavicon(const GURL& icon_url,
                    history::IconType icon_type,
                    CancelableRequestConsumerBase* consumer,
                    const FaviconDataCallback& callback);

  // Fetches the |icon_type| of favicon at |icon_url|, sending the results to
  // the given |callback|. If the favicon has previously been set via
  // SetFavicon(), then the favicon URL for |page_url| and all redirects is set
  // to |icon_url|. If the favicon has not been set, the database is not
  // updated.
  Handle UpdateFaviconMappingAndFetch(const GURL& page_url,
                                      const GURL& icon_url,
                                      history::IconType icon_type,
                                      CancelableRequestConsumerBase* consumer,
                                      const FaviconDataCallback& callback);

  // Requests any |icon_types| of favicon for a web page URL. |consumer| is
  // notified when the bits have been fetched. |icon_types| can be any
  // combination of IconType value, but only one icon will be returned in the
  // priority of TOUCH_PRECOMPOSED_ICON, TOUCH_ICON and FAVICON.
  //
  // Note: this version is intended to be used to retrieve the favicon of a
  // page that has been browsed in the past. |expired| in the callback is
  // always false.
  Handle GetFaviconForURL(const GURL& page_url,
                          int icon_types,
                          CancelableRequestConsumerBase* consumer,
                          const FaviconDataCallback& callback);

  // Marks all types of favicon for the page as being out of date.
  void SetFaviconOutOfDateForPage(const GURL& page_url);

  // Clones all icons from an existing page. This associates the icons from
  // |old_page_url| with |new_page_url|, provided |new_page_url| has no
  // recorded associations to any other icons.
  // Needed if you want to declare favicons (tentatively) in advance, before a
  // page is ever visited.
  void CloneFavicon(const GURL& old_page_url, const GURL& new_page_url);

  // Allows the importer to set many favicons for many pages at once. The pages
  // must exist, any favicon sets for unknown pages will be discarded. Existing
  // favicons will not be overwritten.
  void SetImportedFavicons(
      const std::vector<history::ImportedFaviconUsage>& favicon_usage);

  // Sets the favicon for a page.
  void SetFavicon(const GURL& page_url,
                  const GURL& icon_url,
                  const std::vector<unsigned char>& image_data,
                  history::IconType icon_type);

 private:


  Profile* profile_;

  // Helper to forward an empty result if we cannot get the history service.
  void ForwardEmptyResultAsync(GetFaviconRequest* request);

  DISALLOW_COPY_AND_ASSIGN(FaviconService);
};

#endif  // CHROME_BROWSER_FAVICON_FAVICON_SERVICE_H_
