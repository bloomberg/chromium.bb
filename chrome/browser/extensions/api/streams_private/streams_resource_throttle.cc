// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/streams_private/streams_resource_throttle.h"

#include <string>

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_info_map.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_set.h"
#include "chrome/common/extensions/mime_types_handler.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_controller.h"
#include "content/public/browser/web_contents.h"
#include "net/url_request/url_request.h"

using extensions::Event;
using extensions::Extension;
using extensions::ExtensionSystem;

namespace {

const char* const kOnExecuteMimeTypeHandlerEvent =
    "streamsPrivate.onExecuteMimeTypeHandler";

// Goes through the extension's mime type handlers and checks it there is one
// that can handle the |mime_type|.
// |extension| must not be NULL.
bool CanHandleMimeType(const Extension* extension,
                       const std::string& mime_type) {
  MimeTypesHandler* handler = MimeTypesHandler::GetHandler(extension);
  if (!handler)
    return false;

  return handler->CanHandleMIMEType(mime_type);
}

// Retrieves Profile for a render view host specified by |render_process_id| and
// |render_view_id|.
Profile* GetProfile(int render_process_id, int render_view_id) {
  content::RenderViewHost* render_view_host =
      content::RenderViewHost::FromID(render_process_id, render_view_id);
  if (!render_view_host)
    return NULL;

  content::WebContents* web_contents =
      content::WebContents::FromRenderViewHost(render_view_host);
  if (!web_contents)
    return NULL;

  content::BrowserContext* browser_context = web_contents->GetBrowserContext();
  if (!browser_context)
    return NULL;

  return Profile::FromBrowserContext(browser_context);
}

void DispatchEventOnUIThread(const std::string& mime_type,
                             const GURL& request_url,
                             int render_process_id,
                             int render_view_id,
                             const std::string& extension_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  Profile* profile = GetProfile(render_process_id, render_view_id);
  if (!profile)
    return;

  // Create the event's arguments value.
  scoped_ptr<ListValue> event_args(new ListValue());
  event_args->Append(new base::StringValue(mime_type));
  event_args->Append(new base::StringValue(request_url.spec()));
  event_args->Append(new base::StringValue(request_url.spec()));

  scoped_ptr<Event> event(new Event(kOnExecuteMimeTypeHandlerEvent,
                                    event_args.Pass()));

  ExtensionSystem::Get(profile)->event_router()->DispatchEventToExtension(
      extension_id, event.Pass());
}

// Default implementation of StreamsPrivateEventRouter.
class StreamsPrivateEventRouterImpl
    : public StreamsResourceThrottle::StreamsPrivateEventRouter {
 public:
  StreamsPrivateEventRouterImpl() {}
  virtual ~StreamsPrivateEventRouterImpl() {}

  virtual void DispatchMimeTypeHandlerEvent(
      int render_process_id,
      int render_view_id,
      const std::string& mime_type,
      const GURL& request_url,
      const std::string& extension_id) OVERRIDE {
    // The event must be dispatched on the UI thread.
    content::BrowserThread::PostTask(
              content::BrowserThread::UI, FROM_HERE,
              base::Bind(&DispatchEventOnUIThread, mime_type, request_url,
                         render_process_id, render_view_id, extension_id));
  }
};

}  // namespace

// static
StreamsResourceThrottle* StreamsResourceThrottle::Create(
    int render_process_id,
    int render_view_id,
    net::URLRequest* request,
    bool profile_is_incognito,
    const ExtensionInfoMap* extension_info_map) {
  std::string mime_type;
  request->GetMimeType(&mime_type);
  scoped_ptr<StreamsPrivateEventRouter> event_router(
      new StreamsPrivateEventRouterImpl());
  return new StreamsResourceThrottle(render_process_id, render_view_id,
      mime_type, request->url(), profile_is_incognito, extension_info_map,
      event_router.Pass());
}

// static
StreamsResourceThrottle* StreamsResourceThrottle::CreateForTest(
    int render_process_id,
    int render_view_id,
    const std::string& mime_type,
    const GURL& request_url,
    bool profile_is_incognito,
    const ExtensionInfoMap* extension_info_map,
    scoped_ptr<StreamsPrivateEventRouter> event_router_in) {
  scoped_ptr<StreamsPrivateEventRouter> event_router =
      event_router_in.Pass();
  if (!event_router)
    event_router.reset(new StreamsPrivateEventRouterImpl());
  return new StreamsResourceThrottle(render_process_id, render_view_id,
      mime_type, request_url, profile_is_incognito, extension_info_map,
      event_router.Pass());
}

StreamsResourceThrottle::StreamsResourceThrottle(
    int render_process_id,
    int render_view_id,
    const std::string& mime_type,
    const GURL& request_url,
    bool profile_is_incognito,
    const ExtensionInfoMap* extension_info_map,
    scoped_ptr<StreamsPrivateEventRouter> event_router)
    : render_process_id_(render_process_id),
      render_view_id_(render_view_id),
      mime_type_(mime_type),
      request_url_(request_url),
      profile_is_incognito_(profile_is_incognito),
      extension_info_map_(extension_info_map),
      event_router_(event_router.Pass()) {
}

StreamsResourceThrottle::~StreamsResourceThrottle() {}

void StreamsResourceThrottle::WillProcessResponse(bool* defer) {
  std::vector<std::string> whitelist = MimeTypesHandler::GetMIMETypeWhitelist();
  // Go through the white-listed extensions and try to use them to intercept
  // the URL request.
  for (size_t i = 0; i < whitelist.size(); ++i) {
    if (MaybeInterceptWithExtension(whitelist[i]))
      return;
  }
}

bool StreamsResourceThrottle::MaybeInterceptWithExtension(
    const std::string& extension_id) {
  const Extension* extension =
      extension_info_map_->extensions().GetByID(extension_id);
  // The white-listed extension may not be installed, so we have to NULL check
  // |extension|.
  if (!extension)
    return false;

  // If in incognito mode, skip the extensions that are not incognito enabled.
  if (profile_is_incognito_ &&
      !extension_info_map_->IsIncognitoEnabled(extension_id)) {
    return false;
  }

  if (CanHandleMimeType(extension, mime_type_)) {
    event_router_->DispatchMimeTypeHandlerEvent(render_process_id_,
        render_view_id_, mime_type_, request_url_, extension->id());
    controller()->CancelAndIgnore();
    return true;
  }
  return false;
}
