// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_CONTENT_BROWSER_CLIENT_H_
#define CHROME_BROWSER_CHROME_CONTENT_BROWSER_CLIENT_H_
#pragma once

#include "content/browser/content_browser_client.h"

namespace chrome {

class ChromeContentBrowserClient : public content::ContentBrowserClient {
 public:
  virtual void RenderViewHostCreated(RenderViewHost* render_view_host);
  virtual void BrowserRenderProcessHostCreated(BrowserRenderProcessHost* host);
  virtual void PluginProcessHostCreated(PluginProcessHost* host);
  virtual void WorkerProcessHostCreated(WorkerProcessHost* host);
  virtual content::WebUIFactory* GetWebUIFactory();
  virtual GURL GetEffectiveURL(Profile* profile, const GURL& url);
  virtual bool IsURLSameAsAnySiteInstance(const GURL& url);
  virtual GURL GetAlternateErrorPageURL(const TabContents* tab);
  virtual std::string GetCanonicalEncodingNameByAliasName(
      const std::string& alias_name);
  virtual void AppendExtraCommandLineSwitches(CommandLine* command_line,
                                              int child_process_id);
  virtual std::string GetApplicationLocale();
  virtual bool AllowAppCache(const GURL& manifest_url,
                             const content::ResourceContext& context);
  virtual bool AllowGetCookie(const GURL& url,
                              const GURL& first_party,
                              const net::CookieList& cookie_list,
                              const content::ResourceContext& context,
                              int render_process_id,
                              int render_view_id);
  virtual bool AllowSetCookie(const GURL& url,
                              const GURL& first_party,
                              const std::string& cookie_line,
                              const content::ResourceContext& context,
                              int render_process_id,
                              int render_view_id,
                              net::CookieOptions* options);
#if defined(OS_LINUX)
  // Can return an optional fd for crash handling, otherwise returns -1.
  virtual int GetCrashSignalFD(const std::string& process_type);
#endif
};

}  // namespace chrome

#endif  // CHROME_BROWSER_CHROME_CONTENT_BROWSER_CLIENT_H_
