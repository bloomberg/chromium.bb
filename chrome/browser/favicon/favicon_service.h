// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAVICON_FAVICON_SERVICE_H_
#define CHROME_BROWSER_FAVICON_FAVICON_SERVICE_H_

#include <vector>

#include "base/callback.h"
#include "base/containers/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/common/favicon/favicon_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ui/base/layout.h"

class GURL;
class HistoryService;
struct ImportedFaviconUsage;
class Profile;

namespace chrome {
struct FaviconImageResult;
}

// The favicon service provides methods to access favicons. It calls the history
// backend behind the scenes.
class FaviconService : public KeyedService {
 public:
  explicit FaviconService(Profile* profile);

  virtual ~FaviconService();

  // Auxiliary argument structure for requesting favicons for URLs.
  struct FaviconForURLParams {
    FaviconForURLParams(const GURL& page_url,
                        int icon_types,
                        int desired_size_in_dip)
        : page_url(page_url),
          icon_types(icon_types),
          desired_size_in_dip(desired_size_in_dip) {}

    GURL page_url;
    int icon_types;
    int desired_size_in_dip;
  };

  // Callback for GetFaviconImage() and GetFaviconImageForURL().
  // |FaviconImageResult::image| is constructed from the bitmaps for the
  // passed in URL and icon types which most which closely match the passed in
  // |desired_size_in_dip| at the scale factors supported by the current
  // platform (eg MacOS) in addition to 1x.
  // |FaviconImageResult::icon_url| is the favicon that the favicon bitmaps in
  // |image| originate from.
  // TODO(pkotwicz): Enable constructing |image| from bitmaps from several
  // icon URLs.
  typedef base::Callback<void(const chrome::FaviconImageResult&)>
      FaviconImageCallback;

  // Callback for GetRawFavicon(), GetRawFaviconForURL() and
  // GetLargestRawFavicon().
  // See function for details on value.
  typedef base::Callback<void(const chrome::FaviconBitmapResult&)>
      FaviconRawCallback;

  // Callback for GetFavicon() and GetFaviconForURL().
  //
  // The first argument is the set of bitmaps for the passed in URL and
  // icon types whose pixel sizes best match the passed in
  // |desired_size_in_dip| at the scale factors supported by the current
  // platform (eg MacOS) in addition to 1x. The vector has at most one result
  // for each of the scale factors. There are less entries if a single result
  // is the best bitmap to use for several scale factors.
  typedef base::Callback<void(const std::vector<chrome::FaviconBitmapResult>&)>
      FaviconResultsCallback;

  // We usually pass parameters with pointer to avoid copy. This function is a
  // helper to run FaviconResultsCallback with pointer parameters.
  static void FaviconResultsCallbackRunner(
      const FaviconResultsCallback& callback,
      const std::vector<chrome::FaviconBitmapResult>* results);

  // Requests the favicon at |icon_url| of |icon_type| whose size most closely
  // matches |desired_size_in_dip|. If |desired_size_in_dip| is 0, the largest
  // favicon bitmap at |icon_url| is returned. |consumer| is notified when the
  // bits have been fetched. |icon_url| is the URL of the icon itself, e.g.
  // <http://www.google.com/favicon.ico>.
  // Each of the three methods below differs in the format of the callback and
  // the requested scale factors. All of the scale factors supported by the
  // current platform (eg MacOS) are requested for GetFaviconImage().
  base::CancelableTaskTracker::TaskId GetFaviconImage(
      const GURL& icon_url,
      chrome::IconType icon_type,
      int desired_size_in_dip,
      const FaviconImageCallback& callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId GetRawFavicon(
      const GURL& icon_url,
      chrome::IconType icon_type,
      int desired_size_in_dip,
      ui::ScaleFactor desired_scale_factor,
      const FaviconRawCallback& callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId GetFavicon(
      const GURL& icon_url,
      chrome::IconType icon_type,
      int desired_size_in_dip,
      const FaviconResultsCallback& callback,
      base::CancelableTaskTracker* tracker);

  // Set the favicon mappings to |page_url| for |icon_types| in the history
  // database.
  // Sample |icon_urls|:
  //  { ICON_URL1 -> TOUCH_ICON, known to the database,
  //    ICON_URL2 -> TOUCH_ICON, not known to the database,
  //    ICON_URL3 -> TOUCH_PRECOMPOSED_ICON, known to the database }
  // The new mappings are computed from |icon_urls| with these rules:
  // 1) Any urls in |icon_urls| which are not already known to the database are
  //    rejected.
  //    Sample new mappings to |page_url|: { ICON_URL1, ICON_URL3 }
  // 2) If |icon_types| has multiple types, the mappings are only set for the
  //    largest icon type.
  //    Sample new mappings to |page_url|: { ICON_URL3 }
  // |icon_types| can only have multiple IconTypes if
  // |icon_types| == TOUCH_ICON | TOUCH_PRECOMPOSED_ICON.
  // The favicon bitmaps which most closely match |desired_size_in_dip|
  // at the scale factors supported by the current platform (eg MacOS) in
  // addition to 1x from the favicons which were just mapped to |page_url| are
  // returned. If |desired_size_in_dip| is 0, the largest favicon bitmap is
  // returned.
  base::CancelableTaskTracker::TaskId UpdateFaviconMappingsAndFetch(
      const GURL& page_url,
      const std::vector<GURL>& icon_urls,
      int icon_types,
      int desired_size_in_dip,
      const FaviconResultsCallback& callback,
      base::CancelableTaskTracker* tracker);

  // Requests the favicons of any of |icon_types| whose pixel sizes most
  // closely match |desired_size_in_dip| and desired scale factors for a web
  // page URL. If |desired_size_in_dip| is 0, the largest favicon for the web
  // page URL is returned. |callback| is run when the bits have been fetched.
  // |icon_types| can be any combination of IconType value, but only one icon
  // will be returned in the priority of TOUCH_PRECOMPOSED_ICON, TOUCH_ICON and
  // FAVICON. Each of the three methods below differs in the format of the
  // callback and the requested scale factors. All of the scale factors
  // supported by the current platform (eg MacOS) are requested for
  // GetFaviconImageForURL().
  // Note. |callback| is always run asynchronously.
  base::CancelableTaskTracker::TaskId GetFaviconImageForURL(
      const FaviconForURLParams& params,
      const FaviconImageCallback& callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId GetRawFaviconForURL(
      const FaviconForURLParams& params,
      ui::ScaleFactor desired_scale_factor,
      const FaviconRawCallback& callback,
      base::CancelableTaskTracker* tracker);

  // See HistoryService::GetLargestFaviconForURL().
  base::CancelableTaskTracker::TaskId GetLargestRawFaviconForURL(
      Profile* profile,
      const GURL& page_url,
      const std::vector<int>& icon_types,
      int minimum_size_in_pixels,
      const FaviconRawCallback& callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId GetFaviconForURL(
      const FaviconForURLParams& params,
      const FaviconResultsCallback& callback,
      base::CancelableTaskTracker* tracker);

  // Used to request a bitmap for the favicon with |favicon_id| which is not
  // resized from the size it is stored at in the database. If there are
  // multiple favicon bitmaps for |favicon_id|, the largest favicon bitmap is
  // returned.
  base::CancelableTaskTracker::TaskId GetLargestRawFaviconForID(
      chrome::FaviconID favicon_id,
      const FaviconRawCallback& callback,
      base::CancelableTaskTracker* tracker);

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
      const std::vector<ImportedFaviconUsage>& favicon_usage);

  // Set the favicon for |page_url| for |icon_type| in the thumbnail database.
  // Unlike SetFavicons(), this method will not delete preexisting bitmap data
  // which is associated to |page_url| if at all possible. Use this method if
  // the favicon bitmaps for any of ui::GetSupportedScaleFactors() are not
  // known.
  void MergeFavicon(const GURL& page_url,
                    const GURL& icon_url,
                    chrome::IconType icon_type,
                    scoped_refptr<base::RefCountedMemory> bitmap_data,
                    const gfx::Size& pixel_size);

  // Set the favicon for |page_url| for |icon_type| in the thumbnail database.
  // |icon_url| is the single favicon to map to |page_url|. Mappings from
  // |page_url| to favicons at different icon URLs will be deleted.
  // A favicon bitmap is added for each image rep in |image|. Any preexisting
  // bitmap data for |icon_url| is deleted. It is important that |image|
  // contains image reps for all of ui::GetSupportedScaleFactors(). Use
  // MergeFavicon() if it does not.
  // TODO(pkotwicz): Save unresized favicon bitmaps to the database.
  // TODO(pkotwicz): Support adding favicons for multiple icon URLs to the
  // thumbnail database.
  void SetFavicons(const GURL& page_url,
                   const GURL& icon_url,
                   chrome::IconType icon_type,
                   const gfx::Image& image);

  // Avoid repeated requests to download missing favicon.
  void UnableToDownloadFavicon(const GURL& icon_url);
  bool WasUnableToDownloadFavicon(const GURL& icon_url) const;
  void ClearUnableToDownloadFavicons();

 private:
  typedef uint32 MissingFaviconURLHash;
  base::hash_set<MissingFaviconURLHash> missing_favicon_urls_;
  HistoryService* history_service_;
  Profile* profile_;

  // Helper function for GetFaviconImageForURL(), GetRawFaviconForURL() and
  // GetFaviconForURL().
  base::CancelableTaskTracker::TaskId GetFaviconForURLImpl(
      const FaviconForURLParams& params,
      const std::vector<ui::ScaleFactor>& desired_scale_factors,
      const FaviconResultsCallback& callback,
      base::CancelableTaskTracker* tracker);

  // Intermediate callback for GetFaviconImage() and GetFaviconImageForURL()
  // so that history service can deal solely with FaviconResultsCallback.
  // Builds chrome::FaviconImageResult from |favicon_bitmap_results| and runs
  // |callback|.
  void RunFaviconImageCallbackWithBitmapResults(
      const FaviconImageCallback& callback,
      int desired_size_in_dip,
      const std::vector<chrome::FaviconBitmapResult>& favicon_bitmap_results);

  // Intermediate callback for GetRawFavicon() and GetRawFaviconForURL()
  // so that history service can deal solely with FaviconResultsCallback.
  // Resizes chrome::FaviconBitmapResult if necessary and runs |callback|.
  void RunFaviconRawCallbackWithBitmapResults(
      const FaviconRawCallback& callback,
      int desired_size_in_dip,
      ui::ScaleFactor desired_scale_factor,
      const std::vector<chrome::FaviconBitmapResult>& favicon_bitmap_results);

  DISALLOW_COPY_AND_ASSIGN(FaviconService);
};

#endif  // CHROME_BROWSER_FAVICON_FAVICON_SERVICE_H_
