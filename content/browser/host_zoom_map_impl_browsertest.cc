// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/host_zoom_map_impl.h"

#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"
#include "url/gurl.h"

namespace content {

class HostZoomMapImplBrowserTest : public ContentBrowserTest {
};

void RunTestForURL(const GURL& url,
                   Shell* shell,
                   double host_zoom_level,
                   double temp_zoom_level) {
  shell->LoadURL(url);
  WebContents* web_contents = shell->web_contents();

  HostZoomMapImpl* host_zoom_map = static_cast<HostZoomMapImpl*>(
      HostZoomMap::GetForWebContents(web_contents));

  int view_id = web_contents->GetRoutingID();
  int render_process_id = web_contents->GetRenderProcessHost()->GetID();

  // Assume caller has set the zoom level to |host_zoom_level| using
  // either a host or host+scheme entry in the HostZoomMap prior to
  // calling this function.
  EXPECT_DOUBLE_EQ(host_zoom_level, host_zoom_map->GetZoomLevelForView(
                                        url, render_process_id, view_id));

  // Make sure that GetZoomLevelForView() works for temporary zoom levels.
  host_zoom_map->SetTemporaryZoomLevel(render_process_id, view_id,
                                       temp_zoom_level);
  EXPECT_DOUBLE_EQ(temp_zoom_level, host_zoom_map->GetZoomLevelForView(
                                        url, render_process_id, view_id));
  // Clear the temporary zoom level in case subsequent test calls use the same
  // web_contents.
  host_zoom_map->ClearTemporaryZoomLevel(render_process_id, view_id);
}

// Test to make sure that GetZoomForView() works properly for zoom levels
// stored by host value, and can distinguish temporary zoom levels from
// these.
IN_PROC_BROWSER_TEST_F(HostZoomMapImplBrowserTest, GetZoomForView_Host) {
  GURL url("http://abc.com");

  HostZoomMap* host_zoom_map =
      HostZoomMap::GetForWebContents(shell()->web_contents());

  double default_zoom_level = host_zoom_map->GetDefaultZoomLevel();
  double host_zoom_level = default_zoom_level + 1.0;
  double temp_zoom_level = default_zoom_level + 2.0;

  host_zoom_map->SetZoomLevelForHost(url.host(), host_zoom_level);

  RunTestForURL(url, shell(), host_zoom_level, temp_zoom_level);
}

// Test to make sure that GetZoomForView() works properly for zoom levels
// stored by host and scheme values, and can distinguish temporary zoom levels
// from these.
IN_PROC_BROWSER_TEST_F(HostZoomMapImplBrowserTest,
                       GetZoomForView_HostAndScheme) {
  GURL url("http://abc.com");

  HostZoomMap* host_zoom_map =
      HostZoomMap::GetForWebContents(shell()->web_contents());

  double default_zoom_level = host_zoom_map->GetDefaultZoomLevel();
  double host_zoom_level = default_zoom_level + 1.0;
  double temp_zoom_level = default_zoom_level + 2.0;

  host_zoom_map->SetZoomLevelForHostAndScheme(url.scheme(), url.host(),
                                              host_zoom_level);

  RunTestForURL(url, shell(), host_zoom_level, temp_zoom_level);
}

}  // namespace content
