// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/declarative_user_script_master.h"

#include <set>

#include "content/public/browser/browser_context.h"

namespace extensions {

DeclarativeUserScriptMaster::DeclarativeUserScriptMaster(
    content::BrowserContext* browser_context,
    const HostID& host_id)
    : host_id_(host_id),
      loader_(browser_context,
              host_id,
              false /* listen_for_extension_system_loaded */) {
}

DeclarativeUserScriptMaster::~DeclarativeUserScriptMaster() {
}

void DeclarativeUserScriptMaster::AddScript(const UserScript& script) {
  std::set<UserScript> set;
  set.insert(script);
  loader_.AddScripts(set);
}

void DeclarativeUserScriptMaster::RemoveScript(const UserScript& script) {
  std::set<UserScript> set;
  set.insert(script);
  loader_.RemoveScripts(set);
}

void DeclarativeUserScriptMaster::ClearScripts() {
  loader_.ClearScripts();
}

}  // namespace extensions
