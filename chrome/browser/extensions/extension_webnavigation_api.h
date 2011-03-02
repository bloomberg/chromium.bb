// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the Chrome Extensions WebNavigation API functions for observing and
// intercepting navigation events, as specified in
// chrome/common/extensions/api/extension_api.json.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_WEBNAVIGATION_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_WEBNAVIGATION_API_H_
#pragma once

#include <map>

#include "base/singleton.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "googleurl/src/gurl.h"

class NavigationController;
class ProvisionalLoadDetails;
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

  void set_allow_extension_scheme() { allow_extension_scheme_ = true; }

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
  bool allow_extension_scheme_;

  DISALLOW_COPY_AND_ASSIGN(FrameNavigationState);
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

#if defined(UNIT_TEST)
  // Also send events for chrome-extension:// URLs.
  void EnableExtensionScheme() {
    navigation_state_.set_allow_extension_scheme();
  }
#endif

 private:
  friend struct DefaultSingletonTraits<ExtensionWebNavigationEventRouter>;

  ExtensionWebNavigationEventRouter() {}
  virtual ~ExtensionWebNavigationEventRouter() {}

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Handler for the FRAME_PROVISIONAL_LOAD_START event. The method takes the
  // details of such an event and constructs a suitable JSON formatted extension
  // event from it.
  void FrameProvisionalLoadStart(NavigationController* controller,
                                 ProvisionalLoadDetails* details);

  // Handler for the FRAME_PROVISIONAL_LOAD_COMMITTED event. The method takes
  // the details of such an event and constructs a suitable JSON formatted
  // extension event from it.
  void FrameProvisionalLoadCommitted(NavigationController* controller,
                                     ProvisionalLoadDetails* details);

  // Handler for the FRAME_DOM_CONTENT_LOADED event. The method takes the frame
  // ID and constructs a suitable JSON formatted extension event from it.
  void FrameDomContentLoaded(NavigationController* controller,
                             int64 frame_id);

  // Handler for the FRAME_DID_FINISH_LOAD event. The method takes the frame
  // ID and constructs a suitable JSON formatted extension event from it.
  void FrameDidFinishLoad(NavigationController* controller, int64 frame_id);

  // Handler for the FAIL_PROVISIONAL_LOAD_WITH_ERROR event. The method takes
  // the details of such an event and constructs a suitable JSON formatted
  // extension event from it.
  void FailProvisionalLoadWithError(NavigationController* controller,
                                    ProvisionalLoadDetails* details);

  // Handler for the CREATING_NEW_WINDOW event. The method takes the details of
  // such an event and constructs a suitable JSON formatted extension event from
  // it.
  void CreatingNewWindow(TabContents* tab_content,
                         const ViewHostMsg_CreateWindow_Params* details);

  // True if the load details correspond to a reference fragment navigation.
  bool IsReferenceFragmentNavigation(ProvisionalLoadDetails* details);

  // Simulates a complete series of events for reference fragment navigations.
  void NavigatedReferenceFragment(NavigationController* controller,
                                  ProvisionalLoadDetails* details);

  // Tracks the state of the frames we are sending events for.
  FrameNavigationState navigation_state_;

  // Used for tracking registrations to navigation notifications.
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionWebNavigationEventRouter);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_WEBNAVIGATION_API_H_
