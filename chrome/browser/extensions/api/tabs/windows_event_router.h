// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TABS_WINDOWS_EVENT_ROUTER_H_
#define CHROME_BROWSER_EXTENSIONS_API_TABS_WINDOWS_EVENT_ROUTER_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/window_controller_list_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#if defined(TOOLKIT_VIEWS)
#include "ui/views/focus/widget_focus_manager.h"
#endif

class Profile;

namespace base {
class ListValue;
}

namespace extensions {

// The WindowsEventRouter sends chrome.windows.* events to listeners
// inside extension process renderers. The router listens to *all* events,
// but will only route eventes within a profile to extension processes in the
// same profile.
class WindowsEventRouter : public WindowControllerListObserver,
#if defined(TOOLKIT_VIEWS)
                          public views::WidgetFocusChangeListener,
#endif
                          public content::NotificationObserver {
 public:
  explicit WindowsEventRouter(Profile* profile);
  virtual ~WindowsEventRouter();

  // WindowControllerListObserver methods:
  virtual void OnWindowControllerAdded(
      WindowController* window_controller) OVERRIDE;
  virtual void OnWindowControllerRemoved(
      WindowController* window) OVERRIDE;

#if defined(TOOLKIT_VIEWS)
  virtual void OnNativeFocusChange(gfx::NativeView focused_before,
                                   gfx::NativeView focused_now) OVERRIDE;
#endif

  // content::NotificationObserver.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // |window_controller| is NULL to indicate a focused window has lost focus.
  void OnActiveWindowChanged(WindowController* window_controller);

 private:
  void DispatchEvent(const std::string& event_name,
                     Profile* profile,
                     scoped_ptr<base::ListValue> args);

  content::NotificationRegistrar registrar_;

  // The main profile that owns this event router.
  Profile* profile_;

  // The profile the currently focused window belongs to; either the main or
  // incognito profile or NULL (none of the above). We remember this in order
  // to correctly handle focus changes between non-OTR and OTR windows.
  Profile* focused_profile_;

  // The currently focused window. We keep this so as to avoid sending multiple
  // windows.onFocusChanged events with the same windowId.
  int focused_window_id_;

  DISALLOW_COPY_AND_ASSIGN(WindowsEventRouter);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_TABS_WINDOWS_EVENT_ROUTER_H_
