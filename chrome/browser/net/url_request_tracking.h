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
// If you make a request on behalf of a child process, please call this
// function. The default value will be -1 which will be interprepreted as
// originating from the browser itself.
//
// The ID is the child process' unique ID (not a PID) of the process originating
// the request. This is normally the renderer corresponding to the load. If a
// plugin process does a request through a renderer process this will be the
// plugin (the originator of the request).
void SetOriginProcessUniqueIDForRequest(int id, net::URLRequest* request);

// Returns the child process' unique ID that has been previously set by
// SetOriginProcessUniqueIDForRequest. If no ID has been set, the return
// value is -1. We use this to identify requests made by the browser process.
int GetOriginProcessUniqueIDForRequest(const net::URLRequest* request);

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_URL_REQUEST_TRACKING_H_
