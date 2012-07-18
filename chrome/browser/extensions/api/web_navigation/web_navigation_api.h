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
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"
#include "googleurl/src/gurl.h"

struct RetargetingDetails;

namespace extensions {

// Tab contents observer that forwards navigation events to the event router.
class WebNavigationTabObserver : public content::NotificationObserver,
                                 public content::WebContentsObserver {
 public:
  explicit WebNavigationTabObserver(content::WebContents* web_contents);
  virtual ~WebNavigationTabObserver();

  // Returns the object for the given |tab_contents|.
  static WebNavigationTabObserver* Get(content::WebContents* web_contents);

  const FrameNavigationState& frame_navigation_state() const {
    return navigation_state_;
  }

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;


  // content::WebContentsObserver implementation.
  virtual void DidStartProvisionalLoadForFrame(
      int64 frame_id,
      bool is_main_frame,
      const GURL& validated_url,
      bool is_error_page,
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void DidCommitProvisionalLoadForFrame(
      int64 frame_id,
      bool is_main_frame,
      const GURL& url,
      content::PageTransition transition_type,
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void DidFailProvisionalLoad(
      int64 frame_id,
      bool is_main_frame,
      const GURL& validated_url,
      int error_code,
      const string16& error_description,
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void DocumentLoadedInFrame(int64 frame_id) OVERRIDE;
  virtual void DidFinishLoad(int64 frame_id,
                             const GURL& validated_url,
                             bool is_main_frame) OVERRIDE;
  virtual void DidFailLoad(int64 frame_id,
                           const GURL& validated_url,
                           bool is_main_frame,
                           int error_code,
                           const string16& error_description) OVERRIDE;
  virtual void DidOpenRequestedURL(content::WebContents* new_contents,
                                   const GURL& url,
                                   const content::Referrer& referrer,
                                   WindowOpenDisposition disposition,
                                   content::PageTransition transition,
                                   int64 source_frame_id) OVERRIDE;
  virtual void WebContentsDestroyed(content::WebContents* tab) OVERRIDE;

 private:
  // True if the transition and target url correspond to a reference fragment
  // navigation.
  bool IsReferenceFragmentNavigation(int64 frame_id, const GURL& url);

  // Tracks the state of the frames we are sending events for.
  FrameNavigationState navigation_state_;

  // Used for tracking registrations to redirect notifications.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(WebNavigationTabObserver);
};

// Observes navigation notifications and routes them as events to the extension
// system.
class WebNavigationEventRouter : public TabStripModelObserver,
                                 public chrome::BrowserListObserver,
                                 public content::NotificationObserver {
 public:
  explicit WebNavigationEventRouter(Profile* profile);
  virtual ~WebNavigationEventRouter();

  // Invoked by the extensions service once the extension system is fully set
  // up and can start dispatching events to extensions.
  void Init();

 private:
  // Used to cache the information about newly created WebContents objects.
  struct PendingWebContents{
    PendingWebContents();
    PendingWebContents(content::WebContents* source_web_contents,
                       int64 source_frame_id,
                       bool source_frame_is_main_frame,
                       content::WebContents* target_web_contents,
                       const GURL& target_url);
    ~PendingWebContents();

    content::WebContents* source_web_contents;
    int64 source_frame_id;
    bool source_frame_is_main_frame;
    content::WebContents* target_web_contents;
    GURL target_url;
  };

  // TabStripModelObserver implementation.
  virtual void TabReplacedAt(TabStripModel* tab_strip_model,
                             TabContents* old_contents,
                             TabContents* new_contents,
                             int index) OVERRIDE;

  // chrome::BrowserListObserver implementation.
  virtual void OnBrowserAdded(Browser* browser) OVERRIDE;
  virtual void OnBrowserRemoved(Browser* browser) OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

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
class GetFrameFunction : public SyncExtensionFunction {
  virtual ~GetFrameFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("webNavigation.getFrame")
};

// API function that returns the states of all frames in a given tab.
class GetAllFramesFunction : public SyncExtensionFunction {
  virtual ~GetAllFramesFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("webNavigation.getAllFrames")
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_WEB_NAVIGATION_WEB_NAVIGATION_API_H_
