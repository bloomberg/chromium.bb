// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CRASH_REPORT_CRASH_REPORT_HELPER_H_
#define IOS_CHROME_BROWSER_CRASH_REPORT_CRASH_REPORT_HELPER_H_

@class NSString;
@class TabModel;

namespace web {
class WebState;
}  // namespace web

class WebStateList;

namespace breakpad {

// Monitors the urls loaded by |web_state| to allow crash reports to contain the
// current loading url.
void MonitorURLsForWebState(web::WebState* web_state);

// Stop monitoring the urls loaded by |web_state|.
void StopMonitoringURLsForWebState(web::WebState* web_state);

// Monitors the urls loaded in |web_state_list| to allow crash reports to
// contain the currently loaded urls.
void MonitorURLsForWebStateList(WebStateList* web_state_list);

// Stop monitoring the urls loaded in the |web_state_list|.
void StopMonitoringURLsForWebStateList(WebStateList* web_state_list);

// Adds the state monitor to |tab_model|. TabModels that are not monitored via
// this function are still monitored through notifications, but calling this
// function is mandatory to keep the monitoring of deleted tabs consistent.
void MonitorTabStateForTabModel(TabModel* tab_model);

// Stop the state monitor of |tab_model|.
void StopMonitoringTabStateForTabModel(TabModel* tab_model);

// Clear any state about the urls loaded in the given WebStateList; this should
// be called when the WebStateList is deactivated.
void ClearStateForWebStateList(WebStateList* web_state_list);

}  // namespace breakpad

#endif  // IOS_CHROME_BROWSER_CRASH_REPORT_CRASH_REPORT_HELPER_H_
