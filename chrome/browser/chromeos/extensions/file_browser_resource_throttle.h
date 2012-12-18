// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_BROWSER_RESOURCE_THROTTLE_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_BROWSER_RESOURCE_THROTTLE_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/resource_throttle.h"
#include "googleurl/src/gurl.h"

class ExtensionInfoMap;
class ExtensionSet;

namespace net {
class URLRequest;
}

// Resource throttle that intercepts URL requests that can be handled by an
// installed, white-listed file browser handler extension.
// The requests are intercepted before processing the response because the
// request's MIME type is needed to determine if it can be handled by a file
// browser handler. It the request can be handled by a file browser handler
// extension, it gets canceled and the extension is notified to handle
// the requested URL.
class FileBrowserResourceThrottle : public content::ResourceThrottle {
 public:
  // Used to dispatch fileBrowserHandler events to notify an extension it should
  // handle an URL request.
  // Can be used on any thread. The actual dispatch tasks will be posted to the
  // UI thread.
  class FileBrowserHandlerEventRouter {
   public:
    virtual ~FileBrowserHandlerEventRouter() {}

    // Used to dispatch fileBrowserHandler.onExecuteContentHandler to the
    // extension with id |extension_id|. |mime_type| and |request_url| are the
    // URL request's parameters that should be handed to the extension.
    // |render_process_id| and |render_view_id| are used to determine the
    // profile from which the URL request came and in which the event should be
    // dispatched.
    virtual void DispatchMimeTypeHandlerEvent(
        int render_process_id,
        int render_view_id,
        const std::string& mime_type,
        const GURL& request_url,
        const std::string& extension_id) = 0;
  };

  // Creates a FileBrowserResourceThrottle for the |request|.
  // The throtlle's |mime_type_| and |url_request_| members are extracted from
  // the request.
  // The throttle's |event_router_| is created and set (it's a
  // FileBrowserHandlerEventRouterImpl instance; see the .cc file).
  static FileBrowserResourceThrottle* Create(
      int render_process_id,
      int render_view_id,
      net::URLRequest* request,
      bool profile_is_incognito,
      const ExtensionInfoMap* extension_info_map);

  // Creates a FileBrowserResourceThrottle to be used in a test.
  // |event_router| can hold NULL, in which case the throttle's |event_router_|
  // member is set to a FileBrowserHandlerEventRouterImpl instance, just like in
  // FileBrowserResourceThrottle::Create.
  static FileBrowserResourceThrottle* CreateForTest(
      int render_process_id,
      int renser_view_id,
      const std::string& mime_type,
      const GURL& request_url,
      bool profile_is_incognito,
      const ExtensionInfoMap* extension_info_map,
      scoped_ptr<FileBrowserHandlerEventRouter> event_router);

  virtual ~FileBrowserResourceThrottle();

  // content::ResourceThrottle implementation.
  // Calls |MaybeInterceptWithExtension| for all extension_id's white-listed to
  // use file browser handler MIME type filters.
  virtual void WillProcessResponse(bool* defer) OVERRIDE;

 private:
  // Use Create* methods to create a FileBrowserResourceThrottle instance.
  FileBrowserResourceThrottle(
      int render_process_id,
      int render_view_id,
      const std::string& mime_type,
      const GURL& request_url,
      bool profile_is_incognito,
      const ExtensionInfoMap* extension_info_map,
      scoped_ptr<FileBrowserHandlerEventRouter> event_router);

  // Checks if the extension has a registered file browser handler that can
  // handle the request's mime_type. If this is the case, the request is
  // canceled and fileBrowserHandler.onExtecuteContentHandler event is
  // dispatched to the extension.
  // Returns whether the URL request has been intercepted.
  bool MaybeInterceptWithExtension(const std::string& extension_id);

  // Render process id from which the request came.
  int render_process_id_;
  // Render view id from which the request came.
  int render_view_id_;
  // The request's MIME type.
  std::string mime_type_;
  // The request's URL.
  GURL request_url_;
  // Whether the request came from an incognito profile.
  bool profile_is_incognito_;
  // Map holding list of installed extensions. Must be used exclusively on IO
  // thread.
  const scoped_refptr<const ExtensionInfoMap> extension_info_map_;

  // Event router to be used to dispatch the fileBrowserHandler events.
  scoped_ptr<FileBrowserHandlerEventRouter> event_router_;
};

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_BROWSER_RESOURCE_THROTTLE_H_
