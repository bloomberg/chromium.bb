// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/web_request/chrome_extension_web_request_event_router_delegate.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/activity_log/activity_action_constants.h"
#include "chrome/browser/extensions/activity_log/activity_log.h"
#include "chrome/browser/extensions/extension_renderer_state.h"
#include "content/public/browser/resource_request_info.h"
#include "extensions/browser/api/activity_log/web_request_constants.h"
#include "extensions/browser/api/web_request/web_request_api.h"
#include "extensions/browser/api/web_request/web_request_api_constants.h"
#include "extensions/browser/api/web_request/web_request_api_helpers.h"
#include "net/url_request/url_request.h"

namespace activitylog = activity_log_web_request_constants;
namespace helpers = extension_web_request_api_helpers;
namespace keys = extension_web_request_api_constants;

namespace {

void ExtractExtraRequestDetailsInternal(
    net::URLRequest* request, int* tab_id, int* window_id) {
  if (!request->GetUserData(NULL))
    return;

  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request);
  ExtensionRendererState::GetInstance()->GetTabAndWindowId(
      info, tab_id, window_id);
}

}  // namespace

ChromeExtensionWebRequestEventRouterDelegate::
    ChromeExtensionWebRequestEventRouterDelegate(){
}

ChromeExtensionWebRequestEventRouterDelegate::
    ~ChromeExtensionWebRequestEventRouterDelegate() {
}

void ChromeExtensionWebRequestEventRouterDelegate::LogExtensionActivity(
    content::BrowserContext* browser_context,
    bool is_incognito,
    const std::string& extension_id,
    const GURL& url,
    const std::string& api_call,
    scoped_ptr<base::DictionaryValue> details) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (!extensions::ExtensionsBrowserClient::Get()->IsValidContext(
      browser_context))
    return;

  scoped_refptr<extensions::Action> action =
      new extensions::Action(extension_id,
                              base::Time::Now(),
                              extensions::Action::ACTION_WEB_REQUEST,
                              api_call);
  action->set_page_url(url);
  action->set_page_incognito(is_incognito);
  action->mutable_other()->Set(activity_log_constants::kActionWebRequest,
                               details.release());
  extensions::ActivityLog::GetInstance(browser_context)->LogAction(action);
}

void ChromeExtensionWebRequestEventRouterDelegate::ExtractExtraRequestDetails(
   net::URLRequest* request, base::DictionaryValue* out) {
  int tab_id = -1;
  int window_id = -1;
  ExtractExtraRequestDetailsInternal(request, &tab_id, &window_id);
  out->SetInteger(keys::kTabIdKey, tab_id);
}

bool
ChromeExtensionWebRequestEventRouterDelegate::OnGetMatchingListenersImplCheck(
    int filter_tab_id, int filter_window_id, net::URLRequest* request) {
  int tab_id = -1;
  int window_id = -1;
  ExtractExtraRequestDetailsInternal(request, &tab_id, &window_id);
  if (filter_tab_id != -1 && tab_id != filter_tab_id)
    return true;
  if (filter_window_id != -1 && window_id != filter_window_id)
    return true;
  return false;
}
