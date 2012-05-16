// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the Chrome Extensions WebNavigation API functions for observing and
// intercepting navigation events, as specified in the extension JSON API.

#ifndef CHROME_BROWSER_EXTENSIONS_API_WEB_NAVIGATION_WEB_NAVIGATION_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_WEB_NAVIGATION_WEB_NAVIGATION_API_H_
#pragma once

#include <map>
#include <set>

#include "base/compiler_specific.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"
#include "googleurl/src/gurl.h"

struct RetargetingDetails;

namespace extensions {

// Tracks the navigation state of all frames in a given tab currently known to
// the webNavigation API. It is mainly used to track in which frames an error
// occurred so no further events for this frame are being sent.
class FrameNavigationState {
 public:
  typedef std::set<int64>::const_iterator const_iterator;

  FrameNavigationState();
  ~FrameNavigationState();

  // Use these to iterate over all frame IDs known by this object.
  const_iterator begin() const { return frame_ids_.begin(); }
  const_iterator end() const { return frame_ids_.end(); }

  // True if navigation events for the given frame can be sent.
  bool CanSendEvents(int64 frame_id) const;

  // True if in general webNavigation events may be sent for the given URL.
  bool IsValidUrl(const GURL& url) const;

  // Starts to track a frame identified by its |frame_id| showing the URL |url|.
  void TrackFrame(int64 frame_id,
                  const GURL& url,
                  bool is_main_frame,
                  bool is_error_page);

  // Update the URL associated with a given frame.
  void UpdateFrame(int64 frame_id, const GURL& url);

  // Returns true if |frame_id| is a known frame.
  bool IsValidFrame(int64 frame_id) const;

  // Returns the URL corresponding to a tracked frame given by its |frame_id|.
  GURL GetUrl(int64 frame_id) const;

  // True if the frame given by its |frame_id| is the main frame of its tab.
  bool IsMainFrame(int64 frame_id) const;

  // Returns the frame ID of the main frame, or -1 if the frame ID is not
  // known.
  int64 GetMainFrameID() const;

  // Marks a frame as in an error state, i.e. the onErrorOccurred event was
  // fired for this frame, and no further events should be sent for it.
  void SetErrorOccurredInFrame(int64 frame_id);

  // True if the frame is marked as being in an error state.
  bool GetErrorOccurredInFrame(int64 frame_id) const;

  // Marks a frame as having finished its last navigation, i.e. the onCompleted
  // event was fired for this frame.
  void SetNavigationCompleted(int64 frame_id);

  // True if the frame is currently not navigating.
  bool GetNavigationCompleted(int64 frame_id) const;

  // Marks a frame as having committed its navigation, i.e. the onCommitted
  // event was fired for this frame.
  void SetNavigationCommitted(int64 frame_id);

  // True if the frame has committed its navigation.
  bool GetNavigationCommitted(int64 frame_id) const;

  // Marks a frame as redirected by the server.
  void SetIsServerRedirected(int64 frame_id);

  // True if the frame was redirected by the server.
  bool GetIsServerRedirected(int64 frame_id) const;

#ifdef UNIT_TEST
  static void set_allow_extension_scheme(bool allow_extension_scheme) {
    allow_extension_scheme_ = allow_extension_scheme;
  }
#endif

 private:
  struct FrameState {
    bool error_occurred;  // True if an error has occurred in this frame.
    bool is_main_frame;  // True if this is a main frame.
    bool is_navigating;  // True if there is a navigation going on.
    bool is_committed;  // True if the navigation is already committed.
    bool is_server_redirected;  // True if a server redirect happened.
    GURL url;  // URL of this frame.
  };
  typedef std::map<int64, FrameState> FrameIdToStateMap;

  // Tracks the state of known frames.
  FrameIdToStateMap frame_state_map_;

  // Set of all known frames.
  std::set<int64> frame_ids_;

  // The current main frame.
  int64 main_frame_id_;

  // If true, also allow events from chrome-extension:// URLs.
  static bool allow_extension_scheme_;

  DISALLOW_COPY_AND_ASSIGN(FrameNavigationState);
};

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
class WebNavigationEventRouter : public content::NotificationObserver {
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
