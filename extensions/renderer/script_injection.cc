// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/script_injection.h"

#include <map>

#include "base/lazy_instance.h"
#include "content/public/renderer/render_view.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/manifest_handlers/csp_info.h"
#include "extensions/renderer/extensions_renderer_client.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebSecurityOrigin.h"

namespace extensions {

namespace {

typedef std::map<std::string, int> IsolatedWorldMap;
base::LazyInstance<IsolatedWorldMap> g_isolated_worlds =
    LAZY_INSTANCE_INITIALIZER;

const int kInvalidRequestId = -1;

// The id of the next pending injection.
int64 g_next_pending_id = 0;

bool ShouldDelayForPermission() {
  return FeatureSwitch::scripts_require_action()->IsEnabled();
}

}  // namespace

ScriptInjection::ScriptsRunInfo::ScriptsRunInfo() : num_css(0u), num_js(0u) {
}

ScriptInjection::ScriptsRunInfo::~ScriptsRunInfo() {
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

ScriptInjection::ScriptInjection(
    blink::WebFrame* web_frame,
    const std::string& extension_id,
    UserScript::RunLocation run_location,
    int tab_id)
    : web_frame_(web_frame),
      extension_id_(extension_id),
      run_location_(run_location),
      tab_id_(tab_id),
      request_id_(-1),
      complete_(false) {
}

ScriptInjection::~ScriptInjection() {
  if (!complete_)
    OnWillNotInject(WONT_INJECT);
}

bool ScriptInjection::TryToInject(UserScript::RunLocation current_location,
                                  const Extension* extension,
                                  ScriptsRunInfo* scripts_run_info) {
  if (current_location < run_location_)
    return false;  // Wait for the right location.

  if (request_id_ != -1)
    return false;  // We're waiting for permission right now, try again later.

  if (!extension) {
    NotifyWillNotInject(EXTENSION_REMOVED);
    return true;  // We're done.
  }

  switch (Allowed(extension)) {
    case DENY_ACCESS:
      NotifyWillNotInject(NOT_ALLOWED);
      return true;  // We're done.
    case REQUEST_ACCESS:
      RequestPermission();
      if (ShouldDelayForPermission())
        return false;  // Wait around for permission.
      // else fall through
    case ALLOW_ACCESS:
      InjectAndMarkComplete(extension, scripts_run_info);
      return true;  // We're done!
  }

  // Some compilers don't realize that we always return from the switch() above.
  // Make them happy.
  return false;
}

bool ScriptInjection::OnPermissionGranted(const Extension* extension,
                                          ScriptsRunInfo* scripts_run_info) {
  if (!extension) {
    NotifyWillNotInject(EXTENSION_REMOVED);
    return false;
  }

  InjectAndMarkComplete(extension, scripts_run_info);
  return true;
}

void ScriptInjection::RequestPermission() {
  content::RenderView* render_view =
      content::RenderView::FromWebView(web_frame()->top()->view());

  // If the feature to delay for permission isn't enabled, then just send an
  // invalid request (which is treated like a notification).
  int64 request_id = ShouldDelayForPermission() ? g_next_pending_id++
                                                : kInvalidRequestId;
  set_request_id(request_id);
  render_view->Send(new ExtensionHostMsg_RequestScriptInjectionPermission(
      render_view->GetRoutingID(),
      extension_id_,
      render_view->GetPageId(),
      request_id));
}

void ScriptInjection::NotifyWillNotInject(InjectFailureReason reason) {
  complete_ = true;
  OnWillNotInject(reason);
}

void ScriptInjection::InjectAndMarkComplete(const Extension* extension,
                                            ScriptsRunInfo* scripts_run_info) {
  complete_ = true;
  Inject(extension, scripts_run_info);
}

}  // namespace extensions
