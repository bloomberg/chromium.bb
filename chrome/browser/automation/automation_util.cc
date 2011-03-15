// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/automation_util.h"

#include <string>

#include "base/values.h"
#include "chrome/browser/automation/automation_provider_json.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/browser_render_process_host.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "chrome/browser/ui/browser.h"
#include "content/browser/browser_thread.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "net/base/cookie_store.h"
#include "net/url_request/url_request_context.h"

namespace {

void GetCookiesOnIOThread(
    const GURL& url,
    const scoped_refptr<URLRequestContextGetter>& context_getter,
    base::WaitableEvent* event,
    std::string* cookies) {
  *cookies = context_getter->GetCookieStore()->GetCookies(url);
  event->Signal();
}

void SetCookieOnIOThread(
    const GURL& url,
    const std::string& value,
    const scoped_refptr<URLRequestContextGetter>& context_getter,
    base::WaitableEvent* event,
    bool* success) {
  *success = context_getter->GetCookieStore()->SetCookie(url, value);
  event->Signal();
}

void DeleteCookieOnIOThread(
    const GURL& url,
    const std::string& name,
    const scoped_refptr<URLRequestContextGetter>& context_getter,
    base::WaitableEvent* event) {
  context_getter->GetCookieStore()->DeleteCookie(url, name);
  event->Signal();
}

}  // namespace

namespace automation_util {

void GetCookies(const GURL& url,
                TabContents* contents,
                int* value_size,
                std::string* value) {
  *value_size = -1;
  if (url.is_valid() && contents) {
    // Since we may be on the UI thread don't call GetURLRequestContext().
    // Get the request context specific to the current TabContents and app.
    const Extension* installed_app = static_cast<BrowserRenderProcessHost*>(
        contents->render_view_host()->process())->installed_app();
    scoped_refptr<URLRequestContextGetter> context_getter =
        contents->profile()->GetRequestContextForPossibleApp(installed_app);

    base::WaitableEvent event(true /* manual reset */,
                              false /* not initially signaled */);
    CHECK(BrowserThread::PostTask(
              BrowserThread::IO, FROM_HERE,
              NewRunnableFunction(&GetCookiesOnIOThread,
                                  url, context_getter, &event, value)));
    event.Wait();

    *value_size = static_cast<int>(value->size());
  }
}

void SetCookie(const GURL& url,
               const std::string value,
               TabContents* contents,
               int* response_value) {
  *response_value = -1;

  if (url.is_valid() && contents) {
    // Since we may be on the UI thread don't call GetURLRequestContext().
    // Get the request context specific to the current TabContents and app.
    const Extension* installed_app = static_cast<BrowserRenderProcessHost*>(
        contents->render_view_host()->process())->installed_app();
    scoped_refptr<URLRequestContextGetter> context_getter =
        contents->profile()->GetRequestContextForPossibleApp(installed_app);

    base::WaitableEvent event(true /* manual reset */,
                              false /* not initially signaled */);
    bool success = false;
    CHECK(BrowserThread::PostTask(
              BrowserThread::IO, FROM_HERE,
              NewRunnableFunction(&SetCookieOnIOThread,
                                  url, value, context_getter, &event,
                                  &success)));
    event.Wait();
    if (success)
      *response_value = 1;
  }
}

void DeleteCookie(const GURL& url,
                  const std::string& cookie_name,
                  TabContents* contents,
                  bool* success) {
  *success = false;
  if (url.is_valid() && contents) {
    // Since we may be on the UI thread don't call GetURLRequestContext().
    // Get the request context specific to the current TabContents and app.
    const Extension* installed_app = static_cast<BrowserRenderProcessHost*>(
        contents->render_view_host()->process())->installed_app();
    scoped_refptr<URLRequestContextGetter> context_getter =
        contents->profile()->GetRequestContextForPossibleApp(installed_app);

    base::WaitableEvent event(true /* manual reset */,
                              false /* not initially signaled */);
    CHECK(BrowserThread::PostTask(
              BrowserThread::IO, FROM_HERE,
              NewRunnableFunction(&DeleteCookieOnIOThread,
                                  url, cookie_name, context_getter, &event)));
    event.Wait();
    *success = true;
  }
}

void GetCookiesJSON(AutomationProvider* provider,
                    DictionaryValue* args,
                    IPC::Message* reply_message) {
  AutomationJSONReply reply(provider, reply_message);
  Browser* browser;
  std::string error;
  if (!GetBrowserFromJSONArgs(args, &browser, &error)) {
    reply.SendError(error);
    return;
  }
  std::string url;
  if (!args->GetString("url", &url)) {
    reply.SendError("'url' missing or invalid");
    return;
  }

  // Since we may be on the UI thread don't call GetURLRequestContext().
  scoped_refptr<URLRequestContextGetter> context_getter =
      browser->profile()->GetRequestContext();

  std::string cookies;
  base::WaitableEvent event(true /* manual reset */,
                            false /* not initially signaled */);
  Task* task = NewRunnableFunction(
      &GetCookiesOnIOThread,
      GURL(url), context_getter, &event, &cookies);
  if (!BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, task)) {
    reply.SendError("Couldn't post task to get the cookies");
    return;
  }
  event.Wait();

  DictionaryValue dict;
  dict.SetString("cookies", cookies);
  reply.SendSuccess(&dict);
}

void DeleteCookieJSON(AutomationProvider* provider,
                      DictionaryValue* args,
                      IPC::Message* reply_message) {
  AutomationJSONReply reply(provider, reply_message);
  Browser* browser;
  std::string error;
  if (!GetBrowserFromJSONArgs(args, &browser, &error)) {
    reply.SendError(error);
    return;
  }
  std::string url, name;
  if (!args->GetString("url", &url)) {
    reply.SendError("'url' missing or invalid");
    return;
  }
  if (!args->GetString("name", &name)) {
    reply.SendError("'name' missing or invalid");
    return;
  }

  // Since we may be on the UI thread don't call GetURLRequestContext().
  scoped_refptr<URLRequestContextGetter> context_getter =
      browser->profile()->GetRequestContext();

  base::WaitableEvent event(true /* manual reset */,
                            false /* not initially signaled */);
  Task* task = NewRunnableFunction(
      &DeleteCookieOnIOThread,
      GURL(url), name, context_getter, &event);
  if (!BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, task)) {
    reply.SendError("Couldn't post task to delete the cookie");
    return;
  }
  event.Wait();
  reply.SendSuccess(NULL);
}

void SetCookieJSON(AutomationProvider* provider,
                   DictionaryValue* args,
                   IPC::Message* reply_message) {
  AutomationJSONReply reply(provider, reply_message);
  Browser* browser;
  std::string error;
  if (!GetBrowserFromJSONArgs(args, &browser, &error)) {
    reply.SendError(error);
    return;
  }
  std::string url, cookie;
  if (!args->GetString("url", &url)) {
    reply.SendError("'url' missing or invalid");
    return;
  }
  if (!args->GetString("cookie", &cookie)) {
    reply.SendError("'cookie' missing or invalid");
    return;
  }

  // Since we may be on the UI thread don't call GetURLRequestContext().
  scoped_refptr<URLRequestContextGetter> context_getter =
      browser->profile()->GetRequestContext();

  base::WaitableEvent event(true /* manual reset */,
                            false /* not initially signaled */);
  bool success = false;
  Task* task = NewRunnableFunction(
      &SetCookieOnIOThread,
      GURL(url), cookie, context_getter, &event, &success);
  if (!BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, task)) {
    reply.SendError("Couldn't post task to set the cookie");
    return;
  }
  event.Wait();

  if (!success) {
    reply.SendError("Could not set the cookie");
    return;
  }
  reply.SendSuccess(NULL);
}

}  // namespace automation_util
