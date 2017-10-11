// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_LEAK_DETECTOR_LEAK_DETECTOR_REMOTE_CLIENT_H_
#define CHROME_RENDERER_LEAK_DETECTOR_LEAK_DETECTOR_REMOTE_CLIENT_H_

#include "base/macros.h"
#include "components/metrics/leak_detector/leak_detector.h"
#include "components/metrics/leak_detector/leak_detector.mojom.h"
#include "content/public/renderer/render_thread_observer.h"

class LeakDetectorRemoteClient : public content::RenderThreadObserver,
                                 public metrics::LeakDetector::Observer {
 public:
  LeakDetectorRemoteClient();
  ~LeakDetectorRemoteClient() override;

  // metrics::LeakDetector::Observer:
  void OnLeaksFound(
      const std::vector<metrics::MemoryLeakReportProto>& reports) override;

 private:
  // Callback for remote function LeakDetectorRemote::GetParams().
  void OnParamsReceived(
      mojo::StructPtr<metrics::mojom::LeakDetectorParams> result);

  // Handle to the remote Mojo interface.
  metrics::mojom::LeakDetectorPtr remote_service_;

  DISALLOW_COPY_AND_ASSIGN(LeakDetectorRemoteClient);
};

#endif  // CHROME_RENDERER_LEAK_DETECTOR_LEAK_DETECTOR_REMOTE_CLIENT_H_
