// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/guestview/guestview.h"

#include "base/lazy_instance.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/guestview/webview/webview_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

using content::WebContents;

namespace {

// <embedder_process_id, guest_instance_id> => GuestView*
typedef std::map<std::pair<int, int>, GuestView*> EmbedderGuestViewMap;
static base::LazyInstance<EmbedderGuestViewMap> embedder_guestview_map =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

GuestView::GuestView(WebContents* guest_web_contents,
                     WebContents* embedder_web_contents,
                     const std::string& extension_id,
                     int view_instance_id,
                     const base::DictionaryValue& args)
    : embedder_web_contents_(embedder_web_contents),
      extension_id_(extension_id),
      embedder_render_process_id_(
          embedder_web_contents->GetRenderProcessHost()->GetID()),
      browser_context_(guest_web_contents->GetBrowserContext()),
      guest_instance_id_(guest_web_contents->GetEmbeddedInstanceID()),
      view_instance_id_(view_instance_id) {
  std::pair<int, int> key(embedder_render_process_id_, guest_instance_id_);
  embedder_guestview_map.Get().insert(std::make_pair(key, this));
}

// static
GuestView* GuestView::From(int embedder_process_id, int guest_instance_id) {
  EmbedderGuestViewMap* guest_map = embedder_guestview_map.Pointer();
  EmbedderGuestViewMap::iterator it = guest_map->find(
      std::make_pair(embedder_process_id, guest_instance_id));
  return it == guest_map->end() ? NULL : it->second;
}

GuestView::~GuestView() {
  std::pair<int, int> key(embedder_render_process_id_, guest_instance_id_);
  embedder_guestview_map.Get().erase(key);
}

void GuestView::DispatchEvent(const std::string& event_name,
                              scoped_ptr<DictionaryValue> event) {
  Profile* profile = Profile::FromBrowserContext(browser_context_);

  extensions::EventFilteringInfo info;
  info.SetURL(GURL());
  info.SetInstanceID(guest_instance_id_);
  scoped_ptr<ListValue> args(new ListValue());
  args->Append(event.release());

  extensions::EventRouter::DispatchEvent(
      embedder_web_contents_, profile, extension_id_,
      event_name, args.Pass(),
      extensions::EventRouter::USER_GESTURE_UNKNOWN, info);
}
