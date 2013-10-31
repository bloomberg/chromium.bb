// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/guestview/guestview.h"

#include "base/lazy_instance.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/guestview/adview/adview_guest.h"
#include "chrome/browser/guestview/guestview_constants.h"
#include "chrome/browser/guestview/webview/webview_guest.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

using content::WebContents;

namespace {

// <embedder_process_id, guest_instance_id> => GuestView*
typedef std::map<std::pair<int, int>, GuestView*> EmbedderGuestViewMap;
static base::LazyInstance<EmbedderGuestViewMap> embedder_guestview_map =
    LAZY_INSTANCE_INITIALIZER;

typedef std::map<WebContents*, GuestView*> WebContentsGuestViewMap;
static base::LazyInstance<WebContentsGuestViewMap> webcontents_guestview_map =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

GuestView::Event::Event(const std::string& event_name,
                        scoped_ptr<DictionaryValue> args)
    : event_name_(event_name),
      args_(args.Pass()) {
}

GuestView::Event::~Event() {
}

scoped_ptr<DictionaryValue> GuestView::Event::GetArguments() {
  return args_.Pass();
}

GuestView::GuestView(WebContents* guest_web_contents,
                     const std::string& extension_id)
    : guest_web_contents_(guest_web_contents),
      embedder_web_contents_(NULL),
      extension_id_(extension_id),
      embedder_render_process_id_(0),
      browser_context_(guest_web_contents->GetBrowserContext()),
      guest_instance_id_(guest_web_contents->GetEmbeddedInstanceID()),
      view_instance_id_(guestview::kInstanceIDNone) {
  webcontents_guestview_map.Get().insert(
      std::make_pair(guest_web_contents, this));
}

// static
GuestView::Type GuestView::GetViewTypeFromString(const std::string& api_type) {
  if (api_type == "adview") {
    return GuestView::ADVIEW;
  } else if (api_type == "webview") {
    return GuestView::WEBVIEW;
  }
  return GuestView::UNKNOWN;
}

// static
GuestView* GuestView::Create(WebContents* guest_web_contents,
                             const std::string& extension_id,
                             GuestView::Type view_type) {
  switch (view_type) {
    case GuestView::WEBVIEW:
      return new WebViewGuest(guest_web_contents, extension_id);
    case GuestView::ADVIEW:
      return new AdViewGuest(guest_web_contents, extension_id);
    default:
      NOTREACHED();
      return NULL;
  }
}

// static
GuestView* GuestView::FromWebContents(WebContents* web_contents) {
  WebContentsGuestViewMap* guest_map = webcontents_guestview_map.Pointer();
  WebContentsGuestViewMap::iterator it = guest_map->find(web_contents);
  return it == guest_map->end() ? NULL : it->second;
}

// static
GuestView* GuestView::From(int embedder_process_id, int guest_instance_id) {
  EmbedderGuestViewMap* guest_map = embedder_guestview_map.Pointer();
  EmbedderGuestViewMap::iterator it = guest_map->find(
      std::make_pair(embedder_process_id, guest_instance_id));
  return it == guest_map->end() ? NULL : it->second;
}

void GuestView::Attach(content::WebContents* embedder_web_contents,
                       const base::DictionaryValue& args) {
  embedder_web_contents_ = embedder_web_contents;
  embedder_render_process_id_ =
      embedder_web_contents->GetRenderProcessHost()->GetID();
  args.GetInteger(guestview::kParameterInstanceId, &view_instance_id_);

  std::pair<int, int> key(embedder_render_process_id_, guest_instance_id_);
  embedder_guestview_map.Get().insert(std::make_pair(key, this));

  // GuestView::Attach is called prior to initialization (and initial
  // navigation) of the guest in the content layer in order to permit mapping
  // the necessary associations between the <*view> element and its guest. This
  // is needed by the <webview> WebRequest API to allow intercepting resource
  // requests during navigation. However, queued events should be fired after
  // content layer initialization in order to ensure that load events (such as
  // 'loadstop') fire in embedder after the contentWindow is available.
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&GuestView::SendQueuedEvents,
                  base::Unretained(this)));
}

GuestView::Type GuestView::GetViewType() const {
  return GuestView::UNKNOWN;
}

WebViewGuest* GuestView::AsWebView() {
  return NULL;
}

AdViewGuest* GuestView::AsAdView() {
  return NULL;
}

GuestView::~GuestView() {
  std::pair<int, int> key(embedder_render_process_id_, guest_instance_id_);
  embedder_guestview_map.Get().erase(key);

  webcontents_guestview_map.Get().erase(guest_web_contents());

  while (!pending_events_.empty()) {
    delete pending_events_.front();
    pending_events_.pop();
  }
}

void GuestView::DispatchEvent(Event* event) {
  if (!attached()) {
    pending_events_.push(event);
    return;
  }

  Profile* profile = Profile::FromBrowserContext(browser_context_);

  extensions::EventFilteringInfo info;
  info.SetURL(GURL());
  info.SetInstanceID(guest_instance_id_);
  scoped_ptr<ListValue> args(new ListValue());
  args->Append(event->GetArguments().release());

  extensions::EventRouter::DispatchEvent(
      embedder_web_contents_, profile, extension_id_,
      event->event_name(), args.Pass(),
      extensions::EventRouter::USER_GESTURE_UNKNOWN, info);

  delete event;
}

void GuestView::SendQueuedEvents() {
  if (!attached())
    return;

  while (!pending_events_.empty()) {
    Event* event = pending_events_.front();
    pending_events_.pop();
    DispatchEvent(event);
  }
}
