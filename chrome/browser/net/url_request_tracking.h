// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_URL_REQUEST_TRACKING_H_
#define CHROME_BROWSER_NET_URL_REQUEST_TRACKING_H_
#pragma once

namespace net {
class URLRequest;
}  // namespace net

namespace chrome_browser_net {

// Sets the given ID on the given request for later retrieval. This information
// duplicates a field in the ResourceDispatcherHost's user data, but is also
// set for non-ResourceDispatcher-related requests. Having this one global
// place allows us to do more general things, such as assigning traffic for the
// network view in the task manager.
//
// If you make a request on behalf of a child process other than a renderer,
// please call this function to store its PID (NOT its browser-assigned unique
// child ID).  For requests originating in a renderer or the browser itself,
// set a PID of zero (the default).
//
// TODO(wez): Get rid of the zero-PID hack & enforce that one is always set.
void SetOriginPIDForRequest(int pid, net::URLRequest* request);

// Returns the process ID of the request's originator, previously stored with
// SetOriginProcessIDForRequest, or zero if no PID has been set.  A PID of zero
// should be interpreted as meaning the request originated from a renderer
// process, or within the browser itself.
int GetOriginPIDForRequest(const net::URLRequest* request);

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_URL_REQUEST_TRACKING_H_
