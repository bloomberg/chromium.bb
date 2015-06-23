// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_MEDIA_ROUTER_MEDIA_ROUTER_INTEGRATION_BROWSERTEST_H_
#define CHROME_TEST_MEDIA_ROUTER_MEDIA_ROUTER_INTEGRATION_BROWSERTEST_H_

#include <string>

#include "base/debug/stack_trace.h"
#include "base/files/file_path.h"
#include "chrome/test/media_router/media_router_base_browsertest.h"


namespace media_router {

class MediaRouter;

class MediaRouterIntegrationBrowserTest : public MediaRouterBaseBrowserTest {
 public:
  MediaRouterIntegrationBrowserTest();
  ~MediaRouterIntegrationBrowserTest() override;

 protected:
  // Simulate user action to choose one sink in the popup dialog.
  // |web_contents|: The web contents of the test page which invokes the popup
  //                 dialog.
  // |sink_id|: The sink id.
  void ChooseSink(content::WebContents* web_contents,
                  const std::string& sink_id);

  // Execute javascript and check the return value.
  void ExecuteJavaScriptAPI(content::WebContents* web_contents,
                            const std::string& script);

  void OpenTestPage(base::FilePath::StringPieceType file);
};

}  // namespace media_router

#endif  // CHROME_TEST_MEDIA_ROUTER_MEDIA_ROUTER_INTEGRATION_BROWSERTEST_H_
