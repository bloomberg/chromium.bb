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

// Tracks which frames are in an error state, and no navigation events should
// be sent for.
class FrameNavigationState {
 public:
  FrameNavigationState();
  ~FrameNavigationState();

  // True if navigation events for the given frame can be sent.
  bool CanSendEvents(long long frame_id) const;

  // Starts to track a frame given by its |frame_id| showing the URL |url| in
  // a |tab_contents|.
  void TrackFrame(long long frame_id,
                  const GURL& url,
                  bool is_main_frame,
                  const TabContents* tab_contents);

  // Returns the URL corresponding to a tracked frame given by its |frame_id|.
  GURL GetUrl(long long frame_id) const;

  // True if the frame given by its |frame_id| is the main frame of its tab.
  bool IsMainFrame(long long frame_id) const;

  // Marks a frame as in an error state.
  void ErrorOccurredInFrame(long long frame_id);

  // Removes state associated with this tab contents and all of its frames.
  void RemoveTabContentsState(const TabContents* tab_contents);

 private:
  typedef std::multimap<const TabContents*, long long> TabContentsToFrameIdMap;
  struct FrameState {
    bool error_occurred;  // True if an error has occurred in this frame.
    bool is_main_frame;  // True if this is a main frame.
    GURL url;  // URL of this frame.
  };
  typedef std::map<long long, FrameState> FrameIdToStateMap;

  // Tracks which frames belong to a given tab contents object.
  TabContentsToFrameIdMap tab_contents_map_;

  // Tracks the state of known frames.
  FrameIdToStateMap frame_state_map_;

  DISALLOW_COPY_AND_ASSIGN(FrameNavigationState);
};

// Observes navigation notifications and routes them as events to the extension
// system.
class ExtensionWebNavigationEventRouter : public NotificationObserver {
 public:
  // Single instance of the event router.
  static ExtensionWebNavigationEventRouter* GetInstance();

  void Init();

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
                             long long frame_id);

  // Handler for the FRAME_DID_FINISH_LOAD event. The method takes the frame
  // ID and constructs a suitable JSON formatted extension event from it.
  void FrameDidFinishLoad(NavigationController* controller, long long frame_id);

  // Handler for the FAIL_PROVISIONAL_LOAD_WITH_ERROR event. The method takes
  // the details of such an event and constructs a suitable JSON formatted
  // extension event from it.
  void FailProvisionalLoadWithError(NavigationController* controller,
                                    ProvisionalLoadDetails* details);

  // This method dispatches events to the extension message service.
  void DispatchEvent(Profile* context,
                     const char* event_name,
                     const std::string& json_args);

  // Tracks the state of the frames we are sending events for.
  FrameNavigationState navigation_state_;

  // Used for tracking registrations to navigation notifications.
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionWebNavigationEventRouter);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_WEBNAVIGATION_API_H_
