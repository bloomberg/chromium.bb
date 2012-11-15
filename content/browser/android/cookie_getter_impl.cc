// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/cookie_getter_impl.h"

#include "base/bind.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "googleurl/src/gurl.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_store.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

namespace content {

static void ReturnCookieOnUIThread(
    const media::CookieGetter::GetCookieCB& callback,
    const std::string& cookies) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE, base::Bind(callback, cookies));
}

// The task object that retrieves cookie on the IO thread.
// TODO(qinmin): refactor this class to make the code reusable by others as
// there are lots of duplicated functionalities elsewhere.
class CookieGetterTask
     : public base::RefCountedThreadSafe<CookieGetterTask> {
 public:
  CookieGetterTask(BrowserContext* browser_context,
                   int renderer_id, int routing_id);

  // Called by CookieGetterImpl to start getting cookies for a URL.
  void RequestCookies(
      const GURL& url, const GURL& first_party_for_cookies,
      const media::CookieGetter::GetCookieCB& callback);

 private:
  friend class base::RefCountedThreadSafe<CookieGetterTask>;
  virtual ~CookieGetterTask();

  void CheckPolicyForCookies(
      const GURL& url, const GURL& first_party_for_cookies,
      const media::CookieGetter::GetCookieCB& callback,
      const net::CookieList& cookie_list);

  // Context getter used to get the CookieStore.
  net::URLRequestContextGetter* context_getter_;

  // Resource context for checking cookie policies.
  ResourceContext* resource_context_;

  // Render process id, used to check whether the process can access cookies.
  int renderer_id_;

  // Routing id for the render view, used to check tab specific cookie policy.
  int routing_id_;

  DISALLOW_COPY_AND_ASSIGN(CookieGetterTask);
};

CookieGetterTask::CookieGetterTask(
    BrowserContext* browser_context, int renderer_id, int routing_id)
    : context_getter_(browser_context->GetRequestContext()),
      resource_context_(browser_context->GetResourceContext()),
      renderer_id_(renderer_id),
      routing_id_(routing_id) {
}

CookieGetterTask::~CookieGetterTask() {}

void CookieGetterTask::RequestCookies(
    const GURL& url, const GURL& first_party_for_cookies,
    const media::CookieGetter::GetCookieCB& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  if (!policy->CanAccessCookiesForOrigin(renderer_id_, url)) {
    callback.Run(std::string());
    return;
  }

  net::CookieStore* cookie_store =
      context_getter_->GetURLRequestContext()->cookie_store();
  if (!cookie_store) {
    callback.Run(std::string());
    return;
  }

  net::CookieMonster* cookie_monster = cookie_store->GetCookieMonster();
  if (cookie_monster) {
    cookie_monster->GetAllCookiesForURLAsync(url, base::Bind(
        &CookieGetterTask::CheckPolicyForCookies, this,
        url, first_party_for_cookies, callback));
  } else {
    callback.Run(std::string());
  }
}

void CookieGetterTask::CheckPolicyForCookies(
    const GURL& url, const GURL& first_party_for_cookies,
    const media::CookieGetter::GetCookieCB& callback,
    const net::CookieList& cookie_list) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (GetContentClient()->browser()->AllowGetCookie(
      url, first_party_for_cookies, cookie_list,
      resource_context_, renderer_id_, routing_id_)) {
    net::CookieStore* cookie_store =
        context_getter_->GetURLRequestContext()->cookie_store();
    cookie_store->GetCookiesWithOptionsAsync(
        url, net::CookieOptions(), callback);
  } else {
    callback.Run(std::string());
  }
}

CookieGetterImpl::CookieGetterImpl(
    BrowserContext* browser_context, int renderer_id, int routing_id)
    : browser_context_(browser_context),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_this_(this)),
      renderer_id_(renderer_id),
      routing_id_(routing_id) {
}

CookieGetterImpl::~CookieGetterImpl() {}

void CookieGetterImpl::GetCookies(const std::string& url,
                                  const std::string& first_party_for_cookies,
                                  const GetCookieCB& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  scoped_refptr<CookieGetterTask> task = new CookieGetterTask(
      browser_context_, renderer_id_, routing_id_);

  GetCookieCB cb = base::Bind(
      &CookieGetterImpl::GetCookiesCallback, weak_this_.GetWeakPtr(), callback);
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&CookieGetterTask::RequestCookies,
                 task, GURL(url), GURL(first_party_for_cookies),
                 base::Bind(&ReturnCookieOnUIThread, cb)));
}

void CookieGetterImpl::GetCookiesCallback(
    const GetCookieCB& callback, const std::string& cookies) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  callback.Run(cookies);
}

}  // namespace content
