// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PAGE_LOAD_HISTOGRAMS_H_
#define CHROME_RENDERER_PAGE_LOAD_HISTOGRAMS_H_

#include "base/basictypes.h"
#include "content/public/renderer/render_view_observer.h"

namespace content {
class DocumentState;
}

class PageLoadHistograms : public content::RenderViewObserver {
 public:
  explicit PageLoadHistograms(content::RenderView* render_view);

 private:
  // RenderViewObserver implementation.
  virtual void FrameWillClose(blink::WebFrame* frame) OVERRIDE;
  virtual void ClosePage() OVERRIDE;

  // Dump all page load histograms appropriate for the given frame.
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
  void Dump(blink::WebFrame* frame);

  void LogPageLoadTime(const content::DocumentState* load_times,
                       const blink::WebDataSource* ds) const;

  DISALLOW_COPY_AND_ASSIGN(PageLoadHistograms);
};

#endif  // CHROME_RENDERER_PAGE_LOAD_HISTOGRAMS_H_
