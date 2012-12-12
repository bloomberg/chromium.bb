// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_WINDOWS_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_WINDOWS_API_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/profiles/profile_keyed_service.h"

namespace extensions {
class WindowsEventRouter;

class TabsWindowsAPI : public ProfileKeyedService,
                       public extensions::EventRouter::Observer {
 public:
  explicit TabsWindowsAPI(Profile* profile);
  virtual ~TabsWindowsAPI();

  // Convenience method to get the TabsWindowsAPI for a profile.
  static TabsWindowsAPI* Get(Profile* profile);

  WindowsEventRouter* windows_event_router();

  // ProfileKeyedService implementation.
  virtual void Shutdown() OVERRIDE;

  // EventRouter::Observer implementation.
  virtual void OnListenerAdded(const extensions::EventListenerInfo& details)
      OVERRIDE;

 private:
  Profile* profile_;

  scoped_ptr<WindowsEventRouter> windows_event_router_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_WINDOWS_API_H_
