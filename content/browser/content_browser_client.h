// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONTENT_BROWSER_CLIENT_H_
#define CONTENT_BROWSER_CONTENT_BROWSER_CLIENT_H_
#pragma once

#include <string>

#include "content/common/content_client.h"

class BrowserRenderProcessHost;
class CommandLine;
class GURL;
class PluginProcessHost;
class Profile;
class RenderViewHost;
class TabContents;
class WorkerProcessHost;

namespace net {
class CookieList;
class CookieOptions;
}

namespace content {

class ResourceContext;
class WebUIFactory;

// Embedder API for participating in browser logic.
class ContentBrowserClient {
 public:
  // Notifies that a new RenderHostView has been created.
  virtual void RenderViewHostCreated(RenderViewHost* render_view_host);

  // Notifies that a BrowserRenderProcessHost has been created. This is called
  // before the content layer adds its own BrowserMessageFilters, so that the
  // embedder's IPC filters have priority.
  virtual void BrowserRenderProcessHostCreated(BrowserRenderProcessHost* host);

  // Notifies that a PluginProcessHost has been created. This is called
  // before the content layer adds its own message filters, so that the
  // embedder's IPC filters have priority.
  virtual void PluginProcessHostCreated(PluginProcessHost* host);

  // Notifies that a WorkerProcessHost has been created. This is called
  // before the content layer adds its own message filters, so that the
  // embedder's IPC filters have priority.
  virtual void WorkerProcessHostCreated(WorkerProcessHost* host);

  // Gets the WebUIFactory which will be responsible for generating WebUIs.
  virtual WebUIFactory* GetWebUIFactory();

  // Get the effective URL for the given actual URL, to allow an embedder to
  // group different url schemes in the same SiteInstance.
  virtual GURL GetEffectiveURL(Profile* profile, const GURL& url);

  // See RenderViewHostDelegate's comment.
  virtual GURL GetAlternateErrorPageURL(const TabContents* tab);

  // See CharacterEncoding's comment.
  virtual std::string GetCanonicalEncodingNameByAliasName(
      const std::string& alias_name);

  // Allows the embedder to pass extra command line flags.
  // switches::kProcessType will already be set at this point.
  virtual void AppendExtraCommandLineSwitches(CommandLine* command_line,
                                              int child_process_id);

  // Returns the locale used by the application.
  virtual std::string GetApplicationLocale();

  // Allow the embedder to control if an AppCache can be used for the given url.
  // This is called on the IO thread.
  virtual bool AllowAppCache(const GURL& manifest_url,
                             const content::ResourceContext& context);

  // Allow the embedder to control if the given cookie can be read.
  // This is called on the IO thread.
  virtual bool AllowGetCookie(const GURL& url,
                              const GURL& first_party,
                              const net::CookieList& cookie_list,
                              const content::ResourceContext& context,
                              int render_process_id,
                              int render_view_id);

  // Allow the embedder to control if the given cookie can be set.
  // This is called on the IO thread.
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

}  // namespace content

#endif  // CONTENT_BROWSER_CONTENT_BROWSER_CLIENT_H_
