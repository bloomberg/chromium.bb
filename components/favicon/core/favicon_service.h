// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_CORE_FAVICON_SERVICE_H_
#define COMPONENTS_FAVICON_CORE_FAVICON_SERVICE_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/favicon_base/favicon_callback.h"
#include "components/favicon_base/favicon_types.h"
#include "components/favicon_base/favicon_usage_data.h"
#include "components/keyed_service/core/keyed_service.h"

class GURL;

namespace favicon {

class FaviconService : public KeyedService {
 public:
  // We usually pass parameters with pointer to avoid copy. This function is a
  // helper to run FaviconResultsCallback with pointer parameters.
  static void FaviconResultsCallbackRunner(
      const favicon_base::FaviconResultsCallback& callback,
      const std::vector<favicon_base::FaviconRawBitmapResult>* results);

  //////////////////////////////////////////////////////////////////////////////
  // Methods to request favicon bitmaps from the history backend for |icon_url|.
  // |icon_url| is the URL of the icon itself.
  // (e.g. <http://www.google.com/favicon.ico>)

  // Requests the favicon at |icon_url| of type favicon_base::FAVICON and of
  // size gfx::kFaviconSize. The returned gfx::Image is populated with
  // representations for all of the scale factors supported by the platform
  // (e.g. MacOS). If data is unavailable for some or all of the scale factors,
  // the bitmaps with the best matching sizes are resized.
  virtual base::CancelableTaskTracker::TaskId GetFaviconImage(
      const GURL& icon_url,
      const favicon_base::FaviconImageCallback& callback,
      base::CancelableTaskTracker* tracker) = 0;

  // Requests the favicon at |icon_url| of |icon_type| of size
  // |desired_size_in_pixel|. If there is no favicon of size
  // |desired_size_in_pixel|, the favicon bitmap which best matches
  // |desired_size_in_pixel| is resized. If |desired_size_in_pixel| is 0,
  // the largest favicon bitmap is returned.
  virtual base::CancelableTaskTracker::TaskId GetRawFavicon(
      const GURL& icon_url,
      favicon_base::IconType icon_type,
      int desired_size_in_pixel,
      const favicon_base::FaviconRawBitmapCallback& callback,
      base::CancelableTaskTracker* tracker) = 0;

  // The first argument for |callback| is the set of bitmaps for the passed in
  // URL and icon types whose pixel sizes best match the passed in
  // |desired_size_in_dip| at the resource scale factors supported by the
  // current platform (eg MacOS) in addition to 1x. The vector has at most one
  // result for each of the resource scale factors. There are less entries if a
  // single/ result is the best bitmap to use for several resource scale
  // factors.
  virtual base::CancelableTaskTracker::TaskId GetFavicon(
      const GURL& icon_url,
      favicon_base::IconType icon_type,
      int desired_size_in_dip,
      const favicon_base::FaviconResultsCallback& callback,
      base::CancelableTaskTracker* tracker) = 0;

  //////////////////////////////////////////////////////////////////////////////
  // Methods to request favicon bitmaps from the history backend for |page_url|.
  // |page_url| is the web page the favicon is associated with.
  // (e.g. <http://www.google.com>)

  // Requests the favicon for the page at |page_url| of type
  // favicon_base::FAVICON and of size gfx::kFaviconSize. The returned
  // gfx::Image is populated with representations for all of the scale factors
  // supported by the platform (e.g. MacOS). If data is unavailable for some or
  // all of the scale factors, the bitmaps with the best matching sizes are
  // resized.
  virtual base::CancelableTaskTracker::TaskId GetFaviconImageForPageURL(
      const GURL& page_url,
      const favicon_base::FaviconImageCallback& callback,
      base::CancelableTaskTracker* tracker) = 0;

  // Requests the favicon for the page at |page_url| with one of |icon_types|
  // and with |desired_size_in_pixel|. |icon_types| can be any combination of
  // IconTypes. If favicon bitmaps for several IconTypes are available, the
  // favicon bitmap is chosen in the priority of TOUCH_PRECOMPOSED_ICON,
  // TOUCH_ICON and FAVICON. If there is no favicon bitmap of size
  // |desired_size_in_pixel|, the favicon bitmap which best matches
  // |desired_size_in_pixel| is resized. If |desired_size_in_pixel| is 0,
  // the largest favicon bitmap is returned. Results with a higher priority
  // IconType are preferred over an exact match of the favicon bitmap size.
  virtual base::CancelableTaskTracker::TaskId GetRawFaviconForPageURL(
      const GURL& page_url,
      int icon_types,
      int desired_size_in_pixel,
      const favicon_base::FaviconRawBitmapCallback& callback,
      base::CancelableTaskTracker* tracker) = 0;

  // See HistoryService::GetLargestFaviconForPageURL().
  virtual base::CancelableTaskTracker::TaskId GetLargestRawFaviconForPageURL(
      const GURL& page_url,
      const std::vector<int>& icon_types,
      int minimum_size_in_pixels,
      const favicon_base::FaviconRawBitmapCallback& callback,
      base::CancelableTaskTracker* tracker) = 0;

  virtual base::CancelableTaskTracker::TaskId GetFaviconForPageURL(
      const GURL& page_url,
      int icon_types,
      int desired_size_in_dip,
      const favicon_base::FaviconResultsCallback& callback,
      base::CancelableTaskTracker* tracker) = 0;

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
  // at the reosurce scale factors supported by the current platform (eg MacOS)
  // in addition to 1x from the favicons which were just mapped to |page_url|
  // are returned. If |desired_size_in_dip| is 0, the largest favicon bitmap is
  // returned.
  virtual base::CancelableTaskTracker::TaskId UpdateFaviconMappingsAndFetch(
      const GURL& page_url,
      const std::vector<GURL>& icon_urls,
      int icon_types,
      int desired_size_in_dip,
      const favicon_base::FaviconResultsCallback& callback,
      base::CancelableTaskTracker* tracker) = 0;

  // Used to request a bitmap for the favicon with |favicon_id| which is not
  // resized from the size it is stored at in the database. If there are
  // multiple favicon bitmaps for |favicon_id|, the largest favicon bitmap is
  // returned.
  virtual base::CancelableTaskTracker::TaskId GetLargestRawFaviconForID(
      favicon_base::FaviconID favicon_id,
      const favicon_base::FaviconRawBitmapCallback& callback,
      base::CancelableTaskTracker* tracker) = 0;

  // Marks all types of favicon for the page as being out of date.
  virtual void SetFaviconOutOfDateForPage(const GURL& page_url) = 0;

  // Allows the importer to set many favicons for many pages at once. The pages
  // must exist, any favicon sets for unknown pages will be discarded. Existing
  // favicons will not be overwritten.
  virtual void SetImportedFavicons(
      const favicon_base::FaviconUsageDataList& favicon_usage) = 0;

  // Set the favicon for |page_url| for |icon_type| in the thumbnail database.
  // Unlike SetFavicons(), this method will not delete preexisting bitmap data
  // which is associated to |page_url| if at all possible. Use this method if
  // the favicon bitmaps for any of ui::GetSupportedScaleFactors() are not
  // known.
  virtual void MergeFavicon(const GURL& page_url,
                            const GURL& icon_url,
                            favicon_base::IconType icon_type,
                            scoped_refptr<base::RefCountedMemory> bitmap_data,
                            const gfx::Size& pixel_size) = 0;

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
  virtual void SetFavicons(const GURL& page_url,
                           const GURL& icon_url,
                           favicon_base::IconType icon_type,
                           const gfx::Image& image) = 0;

  // Avoid repeated requests to download missing favicon.
  virtual void UnableToDownloadFavicon(const GURL& icon_url) = 0;
  virtual bool WasUnableToDownloadFavicon(const GURL& icon_url) const = 0;
  virtual void ClearUnableToDownloadFavicons() = 0;
};

}  // namespace favicon

#endif  // COMPONENTS_FAVICON_CORE_FAVICON_SERVICE_H_
