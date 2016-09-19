// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PAGE_LOAD_METRICS_RENDERER_PAGE_TRACK_DECIDER_H_
#define CHROME_RENDERER_PAGE_LOAD_METRICS_RENDERER_PAGE_TRACK_DECIDER_H_

#include "base/macros.h"
#include "chrome/common/page_load_metrics/page_track_decider.h"

namespace blink {
class WebDocument;
class WebDataSource;
}  // namespace blink

namespace page_load_metrics {

class RendererPageTrackDecider : public PageTrackDecider {
 public:
  // document and data_source are not owned by RendererPageTrackDecider,
  // and must outlive the RendererPageTrackDecider.
  RendererPageTrackDecider(const blink::WebDocument* document,
                           const blink::WebDataSource* data_source);
  ~RendererPageTrackDecider() override;

  bool HasCommitted() override;
  bool IsHttpOrHttpsUrl() override;
  bool IsNewTabPageUrl() override;
  bool IsChromeErrorPage() override;
  bool IsHtmlOrXhtmlPage() override;
  int GetHttpStatusCode() override;

 private:
  const blink::WebDocument* const document_;
  const blink::WebDataSource* const data_source_;

  DISALLOW_COPY_AND_ASSIGN(RendererPageTrackDecider);
};

}  // namespace page_load_metrics

#endif  // CHROME_RENDERER_PAGE_LOAD_METRICS_RENDERER_PAGE_TRACK_DECIDER_H_
