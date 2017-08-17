// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/pdf_iframe_navigation_throttle.h"

#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/pdf_uma.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "net/base/escape.h"
#include "net/http/http_response_headers.h"

PDFIFrameNavigationThrottle::PDFIFrameNavigationThrottle(
    content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle) {}

PDFIFrameNavigationThrottle::~PDFIFrameNavigationThrottle() {}

const char* PDFIFrameNavigationThrottle::GetNameForLogging() {
  return "PDFIFrameNavigationThrottle";
}

// static
std::unique_ptr<content::NavigationThrottle>
PDFIFrameNavigationThrottle::MaybeCreateThrottleFor(
    content::NavigationHandle* handle) {
  if (handle->IsInMainFrame())
    return nullptr;

  content::WebPluginInfo pdf_plugin_info;
  base::FilePath pdf_plugin_path =
      base::FilePath::FromUTF8Unsafe(ChromeContentClient::kPDFPluginPath);
  content::PluginService::GetInstance()->GetPluginInfoByPath(pdf_plugin_path,
                                                             &pdf_plugin_info);

  ChromePluginServiceFilter* filter = ChromePluginServiceFilter::GetInstance();
  int process_id = handle->GetWebContents()->GetRenderProcessHost()->GetID();
  int routing_id =
      handle->GetWebContents()
          ->FindFrameByFrameTreeNodeId(handle->GetFrameTreeNodeId(), process_id)
          ->GetRoutingID();
  content::ResourceContext* resource_context =
      handle->GetWebContents()->GetBrowserContext()->GetResourceContext();
  if (filter->IsPluginAvailable(process_id, routing_id, resource_context,
                                handle->GetURL(), url::Origin(),
                                &pdf_plugin_info)) {
    return nullptr;
  }

  return base::MakeUnique<PDFIFrameNavigationThrottle>(handle);
}

content::NavigationThrottle::ThrottleCheckResult
PDFIFrameNavigationThrottle::WillProcessResponse() {
  std::string mime_type;
  navigation_handle()->GetResponseHeaders()->GetMimeType(&mime_type);
  if (mime_type != kPDFMimeType)
    return content::NavigationThrottle::PROCEED;

  std::string html = base::StringPrintf(
      R"(<body style="margin: 0;"><object data="%s"  type="application/pdf" )"
      R"(style="width: 100%%; height: 100%%;"></object></body>)",
      navigation_handle()->GetURL().spec().c_str());
  GURL data_url("data:text/html," + net::EscapePath(html));

  navigation_handle()->GetWebContents()->OpenURL(
      content::OpenURLParams(data_url, navigation_handle()->GetReferrer(),
                             navigation_handle()->GetFrameTreeNodeId(),
                             WindowOpenDisposition::CURRENT_TAB,
                             ui::PAGE_TRANSITION_AUTO_SUBFRAME, false));

  return content::NavigationThrottle::CANCEL_AND_IGNORE;
}
