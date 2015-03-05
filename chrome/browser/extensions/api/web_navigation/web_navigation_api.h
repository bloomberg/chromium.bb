// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the Chrome Extensions WebNavigation API functions for observing and
// intercepting navigation events, as specified in the extension JSON API.

#ifndef CHROME_BROWSER_EXTENSIONS_API_WEB_NAVIGATION_WEB_NAVIGATION_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_WEB_NAVIGATION_WEB_NAVIGATION_API_H_

#include <map>
#include <set>

#include "base/compiler_specific.h"
#include "chrome/browser/extensions/api/web_navigation/frame_navigation_state.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "url/gurl.h"

struct RetargetingDetails;

namespace extensions {

// Tab contents observer that forwards navigation events to the event router.
class WebNavigationTabObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<WebNavigationTabObserver> {
 public:
  ~WebNavigationTabObserver() override;

  // Returns the object for the given |web_contents|.
  static WebNavigationTabObserver* Get(content::WebContents* web_contents);

  const FrameNavigationState& frame_navigation_state() const {
    return navigation_state_;
  }

  // content::WebContentsObserver implementation.
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void AboutToNavigateRenderFrame(
      content::RenderFrameHost* old_host,
      content::RenderFrameHost* new_host) override;
  void DidStartProvisionalLoadForFrame(
      content::RenderFrameHost* render_frame_host,
      const GURL& validated_url,
      bool is_error_page,
      bool is_iframe_srcdoc) override;
  void DidCommitProvisionalLoadForFrame(
      content::RenderFrameHost* render_frame_host,
      const GURL& url,
      ui::PageTransition transition_type) override;
  void DidFailProvisionalLoad(content::RenderFrameHost* render_frame_host,
                              const GURL& validated_url,
                              int error_code,
                              const base::string16& error_description) override;
  void DocumentLoadedInFrame(
      content::RenderFrameHost* render_frame_host) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void DidFailLoad(content::RenderFrameHost* render_frame_host,
                   const GURL& validated_url,
                   int error_code,
                   const base::string16& error_description) override;
  void DidGetRedirectForResourceRequest(
      content::RenderFrameHost* render_frame_host,
      const content::ResourceRedirectDetails& details) override;
  void DidOpenRequestedURL(content::WebContents* new_contents,
                           content::RenderFrameHost* source_render_frame_host,
                           const GURL& url,
                           const content::Referrer& referrer,
                           WindowOpenDisposition disposition,
                           ui::PageTransition transition) override;
  void WebContentsDestroyed() override;

 private:
  explicit WebNavigationTabObserver(content::WebContents* web_contents);
  friend class content::WebContentsUserData<WebNavigationTabObserver>;

  // True if the transition and target url correspond to a reference fragment
  // navigation.
  bool IsReferenceFragmentNavigation(content::RenderFrameHost* frame_host,
                                     const GURL& url);

  // Creates and sends onErrorOccurred events for all on-going navigations. If
  // |render_view_host| is non-NULL, only generates events for frames in this
  // render view host. If |frame_host_to_skip| is given, no events are sent for
  // that
  // frame.
  void SendErrorEvents(content::WebContents* web_contents,
                       content::RenderViewHost* render_view_host,
                       content::RenderFrameHost* frame_host_to_skip);

  // Tracks the state of the frames we are sending events for.
  FrameNavigationState navigation_state_;

  // Used for tracking registrations to redirect notifications.
  content::NotificationRegistrar registrar_;

  // The current RenderViewHost of the observed WebContents.
  content::RenderViewHost* render_view_host_;

  // During a cross site navigation, the WebContents has a second, pending
  // RenderViewHost.
  content::RenderViewHost* pending_render_view_host_;

  DISALLOW_COPY_AND_ASSIGN(WebNavigationTabObserver);
};

// Observes navigation notifications and routes them as events to the extension
// system.
class WebNavigationEventRouter : public TabStripModelObserver,
                                 public chrome::BrowserListObserver,
                                 public content::NotificationObserver {
 public:
  explicit WebNavigationEventRouter(Profile* profile);
  ~WebNavigationEventRouter() override;

 private:
  // Used to cache the information about newly created WebContents objects.
  struct PendingWebContents{
    PendingWebContents();
    PendingWebContents(content::WebContents* source_web_contents,
                       content::RenderFrameHost* source_frame_host,
                       content::WebContents* target_web_contents,
                       const GURL& target_url);
    ~PendingWebContents();

    content::WebContents* source_web_contents;
    content::RenderFrameHost* source_frame_host;
    content::WebContents* target_web_contents;
    GURL target_url;
  };

  // TabStripModelObserver implementation.
  void TabReplacedAt(TabStripModel* tab_strip_model,
                     content::WebContents* old_contents,
                     content::WebContents* new_contents,
                     int index) override;

  // chrome::BrowserListObserver implementation.
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

  // content::NotificationObserver implementation.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Handler for the NOTIFICATION_RETARGETING event. The method takes the
  // details of such an event and stores them for the later
  // NOTIFICATION_TAB_ADDED event.
  void Retargeting(const RetargetingDetails* details);

  // Handler for the NOTIFICATION_TAB_ADDED event. The method takes the details
  // of such an event and creates a JSON formated extension event from it.
  void TabAdded(content::WebContents* tab);

  // Handler for NOTIFICATION_WEB_CONTENTS_DESTROYED. If |tab| is in
  // |pending_web_contents_|, it is removed.
  void TabDestroyed(content::WebContents* tab);

  // Mapping pointers to WebContents objects to information about how they got
  // created.
  std::map<content::WebContents*, PendingWebContents> pending_web_contents_;

  // Used for tracking registrations to navigation notifications.
  content::NotificationRegistrar registrar_;

  // The profile that owns us via ExtensionService.
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(WebNavigationEventRouter);
};

// API function that returns the state of a given frame.
class WebNavigationGetFrameFunction : public ChromeSyncExtensionFunction {
  ~WebNavigationGetFrameFunction() override {}
  bool RunSync() override;
  DECLARE_EXTENSION_FUNCTION("webNavigation.getFrame", WEBNAVIGATION_GETFRAME)
};

// API function that returns the states of all frames in a given tab.
class WebNavigationGetAllFramesFunction : public ChromeSyncExtensionFunction {
  ~WebNavigationGetAllFramesFunction() override {}
  bool RunSync() override;
  DECLARE_EXTENSION_FUNCTION("webNavigation.getAllFrames",
                             WEBNAVIGATION_GETALLFRAMES)
};

class WebNavigationAPI : public BrowserContextKeyedAPI,
                         public extensions::EventRouter::Observer {
 public:
  explicit WebNavigationAPI(content::BrowserContext* context);
  ~WebNavigationAPI() override;

  // KeyedService implementation.
  void Shutdown() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<WebNavigationAPI>* GetFactoryInstance();

  // EventRouter::Observer implementation.
  void OnListenerAdded(const extensions::EventListenerInfo& details) override;

 private:
  friend class BrowserContextKeyedAPIFactory<WebNavigationAPI>;

  content::BrowserContext* browser_context_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() {
    return "WebNavigationAPI";
  }
  static const bool kServiceIsNULLWhileTesting = true;

  // Created lazily upon OnListenerAdded.
  scoped_ptr<WebNavigationEventRouter> web_navigation_event_router_;

  DISALLOW_COPY_AND_ASSIGN(WebNavigationAPI);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_WEB_NAVIGATION_WEB_NAVIGATION_API_H_
