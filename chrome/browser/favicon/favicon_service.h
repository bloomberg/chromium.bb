// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAVICON_FAVICON_SERVICE_H_
#define CHROME_BROWSER_FAVICON_FAVICON_SERVICE_H_

#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/cancelable_request.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/common/ref_counted_util.h"
#include "ui/base/layout.h"

class GURL;
class HistoryService;
class Profile;

// The favicon service provides methods to access favicons. It calls the history
// backend behind the scenes.
//
// This service is thread safe. Each request callback is invoked in the
// thread that made the request.
class FaviconService : public CancelableRequestProvider,
                       public ProfileKeyedService {
 public:
  explicit FaviconService(HistoryService* history_service);

  virtual ~FaviconService();

  // Auxiliary argument structure for requesting favicons for URLs.
  struct FaviconForURLParams {
    FaviconForURLParams(Profile* profile,
                        const GURL& page_url,
                        int icon_types,
                        int desired_size_in_dip,
                        CancelableRequestConsumerBase* consumer)
        : profile(profile),
          page_url(page_url),
          icon_types(icon_types),
          desired_size_in_dip(desired_size_in_dip),
          consumer(consumer) {
    }

    Profile* profile;
    GURL page_url;
    int icon_types;
    int desired_size_in_dip;
    CancelableRequestConsumerBase* consumer;
  };

  // Callback for GetFaviconImage() and GetFaviconImageForURL().
  // |FaviconImageResult::image| is constructed from the bitmaps for the
  // passed in URL and icon types which most which closely match the passed in
  // |desired_size_in_dip| at the scale factors supported by the current
  // platform (eg MacOS).
  // |FaviconImageResult::icon_url| is the favicon that the favicon bitmaps in
  // |image| originate from.
  // TODO(pkotwicz): Enable constructing |image| from bitmaps from several
  // icon URLs.
  typedef base::Callback<void(Handle, const history::FaviconImageResult&)>
      FaviconImageCallback;

  // Callback for GetRawFavicon() and GetRawFaviconForURL().
  // FaviconBitmapResult::bitmap_data is the bitmap in the thumbnail database
  // for the passed in URL and icon types whose pixel size best matches the
  // passed in |desired_size_in_dip| and |desired_scale_factor|. Returns an
  // invalid history::FaviconBitmapResult if there are no matches.
  typedef base::Callback<void(Handle, const history::FaviconBitmapResult&)>
      FaviconRawCallback;

  // Callback for GetFavicon() and GetFaviconForURL().
  //
  // The second argument is the set of bitmaps for the passed in URL and
  // icon types whose pixel sizes best match the passed in
  // |desired_size_in_dip| and |desired_scale_factors|. The vector has at most
  // one result for each of |desired_scale_factors|. There are less entries if
  // a single result is the best bitmap to use for several scale factors.
  //
  // Third argument:
  // a) If the callback is called as a result of GetFaviconForURL():
  //    The third argument is a map of the icon URLs mapped to |page_url| to
  //    the sizes at which the favicon is available from the web.
  // b) If the callback is called as a result of GetFavicon() or
  //    UpdateFaviconMappingAndFetch():
  //    The third argument is a map with a single element with the passed in
  //    |icon_url| to the vector of sizes of the favicon bitmaps at that URL. If
  //    |icon_url| is not known to the history backend, an empty map is
  //    returned.
  // See history_types.h for more information about IconURLSizesMap.
  typedef base::Callback<
      void(Handle,  // handle
           std::vector<history::FaviconBitmapResult>,
           history::IconURLSizesMap)>
      FaviconResultsCallback;

  typedef CancelableRequest<FaviconResultsCallback> GetFaviconRequest;

  // Requests the favicon at |icon_url| of |icon_type| whose size most closely
  // matches |desired_size_in_dip|. If |desired_size_in_dip| is 0, the largest
  // favicon bitmap at |icon_url| is returned. |consumer| is notified when the
  // bits have been fetched. |icon_url| is the URL of the icon itself, e.g.
  // <http://www.google.com/favicon.ico>.
  // Each of the three methods below differs in the format of the callback and
  // the requested scale factors. All of the scale factors supported by the
  // current platform (eg MacOS) are requested for GetFaviconImage().
  Handle GetFaviconImage(const GURL& icon_url,
                         history::IconType icon_type,
                         int desired_size_in_dip,
                         CancelableRequestConsumerBase* consumer,
                         const FaviconImageCallback& callback);

  Handle GetRawFavicon(const GURL& icon_url,
                       history::IconType icon_type,
                       int desired_size_in_dip,
                       ui::ScaleFactor desired_scale_factor,
                       CancelableRequestConsumerBase* consumer,
                       const FaviconRawCallback& callback);

  Handle GetFavicon(const GURL& icon_url,
                    history::IconType icon_type,
                    int desired_size_in_dip,
                    const std::vector<ui::ScaleFactor>& desired_scale_factors,
                    CancelableRequestConsumerBase* consumer,
                    const FaviconResultsCallback& callback);

  // Fetches the |icon_type| of favicon at |icon_url|, sending the results to
  // the given |callback|. If the favicon has previously been set via
  // SetFavicon(), then the favicon URL for |page_url| and all redirects is set
  // to |icon_url|. If the favicon has not been set, the database is not
  // updated.
  Handle UpdateFaviconMappingAndFetch(const GURL& page_url,
                                      const GURL& icon_url,
                                      history::IconType icon_type,
                                      CancelableRequestConsumerBase* consumer,
                                      const FaviconResultsCallback& callback);

  // Requests the favicons of any of |icon_types| whose pixel sizes most
  // closely match |desired_size_in_dip| and desired scale factors for a web
  // page URL. If |desired_size_in_dip| is 0, the largest favicon for the web
  // page URL is returned. |consumer| is notified when the bits have been
  // fetched. |icon_types| can be any combination of IconType value, but only
  // one icon will be returned in the priority of TOUCH_PRECOMPOSED_ICON,
  // TOUCH_ICON and FAVICON. Each of the three methods below differs in the
  // format of the callback and the requested scale factors. All of the scale
  // factors supported by the current platform (eg MacOS) are requested for
  // GetFaviconImageForURL().
  Handle GetFaviconImageForURL(const FaviconForURLParams& params,
                               const FaviconImageCallback& callback);

  Handle GetRawFaviconForURL(const FaviconForURLParams& params,
                             ui::ScaleFactor desired_scale_factor,
                             const FaviconRawCallback& callback);

  Handle GetFaviconForURL(
      const FaviconForURLParams& params,
      const std::vector<ui::ScaleFactor>& desired_scale_factors,
      const FaviconResultsCallback& callback);

  // Used to request a bitmap for the favicon with |favicon_id| which is not
  // resized from the size it is stored at in the database. If there are
  // multiple favicon bitmaps for |favicon_id|, the largest favicon bitmap is
  // returned.
  Handle GetLargestRawFaviconForID(history::FaviconID favicon_id,
                                   CancelableRequestConsumerBase* consumer,
                                   const FaviconRawCallback& callback);

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
  HistoryService* history_service_;

  // Helper to forward an empty result if we cannot get the history service.
  void ForwardEmptyResultAsync(GetFaviconRequest* request);

  // Helper function for GetFaviconImageForURL(), GetRawFaviconForURL() and
  // GetFaviconForURL().
  Handle GetFaviconForURLImpl(
      const FaviconForURLParams& params,
      const std::vector<ui::ScaleFactor>& desired_scale_factors,
      GetFaviconRequest* request);

  // Intermediate callback for GetFaviconImage() and GetFaviconImageForURL()
  // so that history service can deal solely with FaviconResultsCallback.
  // Builds history::FaviconImageResult from |favicon_bitmap_results| and runs
  // |callback|.
  void GetFaviconImageCallback(
      int desired_size_in_dip,
      FaviconImageCallback callback,
      Handle handle,
      std::vector<history::FaviconBitmapResult> favicon_bitmap_results,
      history::IconURLSizesMap icon_url_sizes_map);

  // Intermediate callback for GetRawFavicon() and GetRawFaviconForURL()
  // so that history service can deal solely with FaviconResultsCallback.
  // Resizes history::FaviconBitmapResult if necessary and runs |callback|.
  void GetRawFaviconCallback(
      int desired_size_in_dip,
      ui::ScaleFactor desired_scale_factor,
      FaviconRawCallback callback,
      Handle handle,
      std::vector<history::FaviconBitmapResult> favicon_bitmap_results,
      history::IconURLSizesMap icon_url_sizes_map);

  DISALLOW_COPY_AND_ASSIGN(FaviconService);
};

#endif  // CHROME_BROWSER_FAVICON_FAVICON_SERVICE_H_
