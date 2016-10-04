// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(bmcquade): delete this class in October 2016, as it is deprecated by the
// new PageLoad.* UMA histograms.

#ifndef CHROME_RENDERER_PAGE_LOAD_HISTOGRAMS_H_
#define CHROME_RENDERER_PAGE_LOAD_HISTOGRAMS_H_

#include "base/macros.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_view_observer.h"

namespace blink {
class WebDataSource;
}

namespace content {
class DocumentState;
}

class PageLoadHistograms : public content::RenderFrameObserver {
 public:
  explicit PageLoadHistograms(content::RenderFrame* render_frame);
  ~PageLoadHistograms() override;

 private:
  // RenderFrameObserver implementation.
  void WillCommitProvisionalLoad() override;
  void OnDestruct() override;

  // Dump all page load histograms appropriate for the associated frame.
  //
  // This method will only dump once-per-instance, so it is safe to call
  // multiple times.
  //
  // The time points we keep are
  //    request: time document was requested by user
  //    start: time load of document started
  //    commit: time load of document started
  //    finish_document: main document loaded, before onload()
  //    finish_all_loads: after onload() and all resources are loaded
  //    first_paint: first paint performed
  //    first_paint_after_load: first paint performed after load is finished
  //    begin: request if it was user requested, start otherwise
  //
  // It's possible for the request time not to be set, if a client
  // redirect had been done (the user never requested the page)
  // Also, it's possible to load a page without ever laying it out
  // so first_paint and first_paint_after_load can be 0.
  void Dump();

  void LogPageLoadTime(const content::DocumentState* load_times,
                       const blink::WebDataSource* ds) const;

  // PageLoadHistograms needs the ClosePage() notification, but it's problematic
  // to inherit from both RVO and RFO. Embed the RVO in a small helper class
  // instead.
  class Helper : public content::RenderViewObserver {
   public:
    explicit Helper(PageLoadHistograms* histograms);

    // RenderViewObserver implementation.
    void ClosePage() override;
    void OnDestruct() override;

   private:
    // The Helper is owned by PageLoadHistograms.
    PageLoadHistograms* const histograms_;

    DISALLOW_COPY_AND_ASSIGN(Helper);
  };

  Helper helper_;

  DISALLOW_COPY_AND_ASSIGN(PageLoadHistograms);
};

#endif  // CHROME_RENDERER_PAGE_LOAD_HISTOGRAMS_H_
