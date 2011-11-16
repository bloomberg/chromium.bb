// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_CONTENTS_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_CONTENTS_H_
#pragma once

#include <list>
#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/prerender/prerender_final_status.h"
#include "content/browser/tab_contents/tab_contents_observer.h"
#include "content/public/browser/notification_registrar.h"

class Profile;
class RenderViewHost;
class RenderViewHostDelegate;
class SessionStorageNamespace;
class TabContents;
class TabContentsWrapper;
struct FaviconURL;
struct ViewHostMsg_FrameNavigate_Params;

namespace base {
class ProcessMetrics;
}

namespace prerender {

class PrerenderManager;
class PrerenderRenderViewHostObserver;
class PrerenderTracker;

// This class is a peer of TabContents. It can host a renderer, but does not
// have any visible display. Its navigation is not managed by a
// NavigationController because is has no facility for navigating (other than
// programatically view window.location.href) or RenderViewHostManager because
// it is never allowed to navigate across a SiteInstance boundary.
class PrerenderContents : public content::NotificationObserver,
                          public TabContentsObserver {
 public:
  // PrerenderContents::Create uses the currently registered Factory to create
  // the PrerenderContents. Factory is intended for testing.
  class Factory {
   public:
    Factory() {}
    virtual ~Factory() {}

    // Ownership is not transfered through this interface as prerender_manager,
    // prerender_tracker, and profile are stored as weak pointers.
    virtual PrerenderContents* CreatePrerenderContents(
        PrerenderManager* prerender_manager,
        PrerenderTracker* prerender_tracker,
        Profile* profile,
        const GURL& url,
        const GURL& referrer,
        Origin origin,
        uint8 experiment_id) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Factory);
  };

  // Information on pages that the prerendered page has tried to prerender.
  struct PendingPrerenderData {
    PendingPrerenderData(Origin origin, const GURL& url, const GURL& referrer);

    Origin origin;
    GURL url;
    GURL referrer;
  };
  typedef std::list<PendingPrerenderData> PendingPrerenderList;

  virtual ~PrerenderContents();

  bool Init();

  static Factory* CreateFactory();

  // |source_render_view_host| is the RenderViewHost that initiated
  // prerendering.
  virtual void StartPrerendering(
      const RenderViewHost* source_render_view_host,
      SessionStorageNamespace* session_storage_namespace);

  // Verifies that the prerendering is not using too many resources, and kills
  // it if not.
  void DestroyWhenUsingTooManyResources();

  RenderViewHost* render_view_host_mutable();
  const RenderViewHost* render_view_host() const;

  ViewHostMsg_FrameNavigate_Params* navigate_params() {
    return navigate_params_.get();
  }
  string16 title() const { return title_; }
  int32 page_id() const { return page_id_; }
  GURL icon_url() const { return icon_url_; }
  const GURL& prerender_url() const { return prerender_url_; }
  const GURL& referrer() const { return referrer_; }
  bool has_stopped_loading() const { return has_stopped_loading_; }
  bool prerendering_has_started() const { return prerendering_has_started_; }

  // Sets the parameter to the value of the associated RenderViewHost's child id
  // and returns a boolean indicating the validity of that id.
  virtual bool GetChildId(int* child_id) const;

  // Sets the parameter to the value of the associated RenderViewHost's route id
  // and returns a boolean indicating the validity of that id.
  virtual bool GetRouteId(int* route_id) const;

  // Set the final status for how the PrerenderContents was used. This
  // should only be called once, and should be called before the prerender
  // contents are destroyed.
  void set_final_status(FinalStatus final_status);
  FinalStatus final_status() const { return final_status_; }

  Origin origin() const { return origin_; }
  uint8 experiment_id() const { return experiment_id_; }

  base::TimeTicks load_start_time() const { return load_start_time_; }

  // Indicates whether this prerendered page can be used for the provided
  // URL, i.e. whether there is a match. |matching_url| is optional and will be
  // set to the URL that is found as a match if it is provided.
  bool MatchesURL(const GURL& url, GURL* matching_url) const;

  void OnJSOutOfMemory();
  bool ShouldSuppressDialogs();

  // TabContentsObserver implementation.
  virtual void DidStopLoading() OVERRIDE;
  virtual void DidStartProvisionalLoadForFrame(
      int64 frame_id,
      bool is_main_frame,
      const GURL& validated_url,
      bool is_error_page,
      RenderViewHost* render_view_host) OVERRIDE;
  virtual void RenderViewGone(base::TerminationStatus status) OVERRIDE;

  // content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Adds an alias URL, for one of the many redirections. If the URL can not
  // be prerendered - for example, it's an ftp URL - |this| will be destroyed
  // and false is returned. Otherwise, true is returned and the alias is
  // remembered.
  virtual bool AddAliasURL(const GURL& url);

  // Adds all alias URLs from another prerender.
  void AddAliasURLsFromOtherPrerenderContents(PrerenderContents* other_pc);

  // The preview TabContents (may be null).
  TabContentsWrapper* prerender_contents() const {
    return prerender_contents_.get();
  }

  TabContentsWrapper* ReleasePrerenderContents();

  // Sets the final status, calls OnDestroy and adds |this| to the
  // PrerenderManager's pending deletes list.
  void Destroy(FinalStatus reason);

  // Applies all the URL history encountered during prerendering to the
  // new tab.
  void CommitHistory(TabContentsWrapper* tab);

  int32 starting_page_id() { return starting_page_id_; }

  base::Value* GetAsValue() const;

  // Returns whether a pending cross-site navigation is happening.
  // This could happen with renderer-issued navigations, such as a
  // MouseEvent being dispatched by a link to a website installed as an app.
  bool IsCrossSiteNavigationPending() const;

  // Adds a pending prerender to the list.
  virtual void AddPendingPrerender(Origin origin,
                                   const GURL& url,
                                   const GURL& referrer);

  // Returns true if |url| corresponds to a pending prerender.
  bool IsPendingEntry(const GURL& url) const;

  // Reissues any pending prerender requests from the prerendered page.  Also
  // clears the list of pending requests.
  void StartPendingPrerenders();

 protected:
  PrerenderContents(PrerenderManager* prerender_manager,
                    PrerenderTracker* prerender_tracker,
                    Profile* profile,
                    const GURL& url,
                    const GURL& referrer,
                    Origin origin,
                    uint8 experiment_id);

  // Called whenever a RenderViewHost is created for prerendering.  Only called
  // once the RenderViewHost has a RenderView and RenderWidgetHostView.
  virtual void OnRenderViewHostCreated(RenderViewHost* new_render_view_host);

  content::NotificationRegistrar& notification_registrar() {
    return notification_registrar_;
  }

  const PendingPrerenderList* pending_prerender_list() const {
    return &pending_prerender_list_;
  }

 private:
  class TabContentsDelegateImpl;

  // Needs to be able to call the constructor.
  friend class PrerenderContentsFactoryImpl;

  friend class PrerenderRenderViewHostObserver;

  // Message handlers.
  void OnUpdateFaviconURL(int32 page_id, const std::vector<FaviconURL>& urls);

  // Returns the RenderViewHost Delegate for this prerender.
  RenderViewHostDelegate* GetRenderViewHostDelegate();

  // Returns the ProcessMetrics for the render process, if it exists.
  base::ProcessMetrics* MaybeGetProcessMetrics();

  // The prerender manager owning this object.
  PrerenderManager* prerender_manager_;

  // The prerender tracker tracking prerenders.
  PrerenderTracker* prerender_tracker_;

  // The URL being prerendered.
  GURL prerender_url_;

  // The referrer.
  GURL referrer_;

  // The NavigationParameters of the finished navigation.
  scoped_ptr<ViewHostMsg_FrameNavigate_Params> navigate_params_;

  // The profile being used
  Profile* profile_;

  // Information about the title and URL of the page that this class as a
  // RenderViewHostDelegate has received from the RenderView.
  // Used to apply to the new RenderViewHost delegate that might eventually
  // own the contained RenderViewHost when the prerendered page is shown
  // in a TabContents.
  string16 title_;
  int32 page_id_;
  GURL url_;
  GURL icon_url_;
  content::NotificationRegistrar notification_registrar_;

  // A vector of URLs that this prerendered page matches against.
  // This array can contain more than element as a result of redirects,
  // such as HTTP redirects or javascript redirects.
  std::vector<GURL> alias_urls_;

  bool has_stopped_loading_;

  // This must be the same value as the PrerenderTracker has recorded for
  // |this|, when |this| has a RenderView.
  FinalStatus final_status_;

  bool prerendering_has_started_;

  // Tracks whether or not prerendering has been cancelled by calling Destroy.
  // Used solely to prevent double deletion.
  bool prerendering_has_been_cancelled_;

  // Time at which we started to load the URL.  This is used to compute
  // the time elapsed from initiating a prerender until the time the
  // (potentially only partially) prerendered page is shown to the user.
  base::TimeTicks load_start_time_;

  // Process Metrics of the render process associated with the
  // RenderViewHost for this object.
  scoped_ptr<base::ProcessMetrics> process_metrics_;

  // The prerendered TabContents; may be null.
  scoped_ptr<TabContentsWrapper> prerender_contents_;

  scoped_ptr<PrerenderRenderViewHostObserver> render_view_host_observer_;

  scoped_ptr<TabContentsDelegateImpl> tab_contents_delegate_;

  // These are -1 before a RenderView is created.
  int child_id_;
  int route_id_;

  // Page ID at which prerendering started.
  int32 starting_page_id_;

  // Origin for this prerender.
  Origin origin_;

  // Experiment during which this prerender is performed.
  uint8 experiment_id_;

  // List of all pages the prerendered page has tried to prerender.
  PendingPrerenderList pending_prerender_list_;

  // Offset by which to offset prerendered pages
  static const int32 kPrerenderPageIdOffset = 10;

  DISALLOW_COPY_AND_ASSIGN(PrerenderContents);
};

}  // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_CONTENTS_H_
