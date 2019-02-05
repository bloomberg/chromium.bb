// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/pdf_iframe_navigation_throttle.h"

#include <string>

#include "base/feature_list.h"
#include "base/memory/weak_ptr.h"
#include "base/task/post_task.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pdf_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_utils.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "net/base/escape.h"
#include "net/http/http_response_headers.h"
#include "ppapi/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#include "content/public/browser/plugin_service.h"
#endif

namespace {

// Used to scope the posted navigation task to the lifetime of |web_contents|.
class WebContentsLifetimeHelper
    : public content::WebContentsUserData<WebContentsLifetimeHelper> {
 public:
  explicit WebContentsLifetimeHelper(content::WebContents* web_contents)
      : web_contents_(web_contents) {}

  base::WeakPtr<WebContentsLifetimeHelper> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  void NavigateIFrameToPlaceholder(const content::OpenURLParams& url_params) {
    web_contents_->OpenURL(url_params);
  }

 private:
  friend class content::WebContentsUserData<WebContentsLifetimeHelper>;

  content::WebContents* const web_contents_;
  base::WeakPtrFactory<WebContentsLifetimeHelper> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(WebContentsLifetimeHelper)

}  // namespace

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

#if BUILDFLAG(ENABLE_PLUGINS)
  content::WebPluginInfo pdf_plugin_info;
  static const base::FilePath pdf_plugin_path(
      ChromeContentClient::kPDFPluginPath);
  content::PluginService::GetInstance()->GetPluginInfoByPath(pdf_plugin_path,
                                                             &pdf_plugin_info);

  ChromePluginServiceFilter* filter = ChromePluginServiceFilter::GetInstance();
  int process_id =
      handle->GetWebContents()->GetMainFrame()->GetProcess()->GetID();
  int routing_id = handle->GetWebContents()->GetMainFrame()->GetRoutingID();
  content::ResourceContext* resource_context =
      handle->GetWebContents()->GetBrowserContext()->GetResourceContext();
  if (filter->IsPluginAvailable(process_id, routing_id, resource_context,
                                handle->GetURL(), url::Origin(),
                                &pdf_plugin_info)) {
    return nullptr;
  }
#endif

  // If ENABLE_PLUGINS is false, the PDF plugin is not available, so we should
  // always intercept PDF iframe navigations.
  return std::make_unique<PDFIFrameNavigationThrottle>(handle);
}

content::NavigationThrottle::ThrottleCheckResult
PDFIFrameNavigationThrottle::WillProcessResponse() {
  const net::HttpResponseHeaders* response_headers =
      navigation_handle()->GetResponseHeaders();
  if (!response_headers)
    return content::NavigationThrottle::PROCEED;

  std::string mime_type;
  response_headers->GetMimeType(&mime_type);
  if (mime_type != kPDFMimeType)
    return content::NavigationThrottle::PROCEED;

  // We MUST download responses marked as attachments rather than showing
  // a placeholder.
  if (content::download_utils::MustDownload(navigation_handle()->GetURL(),
                                            response_headers, mime_type)) {
    return content::NavigationThrottle::PROCEED;
  }

  ReportPDFLoadStatus(PDFLoadStatus::kLoadedIframePdfWithNoPdfViewer);

  if (!base::FeatureList::IsEnabled(features::kClickToOpenPDFPlaceholder))
    return content::NavigationThrottle::PROCEED;

  // Prepare the params to navigate to the placeholder.
  std::string html = GetPDFPlaceholderHTML(navigation_handle()->GetURL());
  GURL data_url("data:text/html," + net::EscapePath(html));
  content::OpenURLParams params(data_url, navigation_handle()->GetReferrer(),
                                navigation_handle()->GetFrameTreeNodeId(),
                                WindowOpenDisposition::CURRENT_TAB,
                                ui::PAGE_TRANSITION_AUTO_SUBFRAME,
                                navigation_handle()->IsRendererInitiated());
  params.initiator_origin = navigation_handle()->GetInitiatorOrigin();

  // Post a task to navigate to the placeholder HTML. We don't navigate
  // synchronously here, as starting a navigation within a navigation is
  // an antipattern. Use a helper object scoped to the WebContents lifetime to
  // scope the navigation task to the WebContents lifetime.
  content::WebContents* web_contents = navigation_handle()->GetWebContents();
  WebContentsLifetimeHelper::CreateForWebContents(web_contents);
  WebContentsLifetimeHelper* helper =
      WebContentsLifetimeHelper::FromWebContents(web_contents);
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&WebContentsLifetimeHelper::NavigateIFrameToPlaceholder,
                     helper->GetWeakPtr(), params));

  return content::NavigationThrottle::CANCEL_AND_IGNORE;
}
