// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONTENT_BROWSER_CLIENT_H_
#define CONTENT_BROWSER_CONTENT_BROWSER_CLIENT_H_
#pragma once

#include <string>

#include "base/callback_old.h"
#include "content/common/content_client.h"
#include "content/common/window_container_type.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNotificationPresenter.h"

class BrowserRenderProcessHost;
class CommandLine;
class FilePath;
class GURL;
class PluginProcessHost;
class Profile;
class QuotaPermissionContext;
class RenderViewHost;
class SSLCertErrorHandler;
class SSLClientAuthHandler;
class TabContents;
class WorkerProcessHost;
struct DesktopNotificationHostMsg_Show_Params;

namespace net {
class CookieList;
class CookieOptions;
class URLRequest;
class X509Certificate;
}

namespace content {

class ResourceContext;
class WebUIFactory;

// Embedder API for participating in browser logic.  The methods are assumed to
// be called on the UI thread unless otherwise specified.
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

  // Returns whether a specified URL is to be considered the same as any
  // SiteInstance.
  virtual bool IsURLSameAsAnySiteInstance(const GURL& url);

  // See CharacterEncoding's comment.
  virtual std::string GetCanonicalEncodingNameByAliasName(
      const std::string& alias_name);

  // Allows the embedder to pass extra command line flags.
  // switches::kProcessType will already be set at this point.
  virtual void AppendExtraCommandLineSwitches(CommandLine* command_line,
                                              int child_process_id);

  // Returns the locale used by the application.
  virtual std::string GetApplicationLocale();

  // Returns the languages used in the Accept-Languages HTTP header.
  // (Not called GetAcceptLanguages so it doesn't clash with win32).
  virtual std::string GetAcceptLangs(const TabContents* tab);

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

  // Create and return a new quota permission context.
  virtual QuotaPermissionContext* CreateQuotaPermissionContext();

  // Shows the given path using the OS file manager.
  virtual void RevealFolderInOS(const FilePath& path);

  // Informs the embedder that a certificate error has occured.  If overridable
  // is true, the user can ignore the error and continue.  If it's false, then
  // the certificate error is severe and the user isn't allowed to proceed.  The
  // embedder can call the callback asynchronously.
  virtual void AllowCertificateError(
      SSLCertErrorHandler* handler,
      bool overridable,
      Callback2<SSLCertErrorHandler*, bool>::Type* callback);

  // Shows the user a SSL client certificate selection dialog. When the user has
  // made a selection, the dialog will report back to |delegate|. |delegate| is
  // notified when the dialog closes in call cases; if the user cancels the
  // dialog, we call  with a NULL certificate.
  virtual void ShowClientCertificateRequestDialog(
      int render_process_id,
      int render_view_id,
      SSLClientAuthHandler* handler);

  // Adds a newly-generated client cert. The embedder should ensure that there's
  // a private key for the cert, displays the cert to the user, and adds it upon
  // user approval.
  virtual void AddNewCertificate(
      net::URLRequest* request,
      net::X509Certificate* cert,
      int render_process_id,
      int render_view_id);

  // Asks permission to show desktop notifications.
  virtual void RequestDesktopNotificationPermission(
      const GURL& source_origin,
      int callback_context,
      int render_process_id,
      int render_view_id);

  // Checks if the given page has permission to show desktop notifications.
  // This is called on the IO thread.
  virtual WebKit::WebNotificationPresenter::Permission
      CheckDesktopNotificationPermission(
          const GURL& source_url,
          const content::ResourceContext& context);

  // Show a desktop notification.  If |worker| is true, the request came from an
  // HTML5 web worker, otherwise, it came from a renderer.
  virtual void ShowDesktopNotification(
      const DesktopNotificationHostMsg_Show_Params& params,
      int render_process_id,
      int render_view_id,
      bool worker);

  // Cancels a displayed desktop notification.
  virtual void CancelDesktopNotification(
      int render_process_id,
      int render_view_id,
      int notification_id);

  // Returns true if the given page is allowed to open a window of the given
  // type. This is called on the IO thread.
  virtual bool CanCreateWindow(
      const GURL& source_url,
      WindowContainerType container_type,
      const content::ResourceContext& context);

  // Returns a title string to use in the task manager for a process host with
  // the given URL, or the empty string to fall back to the default logic.
  // This is called on the IO thread.
  virtual std::string GetWorkerProcessTitle(
      const GURL& url, const content::ResourceContext& context);

#if defined(OS_POSIX) && !defined(OS_MACOSX)
  // Can return an optional fd for crash handling, otherwise returns -1.
  virtual int GetCrashSignalFD(const std::string& process_type);
#endif
};

}  // namespace content

#endif  // CONTENT_BROWSER_CONTENT_BROWSER_CLIENT_H_
