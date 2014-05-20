// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NATIVE_PERMISSION_PERMISSION_REQUEST_HANDLER_H
#define ANDROID_WEBVIEW_NATIVE_PERMISSION_PERMISSION_REQUEST_HANDLER_H

#include <map>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

namespace android_webview {

class AwPermissionRequest;
class AwPermissionRequestDelegate;
class PermissionRequestHandlerClient;

// This class is used to send the permission requests, or cancel ongoing
// requests.
// It is owned by AwContents and has 1x1 mapping to AwContents. All methods
// are running on UI thread.
class PermissionRequestHandler : public content::WebContentsObserver {
 public:
  PermissionRequestHandler(PermissionRequestHandlerClient* client,
                           content::WebContents* aw_contents);
  virtual ~PermissionRequestHandler();

  // Send the given |request| to PermissionRequestHandlerClient.
  void SendRequest(scoped_ptr<AwPermissionRequestDelegate> request);

  // Cancel the ongoing request initiated by |origin| for accessing |resources|.
  void CancelRequest(const GURL& origin, int64 resources);

  // Allow |origin| to access the |resources|.
  void PreauthorizePermission(const GURL& origin, int64 resources);

  // WebContentsObserver
  virtual void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) OVERRIDE;

 private:
  friend class TestPermissionRequestHandler;

  typedef std::vector<base::WeakPtr<AwPermissionRequest> >::iterator
      RequestIterator;

  // Return the request initiated by |origin| for accessing |resources|.
  RequestIterator FindRequest(const GURL& origin, int64 resources);

  // Cancel the given request.
  void CancelRequest(RequestIterator i);

  void CancelAllRequests();

  // Remove the invalid requests from requests_.
  void PruneRequests();

  // Return true if |origin| were preauthorized to access |resources|.
  bool Preauthorized(const GURL& origin, int64 resources);

  PermissionRequestHandlerClient* client_;

  // A list of ongoing requests.
  std::vector<base::WeakPtr<AwPermissionRequest> > requests_;

  std::map<std::string, int64> preauthorized_permission_;

  // The unique id of the active NavigationEntry of the WebContents that we were
  // opened for. Used to help expire on requests.
  int contents_unique_id_;

  DISALLOW_COPY_AND_ASSIGN(PermissionRequestHandler);
};

}  // namespace android_webivew

#endif  // ANDROID_WEBVIEW_NATIVE_PERMISSION_PERMISSION_REQUEST_HANDLER_H
