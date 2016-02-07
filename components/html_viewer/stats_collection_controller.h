// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_STATS_COLLECTION_CONTROLLER_H_
#define COMPONENTS_HTML_VIEWER_STATS_COLLECTION_CONTROLLER_H_

#include "base/macros.h"
#include "gin/wrappable.h"
#include "mojo/services/tracing/public/interfaces/tracing.mojom.h"

namespace blink {
class WebFrame;
}

namespace mojo {
class Shell;
}

namespace html_viewer {

// This class is exposed in JS as window.statsCollectionController and provides
// functionality to read out statistics from the browser.
// Its use must be enabled specifically via the
// --enable-stats-collection-bindings command line flag.
class StatsCollectionController
    : public gin::Wrappable<StatsCollectionController> {
 public:
  static gin::WrapperInfo kWrapperInfo;

  // Install the JS and return a mojo:tracing InterfacePtr for stats reporting.
  // This bails and returns a null pointer without the stats command line flag.
  static tracing::StartupPerformanceDataCollectorPtr Install(
      blink::WebFrame* frame,
      mojo::Shell* shell);

  // Return a mojo:metrics InterfacePtr for stats reporting.
  // This bails and returns a null pointer without the stats command line flag.
  static tracing::StartupPerformanceDataCollectorPtr ConnectToDataCollector(
      mojo::Shell* shell);

 private:
  explicit StatsCollectionController(
      tracing::StartupPerformanceDataCollectorPtr collector);
  ~StatsCollectionController() override;

  // gin::WrappableBase
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  // Retrieves a histogram and returns a JSON representation of it.
  std::string GetHistogram(const std::string& histogram_name);

  // Retrieves a browser histogram and returns a JSON representation of it.
  std::string GetBrowserHistogram(const std::string& histogram_name);

  tracing::StartupPerformanceDataCollectorPtr
      startup_performance_data_collector_;

  DISALLOW_COPY_AND_ASSIGN(StatsCollectionController);
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_STATS_COLLECTION_CONTROLLER_H_
