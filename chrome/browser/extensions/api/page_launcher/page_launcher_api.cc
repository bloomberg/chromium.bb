// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/page_launcher/page_launcher_api.h"

#include "base/lazy_instance.h"
#include "base/memory/linked_ptr.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/common/extensions/api/page_launcher.h"
#include "chrome/common/extensions/api/page_launcher/page_launcher_handler.h"
#include "googleurl/src/gurl.h"

namespace extensions {

static base::LazyInstance<ProfileKeyedAPIFactory<PageLauncherAPI> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

PageLauncherAPI::PageLauncherAPI(Profile* profile) {
  (new PageLauncherHandler)->Register();
}

PageLauncherAPI::~PageLauncherAPI() {
}

// static
void PageLauncherAPI::DispatchOnClickedEvent(
    Profile* profile,
    const std::string& extension_id,
    const GURL& url,
    const std::string& mimetype,
    const std::string* page_title,
    const std::string* selected_text) {
  api::page_launcher::PageData data;
  data.url = url.spec();
  data.mimetype = mimetype;
  if (page_title)
    data.title.reset(new std::string(*page_title));
  if (selected_text)
    data.selection_text.reset(new std::string(*selected_text));

  scoped_ptr<Event> event(
      new Event("pageLauncher.onClicked",
                api::page_launcher::OnClicked::Create(data)));
  EventRouter* event_router = ExtensionSystem::Get(profile)->event_router();
  event_router->DispatchEventToExtension(extension_id, event.Pass());
}


// static
ProfileKeyedAPIFactory<PageLauncherAPI>* PageLauncherAPI::GetFactoryInstance() {
  return &g_factory.Get();
}

}  // namespace extensions
