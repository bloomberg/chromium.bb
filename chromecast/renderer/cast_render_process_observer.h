// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_RENDERER_CAST_RENDER_PROCESS_OBSERVER_H_
#define CHROMECAST_RENDERER_CAST_RENDER_PROCESS_OBSERVER_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "content/public/renderer/render_process_observer.h"

namespace chromecast {
class CapabilitiesMessageFilter;
namespace media {
class CmaMessageFilterProxy;
}

namespace shell {

class CastRenderProcessObserver : public content::RenderProcessObserver {
 public:
  CastRenderProcessObserver();
  ~CastRenderProcessObserver() override;

 private:
  // content::RenderProcessObserver implementation:
  void OnRenderProcessShutdown() override;

  void CreateCustomFilters();

#if !defined(OS_ANDROID)
  scoped_refptr<media::CmaMessageFilterProxy> cma_message_filter_proxy_;
#endif  // !defined(OS_ANDROID)
  scoped_refptr<CapabilitiesMessageFilter> capabilities_message_filter_;

  DISALLOW_COPY_AND_ASSIGN(CastRenderProcessObserver);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_RENDERER_CAST_RENDER_PROCESS_OBSERVER_H_
