// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SITE_INSTANCE_IMPL_H_
#define CONTENT_BROWSER_SITE_INSTANCE_IMPL_H_
#pragma once

#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/site_instance.h"
#include "googleurl/src/gurl.h"

namespace content {
class RenderProcessHostFactory;
}

class CONTENT_EXPORT SiteInstanceImpl : public content::SiteInstance,
                                        public content::NotificationObserver {
 public:
  // content::SiteInstance interface overrides.
  virtual int32 GetId() OVERRIDE;
  virtual bool HasProcess() const OVERRIDE;
  virtual  content::RenderProcessHost* GetProcess() OVERRIDE;
  virtual const GURL& GetSite() const OVERRIDE;
  virtual SiteInstance* GetRelatedSiteInstance(const GURL& url) OVERRIDE;
  virtual bool IsRelatedSiteInstance(const SiteInstance* instance) OVERRIDE;
  virtual content::BrowserContext* GetBrowserContext() const OVERRIDE;

  // Set the web site that this SiteInstance is rendering pages for.
  // This includes the scheme and registered domain, but not the port.  If the
  // URL does not have a valid registered domain, then the full hostname is
  // stored.
  void SetSite(const GURL& url);
  bool HasSite() const;

  // Returns whether there is currently a related SiteInstance (registered with
  // BrowsingInstance) for the site of the given url.  If so, we should try to
  // avoid dedicating an unused SiteInstance to it (e.g., in a new tab).
  bool HasRelatedSiteInstance(const GURL& url);

  // Returns whether this SiteInstance has a process that is the wrong type for
  // the given URL.  If so, the browser should force a process swap when
  // navigating to the URL.
  bool HasWrongProcessForURL(const GURL& url) const;

  // Sets the factory used to create new RenderProcessHosts. This will also be
  // passed on to SiteInstances spawned by this one.
  // The factory must outlive the SiteInstance; ownership is not transferred. It
  // may be NULL, in which case the default BrowserRenderProcessHost will be
  // created (this is the behavior if you don't call this function).
  void set_render_process_host_factory(
      content::RenderProcessHostFactory* rph_factory) {
    render_process_host_factory_ = rph_factory;
  }

  // Returns the site for the given URL, which includes only the scheme and
  // registered domain.  Returns an empty GURL if the URL has no host.
  static GURL GetSiteForURL(content::BrowserContext* context, const GURL& url);

 protected:
  friend class BrowsingInstance;
  friend class content::SiteInstance;

  // Virtual to allow tests to extend it.
  virtual ~SiteInstanceImpl();

  // Create a new SiteInstance.  Protected to give access to BrowsingInstance
  // and tests; most callers should use Create or GetRelatedSiteInstance
  // instead.
  explicit SiteInstanceImpl(BrowsingInstance* browsing_instance);

 private:
  // Get the effective URL for the given actual URL.
  static GURL GetEffectiveURL(content::BrowserContext* browser_context,
                              const GURL& url);

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Used to restrict a process' origin access rights.
  void LockToOrigin();

  // The next available SiteInstance ID.
  static int32 next_site_instance_id_;

  // A unique ID for this SiteInstance.
  int32 id_;

  content::NotificationRegistrar registrar_;

  // BrowsingInstance to which this SiteInstance belongs.
  scoped_refptr<BrowsingInstance> browsing_instance_;

  // Factory for new RenderProcessHosts, not owned by this class. NULL indiactes
  // that the default BrowserRenderProcessHost should be created.
  const content::RenderProcessHostFactory* render_process_host_factory_;

  // Current RenderProcessHost that is rendering pages for this SiteInstance.
  // This pointer will only change once the RenderProcessHost is destructed.  It
  // will still remain the same even if the process crashes, since in that
  // scenario the RenderProcessHost remains the same.
  content::RenderProcessHost* process_;

  // The web site that this SiteInstance is rendering pages for.
  GURL site_;

  // Whether SetSite has been called.
  bool has_site_;

  DISALLOW_COPY_AND_ASSIGN(SiteInstanceImpl);
};

#endif  // CONTENT_BROWSER_SITE_INSTANCE_IMPL_H_
