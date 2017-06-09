// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_STATS_COLLECTION_CONTROLLER_H_
#define CONTENT_RENDERER_STATS_COLLECTION_CONTROLLER_H_

#include "base/macros.h"
#include "gin/wrappable.h"

namespace blink {
class WebLocalFrame;
}

namespace content {

// This class is exposed in JS as window.statsCollectionController and provides
// functionality to read out statistics from the browser.
// Its use must be enabled specifically via the
// --enable-stats-collection-bindings command line flag.
class StatsCollectionController
    : public gin::Wrappable<StatsCollectionController> {
 public:
  static gin::WrapperInfo kWrapperInfo;

  static void Install(blink::WebLocalFrame* frame);

 private:
  StatsCollectionController();
  ~StatsCollectionController() override;

  // gin::WrappableBase
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  // Retrieves a histogram and returns a JSON representation of it.
  std::string GetHistogram(const std::string& histogram_name);

  // Retrieves a histogram from the browser process and returns a JSON
  // representation of it.
  std::string GetBrowserHistogram(const std::string& histogram_name);

  // Returns JSON representation of tab timing information for the current tab.
  std::string GetTabLoadTiming();

  DISALLOW_COPY_AND_ASSIGN(StatsCollectionController);
};

}  // namespace content

#endif  // CONTENT_RENDERER_STATS_COLLECTION_CONTROLLER_H_
