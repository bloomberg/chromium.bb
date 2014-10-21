// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SITE_INSTANCE_IMPL_H_
#define CONTENT_BROWSER_SITE_INSTANCE_IMPL_H_

#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/site_instance.h"
#include "url/gurl.h"

namespace content {
class BrowsingInstance;
class RenderProcessHostFactory;

class CONTENT_EXPORT SiteInstanceImpl : public SiteInstance,
                                        public RenderProcessHostObserver {
 public:
  // SiteInstance interface overrides.
  int32 GetId() override;
  bool HasProcess() const override;
  RenderProcessHost* GetProcess() override;
  BrowserContext* GetBrowserContext() const override;
  const GURL& GetSiteURL() const override;
  SiteInstance* GetRelatedSiteInstance(const GURL& url) override;
  bool IsRelatedSiteInstance(const SiteInstance* instance) override;
  size_t GetRelatedActiveContentsCount() override;

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
  bool HasWrongProcessForURL(const GURL& url);

  // Increase the number of active frames in this SiteInstance. This is
  // increased when a frame is created, or a currently swapped out frame
  // is swapped in.
  void increment_active_frame_count() { active_frame_count_++; }

  // Decrease the number of active frames in this SiteInstance. This is
  // decreased when a frame is destroyed, or a currently active frame is
  // swapped out.
  void decrement_active_frame_count() { active_frame_count_--; }

  // Get the number of active frames which belong to this SiteInstance.  If
  // there are no active frames left, all frames in this SiteInstance can be
  // safely discarded.
  size_t active_frame_count() { return active_frame_count_; }

  // Increase the number of active WebContentses using this SiteInstance. Note
  // that, unlike active_frame_count, this does not count pending RFHs.
  void IncrementRelatedActiveContentsCount();

  // Decrease the number of active WebContentses using this SiteInstance. Note
  // that, unlike active_frame_count, this does not count pending RFHs.
  void DecrementRelatedActiveContentsCount();

  // Sets the global factory used to create new RenderProcessHosts.  It may be
  // NULL, in which case the default BrowserRenderProcessHost will be created
  // (this is the behavior if you don't call this function).  The factory must
  // be set back to NULL before it's destroyed; ownership is not transferred.
  static void set_render_process_host_factory(
      const RenderProcessHostFactory* rph_factory);

  // Get the effective URL for the given actual URL.  This allows the
  // ContentBrowserClient to override the SiteInstance's site for certain URLs.
  // For example, Chrome uses this to replace hosted app URLs with extension
  // hosts.
  // Only public so that we can make a consistent process swap decision in
  // RenderFrameHostManager.
  static GURL GetEffectiveURL(BrowserContext* browser_context,
                              const GURL& url);

 protected:
  friend class BrowsingInstance;
  friend class SiteInstance;

  // Virtual to allow tests to extend it.
  ~SiteInstanceImpl() override;

  // Create a new SiteInstance.  Protected to give access to BrowsingInstance
  // and tests; most callers should use Create or GetRelatedSiteInstance
  // instead.
  explicit SiteInstanceImpl(BrowsingInstance* browsing_instance);

 private:
  // RenderProcessHostObserver implementation.
  void RenderProcessHostDestroyed(RenderProcessHost* host) override;

  // Used to restrict a process' origin access rights.
  void LockToOrigin();

  // An object used to construct RenderProcessHosts.
  static const RenderProcessHostFactory* g_render_process_host_factory_;

  // The next available SiteInstance ID.
  static int32 next_site_instance_id_;

  // A unique ID for this SiteInstance.
  int32 id_;

  // The number of active frames in this SiteInstance.
  size_t active_frame_count_;

  // BrowsingInstance to which this SiteInstance belongs.
  scoped_refptr<BrowsingInstance> browsing_instance_;

  // Current RenderProcessHost that is rendering pages for this SiteInstance.
  // This pointer will only change once the RenderProcessHost is destructed.  It
  // will still remain the same even if the process crashes, since in that
  // scenario the RenderProcessHost remains the same.
  RenderProcessHost* process_;

  // The web site that this SiteInstance is rendering pages for.
  GURL site_;

  // Whether SetSite has been called.
  bool has_site_;

  DISALLOW_COPY_AND_ASSIGN(SiteInstanceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SITE_INSTANCE_IMPL_H_
