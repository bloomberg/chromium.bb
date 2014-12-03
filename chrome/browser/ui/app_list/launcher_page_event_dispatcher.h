// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_LAUNCHER_PAGE_EVENT_DISPATCHER_H_
#define CHROME_BROWSER_UI_APP_LIST_LAUNCHER_PAGE_EVENT_DISPATCHER_H_

#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"

class Profile;

namespace base {
class ListValue;
}

namespace app_list {

// A class which sends API events to the custom launcher page.
class LauncherPageEventDispatcher {
 public:
  LauncherPageEventDispatcher(Profile* profile,
                              const std::string& extension_id);
  ~LauncherPageEventDispatcher();

  void ProgressChanged(double progress);
  void PopSubpage();

 private:
  void SendEventToLauncherPage(const std::string& event_name,
                               scoped_ptr<base::ListValue> args);

  Profile* profile_;
  std::string extension_id_;

  DISALLOW_COPY_AND_ASSIGN(LauncherPageEventDispatcher);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_LAUNCHER_PAGE_EVENT_DISPATCHER_H_
