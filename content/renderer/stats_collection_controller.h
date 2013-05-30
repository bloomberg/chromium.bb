// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_STATS_COLLECTION_CONTROLLER_H_
#define CONTENT_RENDERER_STATS_COLLECTION_CONTROLLER_H_

#include "ipc/ipc_sender.h"
#include "webkit/renderer/cpp_bound_class.h"

namespace content {

// This class is exposed in JS as window.statsCollectionController and provides
// functionality to read out statistics from the browser.
// Its use must be enabled specifically via the
// --enable-stats-collection-bindings command line flag.
class StatsCollectionController : public webkit_glue::CppBoundClass {
 public:
  StatsCollectionController();

  void set_message_sender(IPC::Sender* sender) {
    sender_ = sender;
  }

  // Retrieves a histogram and returns a JSON representation of it.
  void GetHistogram(const webkit_glue::CppArgumentList& args,
                    webkit_glue::CppVariant* result);

  // Retrieves a histogram from the browser process and returns a JSON
  // representation of it.
  void GetBrowserHistogram(const webkit_glue::CppArgumentList& args,
                           webkit_glue::CppVariant* result);

  // Returns JSON representation of tab timing information for the current tab.
  void GetTabLoadTiming(const webkit_glue::CppArgumentList& args,
                        webkit_glue::CppVariant* result);
 private:
  IPC::Sender* sender_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_STATS_COLLECTION_CONTROLLER_H_
