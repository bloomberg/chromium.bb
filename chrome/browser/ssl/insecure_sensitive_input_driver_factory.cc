// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/insecure_sensitive_input_driver_factory.h"

#include <utility>
#include <vector>

#include "base/stl_util.h"
#include "chrome/browser/ssl/insecure_sensitive_input_driver.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(InsecureSensitiveInputDriverFactory);

InsecureSensitiveInputDriverFactory::~InsecureSensitiveInputDriverFactory() {}

// static
void InsecureSensitiveInputDriverFactory::BindDriver(
    blink::mojom::SensitiveInputVisibilityServiceRequest request,
    content::RenderFrameHost* render_frame_host) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  if (!web_contents)
    return;

  InsecureSensitiveInputDriverFactory* factory = FromWebContents(web_contents);

  // If a factory does not yet exist for |web_contents|, create one.
  if (!factory) {
    content::WebContentsUserData<InsecureSensitiveInputDriverFactory>::
        CreateForWebContents(web_contents);
    factory = FromWebContents(web_contents);
    DCHECK(factory);
  }

  InsecureSensitiveInputDriver* driver =
      factory->GetDriverForFrame(render_frame_host);
  if (driver)
    driver->BindSensitiveInputVisibilityServiceRequest(std::move(request));
}

InsecureSensitiveInputDriver*
InsecureSensitiveInputDriverFactory::GetDriverForFrame(
    content::RenderFrameHost* render_frame_host) {
  auto mapping = frame_driver_map_.find(render_frame_host);
  return mapping == frame_driver_map_.end() ? nullptr : mapping->second.get();
}

void InsecureSensitiveInputDriverFactory::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  auto insertion_result =
      frame_driver_map_.insert(std::make_pair(render_frame_host, nullptr));
  // If the map didn't already contain |render_frame_host|, construct a
  // new InsecureSensitiveInputDriver.
  if (insertion_result.second) {
    insertion_result.first->second =
        base::MakeUnique<InsecureSensitiveInputDriver>(render_frame_host);
  }
}

void InsecureSensitiveInputDriverFactory::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  frame_driver_map_.erase(render_frame_host);
}

InsecureSensitiveInputDriverFactory::InsecureSensitiveInputDriverFactory(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  // Create a |InsecureSensitiveInputDriver| for each frame in |web_contents|.
  const std::vector<content::RenderFrameHost*> frames =
      web_contents->GetAllFrames();
  for (content::RenderFrameHost* frame : frames) {
    if (frame->IsRenderFrameLive())
      RenderFrameCreated(frame);
  }
}
