// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the Chrome Extensions WebNavigation API functions for observing and
// intercepting navigation events, as specified in
// chrome/common/extensions/api/extension_api.json.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_WEBNAVIGATION_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_WEBNAVIGATION_API_H_
#pragma once

#include "base/singleton.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "googleurl/src/gurl.h"

class NavigationController;
class ProvisionalLoadDetails;

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

  // Handler for the FAIL_PROVISIONAL_LOAD_WITH_ERROR event. The method takes
  // the details of such an event and constructs a suitable JSON formatted
  // extension event from it.
  void FailProvisionalLoadWithError(NavigationController* controller,
                                    ProvisionalLoadDetails* details);

  // This method dispatches events to the extension message service.
  void DispatchEvent(Profile* context,
                     const char* event_name,
                     const std::string& json_args);

  // Used for tracking registrations to navigation notifications.
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionWebNavigationEventRouter);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_WEBNAVIGATION_API_H_
