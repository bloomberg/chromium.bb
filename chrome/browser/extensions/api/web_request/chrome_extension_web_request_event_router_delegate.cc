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
#include "chrome/browser/extensions/activity_log/web_request_constants.h"
#include "chrome/browser/extensions/api/web_request/web_request_api.h"
#include "chrome/browser/extensions/extension_renderer_state.h"
#include "content/public/browser/resource_request_info.h"
#include "extensions/browser/api/web_request/web_request_api_constants.h"
#include "extensions/browser/api/web_request/web_request_api_helpers.h"
#include "net/url_request/url_request.h"

namespace activitylog = activity_log_web_request_constants;
namespace helpers = extension_web_request_api_helpers;
namespace keys = extension_web_request_api_constants;

namespace {

// Convert a RequestCookieModifications/ResponseCookieModifications object to a
// base::ListValue which summarizes the changes made.  This is templated since
// the two types (request/response) are different but contain essentially the
// same fields.
template<typename CookieType>
base::ListValue* SummarizeCookieModifications(
    const std::vector<linked_ptr<CookieType> >& modifications) {
  scoped_ptr<base::ListValue> cookie_modifications(new base::ListValue());
  for (typename std::vector<linked_ptr<CookieType> >::const_iterator i =
           modifications.begin();
       i != modifications.end(); ++i) {
    scoped_ptr<base::DictionaryValue> summary(new base::DictionaryValue());
    const CookieType& mod = *i->get();
    switch (mod.type) {
      case helpers::ADD:
        summary->SetString(activitylog::kCookieModificationTypeKey,
                           activitylog::kCookieModificationAdd);
        break;
      case helpers::EDIT:
        summary->SetString(activitylog::kCookieModificationTypeKey,
                           activitylog::kCookieModificationEdit);
        break;
      case helpers::REMOVE:
        summary->SetString(activitylog::kCookieModificationTypeKey,
                           activitylog::kCookieModificationRemove);
        break;
    }
    if (mod.filter) {
      if (mod.filter->name)
        summary->SetString(activitylog::kCookieFilterNameKey,
                           *mod.modification->name);
      if (mod.filter->domain)
        summary->SetString(activitylog::kCookieFilterDomainKey,
                           *mod.modification->name);
    }
    if (mod.modification) {
      if (mod.modification->name)
        summary->SetString(activitylog::kCookieModDomainKey,
                           *mod.modification->name);
      if (mod.modification->domain)
        summary->SetString(activitylog::kCookieModDomainKey,
                           *mod.modification->name);
    }
    cookie_modifications->Append(summary.release());
  }
  return cookie_modifications.release();
}

base::Value* SerializeResponseHeaders(const helpers::ResponseHeaders& headers) {
  scoped_ptr<base::ListValue> serialized_headers(new base::ListValue());
  for (helpers::ResponseHeaders::const_iterator i = headers.begin();
       i != headers.end(); ++i) {
    serialized_headers->Append(
        helpers::CreateHeaderDictionary(i->first, i->second));
  }
  return serialized_headers.release();
}

// Converts an EventResponseDelta object to a dictionary value suitable for the
// activity log.
scoped_ptr<base::DictionaryValue> SummarizeResponseDelta(
    const std::string& event_name,
    const helpers::EventResponseDelta& delta) {
  scoped_ptr<base::DictionaryValue> details(new base::DictionaryValue());
  if (delta.cancel) {
    details->SetBoolean(activitylog::kCancelKey, true);
  }
  if (!delta.new_url.is_empty()) {
      details->SetString(activitylog::kNewUrlKey, delta.new_url.spec());
  }

  scoped_ptr<base::ListValue> modified_headers(new base::ListValue());
  net::HttpRequestHeaders::Iterator iter(delta.modified_request_headers);
  while (iter.GetNext()) {
    modified_headers->Append(
        helpers::CreateHeaderDictionary(iter.name(), iter.value()));
  }
  if (!modified_headers->empty()) {
    details->Set(activitylog::kModifiedRequestHeadersKey,
                 modified_headers.release());
  }

  scoped_ptr<base::ListValue> deleted_headers(new base::ListValue());
  deleted_headers->AppendStrings(delta.deleted_request_headers);
  if (!deleted_headers->empty()) {
    details->Set(activitylog::kDeletedRequestHeadersKey,
                 deleted_headers.release());
  }

  if (!delta.added_response_headers.empty()) {
    details->Set(activitylog::kAddedRequestHeadersKey,
                 SerializeResponseHeaders(delta.added_response_headers));
  }
  if (!delta.deleted_response_headers.empty()) {
    details->Set(activitylog::kDeletedResponseHeadersKey,
                 SerializeResponseHeaders(delta.deleted_response_headers));
  }
  if (delta.auth_credentials) {
    details->SetString(activitylog::kAuthCredentialsKey,
                       base::UTF16ToUTF8(
                           delta.auth_credentials->username()) + ":*");
  }

  if (!delta.response_cookie_modifications.empty()) {
    details->Set(
        activitylog::kResponseCookieModificationsKey,
        SummarizeCookieModifications(delta.response_cookie_modifications));
  }

  return details.Pass();
}

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
    void* browser_context_id,
    bool is_incognito,
    const std::string& extension_id,
    const GURL& url,
    const std::string& api_call,
    const helpers::EventResponseDelta& delta) {
  scoped_ptr<base::DictionaryValue> details = SummarizeResponseDelta(
      api_call, delta);
  LogExtensionActivityInternal(
      browser_context_id, is_incognito, extension_id,
      url, api_call, details.Pass());
}

void ChromeExtensionWebRequestEventRouterDelegate::LogExtensionActivityInternal(
    void* browser_context_id,
    bool is_incognito,
    const std::string& extension_id,
    const GURL& url,
    const std::string& api_call,
    scoped_ptr<base::DictionaryValue> details) {
  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    content::BrowserThread::PostTask(
        content::BrowserThread::UI,
        FROM_HERE,
        base::Bind(&ChromeExtensionWebRequestEventRouterDelegate::
                       LogExtensionActivityInternal,
                   base::Unretained(this),
                   browser_context_id,
                   is_incognito,
                   extension_id,
                   url,
                   api_call,
                   base::Passed(&details)));
  } else {
    content::BrowserContext* browser_context =
        static_cast<content::BrowserContext*>(browser_context_id);
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
