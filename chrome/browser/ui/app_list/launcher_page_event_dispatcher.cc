// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/launcher_page_event_dispatcher.h"

#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/launcher_page.h"
#include "extensions/browser/event_router.h"

namespace OnTransitionChanged =
    extensions::api::launcher_page::OnTransitionChanged;
namespace OnPopSubpage = extensions::api::launcher_page::OnPopSubpage;

namespace app_list {

LauncherPageEventDispatcher::LauncherPageEventDispatcher(
    Profile* profile,
    const std::string& extension_id)
    : profile_(profile), extension_id_(extension_id) {
}

LauncherPageEventDispatcher::~LauncherPageEventDispatcher() {
}

void LauncherPageEventDispatcher::ProgressChanged(double progress) {
  SendEventToLauncherPage(OnTransitionChanged::kEventName,
                          OnTransitionChanged::Create(progress));
}

void LauncherPageEventDispatcher::PopSubpage() {
  SendEventToLauncherPage(OnPopSubpage::kEventName, OnPopSubpage::Create());
}

void LauncherPageEventDispatcher::SendEventToLauncherPage(
    const std::string& event_name,
    scoped_ptr<base::ListValue> args) {
  scoped_ptr<extensions::Event> event(
      new extensions::Event(event_name, args.Pass()));
  extensions::EventRouter::Get(profile_)
      ->DispatchEventToExtension(extension_id_, event.Pass());
}

}  // namespace app_list
