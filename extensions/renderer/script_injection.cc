// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/script_injection.h"

#include <map>

#include "base/lazy_instance.h"
#include "extensions/common/manifest_handlers/csp_info.h"
#include "extensions/renderer/extensions_renderer_client.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebSecurityOrigin.h"

namespace extensions {

namespace {

typedef std::map<std::string, int> IsolatedWorldMap;
base::LazyInstance<IsolatedWorldMap> g_isolated_worlds =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

ScriptInjection::ScriptsRunInfo::ScriptsRunInfo() : num_css(0u), num_js(0u) {
}

ScriptInjection::ScriptsRunInfo::~ScriptsRunInfo() {
}

ScriptInjection::ScriptInjection(
    blink::WebFrame* web_frame,
    const std::string& extension_id,
    UserScript::RunLocation run_location,
    int tab_id)
    : web_frame_(web_frame),
      extension_id_(extension_id),
      run_location_(run_location),
      tab_id_(tab_id),
      request_id_(-1) {
}

ScriptInjection::~ScriptInjection() {
}

// static
int ScriptInjection::GetIsolatedWorldIdForExtension(const Extension* extension,
                                                    blink::WebFrame* frame) {
  static int g_next_isolated_world_id =
      ExtensionsRendererClient::Get()->GetLowestIsolatedWorldId();

  IsolatedWorldMap& isolated_worlds = g_isolated_worlds.Get();

  int id = 0;
  IsolatedWorldMap::iterator iter = isolated_worlds.find(extension->id());
  if (iter != isolated_worlds.end()) {
    id = iter->second;
  } else {
    id = g_next_isolated_world_id++;
    // This map will tend to pile up over time, but realistically, you're never
    // going to have enough extensions for it to matter.
    isolated_worlds[extension->id()] = id;
  }

  // We need to set the isolated world origin and CSP even if it's not a new
  // world since these are stored per frame, and we might not have used this
  // isolated world in this frame before.
  frame->setIsolatedWorldSecurityOrigin(
      id, blink::WebSecurityOrigin::create(extension->url()));
  frame->setIsolatedWorldContentSecurityPolicy(
      id,
      blink::WebString::fromUTF8(CSPInfo::GetContentSecurityPolicy(extension)));

  return id;
}

// static
std::string ScriptInjection::GetExtensionIdForIsolatedWorld(
    int isolated_world_id) {
  IsolatedWorldMap& isolated_worlds = g_isolated_worlds.Get();

  for (IsolatedWorldMap::iterator iter = isolated_worlds.begin();
       iter != isolated_worlds.end();
       ++iter) {
    if (iter->second == isolated_world_id)
      return iter->first;
  }
  return std::string();
}

// static
void ScriptInjection::RemoveIsolatedWorld(const std::string& extension_id) {
  g_isolated_worlds.Get().erase(extension_id);
}

}  // namespace extensions
