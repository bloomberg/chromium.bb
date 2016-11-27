// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_LAUNCHER_PAGE_EVENT_DISPATCHER_H_
#define CHROME_BROWSER_UI_APP_LIST_LAUNCHER_PAGE_EVENT_DISPATCHER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_event_histogram_value.h"

class Profile;

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
  // Dispatches |event| to |extension_id_|.
  void DispatchEvent(std::unique_ptr<extensions::Event> event);

  Profile* profile_;
  std::string extension_id_;

  DISALLOW_COPY_AND_ASSIGN(LauncherPageEventDispatcher);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_LAUNCHER_PAGE_EVENT_DISPATCHER_H_
