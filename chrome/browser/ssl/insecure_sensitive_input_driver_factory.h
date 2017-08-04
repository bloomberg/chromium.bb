// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_INSECURE_SENSITIVE_INPUT_DRIVER_FACTORY_H_
#define CHROME_BROWSER_SSL_INSECURE_SENSITIVE_INPUT_DRIVER_FACTORY_H_

#include <map>
#include <memory>

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "third_party/WebKit/public/platform/modules/insecure_input/insecure_input_service.mojom.h"

namespace content {
class WebContents;
}

class InsecureSensitiveInputDriver;

// Creates and owns InsecureSensitiveInputDrivers. There is one
// factory per WebContents, and one driver per render frame.
class InsecureSensitiveInputDriverFactory
    : public content::WebContentsObserver,
      public content::WebContentsUserData<InsecureSensitiveInputDriverFactory> {
 public:
  ~InsecureSensitiveInputDriverFactory() override;

  // Creates a factory for the |web_contents|, enumerates its current frames
  // and creates an |InsecureSensitiveInputDriver| for each.
  static void BindDriver(blink::mojom::InsecureInputServiceRequest request,
                         content::RenderFrameHost* render_frame_host);

  // Returns the |InsecureSensitiveInputDriver| previously created for the
  // specified |render_frame_host| by |BindDriver| or |RenderFrameCreated|.
  // Returns nullptr if the driver is missing.
  InsecureSensitiveInputDriver* GetDriverForFrame(
      content::RenderFrameHost* render_frame_host);

  // content::WebContentsObserver:
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;

 private:
  explicit InsecureSensitiveInputDriverFactory(
      content::WebContents* web_contents);
  friend class content::WebContentsUserData<
      InsecureSensitiveInputDriverFactory>;

  std::map<content::RenderFrameHost*,
           std::unique_ptr<InsecureSensitiveInputDriver>>
      frame_driver_map_;

  DISALLOW_COPY_AND_ASSIGN(InsecureSensitiveInputDriverFactory);
};

#endif  // CHROME_BROWSER_SSL_INSECURE_SENSITIVE_INPUT_DRIVER_FACTORY_H_
