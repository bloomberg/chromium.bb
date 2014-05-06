// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GUEST_VIEW_GUEST_VIEW_MANAGER_H_
#define CHROME_BROWSER_GUEST_VIEW_GUEST_VIEW_MANAGER_H_

#include <map>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "content/public/browser/browser_plugin_guest_manager_delegate.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"

class GuestViewBase;
class GuestWebContentsObserver;
class GURL;

namespace content {
class BrowserContext;
}  // namespace content

class GuestViewManager : public content::BrowserPluginGuestManagerDelegate,
                         public base::SupportsUserData::Data {
 public:
  explicit GuestViewManager(content::BrowserContext* context);
  virtual ~GuestViewManager();

  static GuestViewManager* FromBrowserContext(content::BrowserContext* context);

  // BrowserPluginGuestManagerDelegate implementation.
  virtual int GetNextInstanceID() OVERRIDE;
  virtual void AddGuest(int guest_instance_id,
                        content::WebContents* guest_web_contents) OVERRIDE;
  virtual void RemoveGuest(int guest_instance_id) OVERRIDE;
  virtual content::WebContents* GetGuestByInstanceID(
      int guest_instance_id,
      int embedder_render_process_id) OVERRIDE;
  virtual bool CanEmbedderAccessInstanceIDMaybeKill(
      int embedder_render_process_id,
      int guest_instance_id) OVERRIDE;
  virtual bool CanEmbedderAccessInstanceID(int embedder_render_process_id,
                                           int guest_instance_id) OVERRIDE;
  virtual content::SiteInstance* GetGuestSiteInstance(
      const GURL& guest_site) OVERRIDE;
  virtual bool ForEachGuest(content::WebContents* embedder_web_contents,
                            const GuestCallback& callback) OVERRIDE;

 private:
  friend class GuestWebContentsObserver;

  void AddRenderProcessHostID(int render_process_host_id);

  static bool CanEmbedderAccessGuest(int embedder_render_process_id,
                                     GuestViewBase* guest);

  // Counts RenderProcessHost IDs of GuestViewBases.
  std::multiset<int> render_process_host_id_multiset_;

  // Contains guests' WebContents, mapping from their instance ids.
  typedef std::map<int, content::WebContents*> GuestInstanceMap;
  GuestInstanceMap guest_web_contents_by_instance_id_;

  int current_instance_id_;
  content::BrowserContext* context_;

  DISALLOW_COPY_AND_ASSIGN(GuestViewManager);
};

#endif  // CHROME_BROWSER_GUEST_VIEW_GUEST_VIEW_MANAGER_H_
