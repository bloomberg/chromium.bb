// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the Chrome Extensions WebNavigation API functions for observing and
// intercepting navigation events, as specified in
// chrome/common/extensions/api/extension_api.json.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_WEBNAVIGATION_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_WEBNAVIGATION_API_H_
#pragma once

#include <map>

#include "base/memory/singleton.h"
#include "chrome/browser/extensions/extension_function.h"
#include "content/browser/tab_contents/tab_contents_observer.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "googleurl/src/gurl.h"

class TabContents;
struct ViewHostMsg_CreateWindow_Params;

// Tracks the navigation state of all frames currently known to the
// webNavigation API. It is mainly used to track in which frames an error
// occurred so no further events for this frame are being sent.
class FrameNavigationState {
 public:
  FrameNavigationState();
  ~FrameNavigationState();

  // True if navigation events for the given frame can be sent.
  bool CanSendEvents(int64 frame_id) const;

  // Starts to track a frame given by its |frame_id| showing the URL |url| in
  // a |tab_contents|.
  void TrackFrame(int64 frame_id,
                  const GURL& url,
                  bool is_main_frame,
                  bool is_error_page,
                  const TabContents* tab_contents);

  // Returns the URL corresponding to a tracked frame given by its |frame_id|.
  GURL GetUrl(int64 frame_id) const;

  // True if the frame given by its |frame_id| is the main frame of its tab.
  bool IsMainFrame(int64 frame_id) const;

  // Marks a frame as in an error state.
  void ErrorOccurredInFrame(int64 frame_id);

  // Removes state associated with this tab contents and all of its frames.
  void RemoveTabContentsState(const TabContents* tab_contents);

#ifdef UNIT_TEST
  static void set_allow_extension_scheme(bool allow_extension_scheme) {
    allow_extension_scheme_ = allow_extension_scheme;
  }
#endif

 private:
  typedef std::multimap<const TabContents*, int64> TabContentsToFrameIdMap;
  struct FrameState {
    bool error_occurred;  // True if an error has occurred in this frame.
    bool is_main_frame;  // True if this is a main frame.
    GURL url;  // URL of this frame.
  };
  typedef std::map<int64, FrameState> FrameIdToStateMap;

  // Tracks which frames belong to a given tab contents object.
  TabContentsToFrameIdMap tab_contents_map_;

  // Tracks the state of known frames.
  FrameIdToStateMap frame_state_map_;

  // If true, also allow events from chrome-extension:// URLs.
  static bool allow_extension_scheme_;

  DISALLOW_COPY_AND_ASSIGN(FrameNavigationState);
};

// Tab contents observer that forwards navigation events to the event router.
class ExtensionWebNavigationTabObserver : public TabContentsObserver {
 public:
  explicit ExtensionWebNavigationTabObserver(TabContents* tab_contents);
  virtual ~ExtensionWebNavigationTabObserver();

  // TabContentsObserver implementation.
  virtual void DidStartProvisionalLoadForFrame(int64 frame_id,
                                                 bool is_main_frame,
                                                 const GURL& validated_url,
                                                 bool is_error_page) OVERRIDE;
  virtual void DidCommitProvisionalLoadForFrame(
      int64 frame_id,
      bool is_main_frame,
      const GURL& url,
      PageTransition::Type transition_type) OVERRIDE;
  virtual void DidFailProvisionalLoad(int64 frame_id,
                                      bool is_main_frame,
                                      const GURL& validated_url,
                                      int error_code) OVERRIDE;
  virtual void DocumentLoadedInFrame(int64 frame_id) OVERRIDE;
  virtual void DidFinishLoad(int64 frame_id) OVERRIDE;
  virtual void TabContentsDestroyed(TabContents* tab) OVERRIDE;
  virtual void DidOpenURL(const GURL& url,
                          const GURL& referrer,
                          WindowOpenDisposition disposition,
                          PageTransition::Type transition);


 private:
  // True if the transition and target url correspond to a reference fragment
  // navigation.
  bool IsReferenceFragmentNavigation(int64 frame_id, const GURL& url);

  // Simulates a complete series of events for reference fragment navigations.
  void NavigatedReferenceFragment(int64 frame_id,
                                  bool is_main_frame,
                                  const GURL& url,
                                  PageTransition::Type transition_type);

  // Tracks the state of the frames we are sending events for.
  FrameNavigationState navigation_state_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionWebNavigationTabObserver);
};

// Observes navigation notifications and routes them as events to the extension
// system.
class ExtensionWebNavigationEventRouter : public NotificationObserver {
 public:
  // Returns the singleton instance of the event router.
  static ExtensionWebNavigationEventRouter* GetInstance();

  // Invoked by the extensions service once the extension system is fully set
  // up and can start dispatching events to extensions.
  void Init();

 private:
  friend struct DefaultSingletonTraits<ExtensionWebNavigationEventRouter>;

  ExtensionWebNavigationEventRouter();
  virtual ~ExtensionWebNavigationEventRouter();

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Handler for the CREATING_NEW_WINDOW event. The method takes the details of
  // such an event and constructs a suitable JSON formatted extension event from
  // it.
  void CreatingNewWindow(TabContents* tab_content,
                         const ViewHostMsg_CreateWindow_Params* details);

  // Used for tracking registrations to navigation notifications.
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionWebNavigationEventRouter);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_WEBNAVIGATION_API_H_
