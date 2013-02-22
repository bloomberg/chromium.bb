// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_tab_helper.h"

#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/time.h"
#include "chrome/browser/prerender/prerender_histograms.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/rect.h"

using content::WebContents;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(prerender::PrerenderTabHelper);

namespace prerender {

// Helper class to compute pixel-based stats on the paint progress
// between when a prerendered page is swapped in and when the onload event
// fires.
class PrerenderTabHelper::PixelStats {
 public:
  explicit PixelStats(PrerenderTabHelper* tab_helper) :
      bitmap_web_contents_(NULL),
      weak_factory_(this),
      tab_helper_(tab_helper) {
  }

  // Reasons why we need to fetch bitmaps: either a prerender was swapped in,
  // or a prerendered page has finished loading.
  enum BitmapType {
      BITMAP_SWAP_IN,
      BITMAP_ON_LOAD
  };

  void GetBitmap(BitmapType bitmap_type, WebContents* web_contents) {
    if (bitmap_type == BITMAP_SWAP_IN) {
      bitmap_.reset();
      bitmap_web_contents_ = web_contents;
    }

    if (bitmap_type == BITMAP_ON_LOAD && bitmap_web_contents_ != web_contents)
      return;

    if (!web_contents || !web_contents->GetView() ||
        !web_contents->GetRenderViewHost()) {
      return;
    }

    web_contents->GetRenderViewHost()->CopyFromBackingStore(
        gfx::Rect(),
        gfx::Size(),
        base::Bind(&PrerenderTabHelper::PixelStats::HandleBitmapResult,
                   weak_factory_.GetWeakPtr(),
                   bitmap_type,
                   web_contents));
  }

 private:
  void HandleBitmapResult(BitmapType bitmap_type,
                          WebContents* web_contents,
                          bool succeeded,
                          const SkBitmap& canvas_bitmap) {
    scoped_ptr<SkBitmap> bitmap;
    if (succeeded) {
      // TODO(nick): This copy may now be unnecessary.
      bitmap.reset(new SkBitmap());
      canvas_bitmap.copyTo(bitmap.get(), SkBitmap::kARGB_8888_Config);
    }

    if (bitmap_web_contents_ != web_contents)
      return;

    if (bitmap_type == BITMAP_SWAP_IN)
      bitmap_.swap(bitmap);

    if (bitmap_type == BITMAP_ON_LOAD) {
      PrerenderManager* prerender_manager =
          tab_helper_->MaybeGetPrerenderManager();
      if (prerender_manager) {
        prerender_manager->RecordFractionPixelsFinalAtSwapin(
            web_contents, CompareBitmaps(bitmap_.get(), bitmap.get()));
      }
      bitmap_.reset();
      bitmap_web_contents_ = NULL;
    }
  }

  // Helper comparing two bitmaps of identical size.
  // Returns a value < 0.0 if there is an error, and otherwise, a double in
  // [0, 1] indicating the fraction of pixels that are the same.
  double CompareBitmaps(SkBitmap* bitmap1, SkBitmap* bitmap2) {
    if (!bitmap1 || !bitmap2) {
      return -2.0;
    }
    if (bitmap1->width() != bitmap2->width() ||
        bitmap1->height() != bitmap2->height()) {
      return -1.0;
    }
    int pixels = bitmap1->width() * bitmap1->height();
    int same_pixels = 0;
    for (int y = 0; y < bitmap1->height(); ++y) {
      for (int x = 0; x < bitmap1->width(); ++x) {
        if (bitmap1->getColor(x, y) == bitmap2->getColor(x, y))
          same_pixels++;
      }
    }
    return static_cast<double>(same_pixels) / static_cast<double>(pixels);
  }

  // Bitmap of what the last swapped in prerendered tab looked like at swapin,
  // and the WebContents that it was swapped into.
  scoped_ptr<SkBitmap> bitmap_;
  WebContents* bitmap_web_contents_;

  base::WeakPtrFactory<PixelStats> weak_factory_;

  PrerenderTabHelper* tab_helper_;
};

PrerenderTabHelper::PrerenderTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
}

PrerenderTabHelper::~PrerenderTabHelper() {
}

void PrerenderTabHelper::ProvisionalChangeToMainFrameUrl(
    const GURL& url,
    content::RenderViewHost* render_view_host) {
  url_ = url;
  PrerenderManager* prerender_manager = MaybeGetPrerenderManager();
  if (!prerender_manager)
    return;
  if (prerender_manager->IsWebContentsPrerendering(web_contents(), NULL))
    return;
  prerender_manager->MarkWebContentsAsNotPrerendered(web_contents());
}

void PrerenderTabHelper::DidCommitProvisionalLoadForFrame(
    int64 frame_id,
    bool is_main_frame,
    const GURL& validated_url,
    content::PageTransition transition_type,
    content::RenderViewHost* render_view_host) {
  if (!is_main_frame)
    return;
  url_ = validated_url;
  PrerenderManager* prerender_manager = MaybeGetPrerenderManager();
  if (!prerender_manager)
    return;
  if (prerender_manager->IsWebContentsPrerendering(web_contents(), NULL))
    return;
  prerender_manager->RecordNavigation(validated_url);
}

void PrerenderTabHelper::DidStopLoading(
    content::RenderViewHost* render_view_host) {
  // Compute the PPLT metric and report it in a histogram, if needed.
  // We include pages that are still prerendering and have just finished
  // loading -- PrerenderManager will sort this out and handle it correctly
  // (putting those times into a separate histogram).
  if (!pplt_load_start_.is_null()) {
    double fraction_elapsed_at_swapin = -1.0;
    base::TimeTicks now = base::TimeTicks::Now();
    if (!actual_load_start_.is_null()) {
      double plt = (now - actual_load_start_).InMillisecondsF();
      if (plt > 0.0) {
        fraction_elapsed_at_swapin = 1.0 -
            (now - pplt_load_start_).InMillisecondsF() / plt;
      } else {
        fraction_elapsed_at_swapin = 1.0;
      }
      DCHECK_GE(fraction_elapsed_at_swapin, 0.0);
      DCHECK_LE(fraction_elapsed_at_swapin, 1.0);
    }
    PrerenderManager::RecordPerceivedPageLoadTime(
        now - pplt_load_start_, fraction_elapsed_at_swapin, web_contents(),
        url_);
    if (IsPrerendered() && pixel_stats_.get())
      pixel_stats_->GetBitmap(PixelStats::BITMAP_ON_LOAD, web_contents());
  }

  // Reset the PPLT metric.
  pplt_load_start_ = base::TimeTicks();
  actual_load_start_ = base::TimeTicks();
}

void PrerenderTabHelper::DidStartProvisionalLoadForFrame(
      int64 frame_id,
      int64 parent_frame_id,
      bool is_main_frame,
      const GURL& validated_url,
      bool is_error_page,
      bool is_iframe_srcdoc,
      content::RenderViewHost* render_view_host) {
  if (is_main_frame) {
    // Record the beginning of a new PPLT navigation.
    pplt_load_start_ = base::TimeTicks::Now();
    actual_load_start_ = base::TimeTicks();
  }
}

PrerenderManager* PrerenderTabHelper::MaybeGetPrerenderManager() const {
  return PrerenderManagerFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
}

bool PrerenderTabHelper::IsPrerendering() {
  PrerenderManager* prerender_manager = MaybeGetPrerenderManager();
  if (!prerender_manager)
    return false;
  return prerender_manager->IsWebContentsPrerendering(web_contents(), NULL);
}

bool PrerenderTabHelper::IsPrerendered() {
  PrerenderManager* prerender_manager = MaybeGetPrerenderManager();
  if (!prerender_manager)
    return false;
  return prerender_manager->IsWebContentsPrerendered(web_contents(), NULL);
}

void PrerenderTabHelper::PrerenderSwappedIn() {
  // Ensure we are not prerendering any more.
  DCHECK(!IsPrerendering());
  if (pplt_load_start_.is_null()) {
    // If we have already finished loading, report a 0 PPLT.
    PrerenderManager::RecordPerceivedPageLoadTime(base::TimeDelta(), 1.0,
                                                  web_contents(), url_);
    PrerenderManager* prerender_manager = MaybeGetPrerenderManager();
    if (prerender_manager)
      prerender_manager->RecordFractionPixelsFinalAtSwapin(web_contents(), 1.0);
  } else {
    // If we have not finished loading yet, record the actual load start, and
    // rebase the start time to now.
    actual_load_start_ = pplt_load_start_;
    pplt_load_start_ = base::TimeTicks::Now();
    if (pixel_stats_.get())
      pixel_stats_->GetBitmap(PixelStats::BITMAP_SWAP_IN, web_contents());
  }
}

}  // namespace prerender
