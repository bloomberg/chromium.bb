// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/user_script_slave.h"

#include <map>

#include "base/logging.h"
#include "base/memory/shared_memory.h"
#include "base/metrics/histogram.h"
#include "base/pickle.h"
#include "base/timer/elapsed_timer.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest_handlers/csp_info.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/renderer/extension_helper.h"
#include "extensions/renderer/extensions_renderer_client.h"
#include "extensions/renderer/script_context.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebSecurityOrigin.h"
#include "third_party/WebKit/public/web/WebSecurityPolicy.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "url/gurl.h"

using blink::WebFrame;
using blink::WebSecurityOrigin;
using blink::WebSecurityPolicy;
using blink::WebString;
using content::RenderThread;

namespace extensions {

int UserScriptSlave::GetIsolatedWorldIdForExtension(const Extension* extension,
                                                    WebFrame* frame) {
  static int g_next_isolated_world_id =
      ExtensionsRendererClient::Get()->GetLowestIsolatedWorldId();

  int id = 0;
  IsolatedWorldMap::iterator iter = isolated_world_ids_.find(extension->id());
  if (iter != isolated_world_ids_.end()) {
    id = iter->second;
  } else {
    id = g_next_isolated_world_id++;
    // This map will tend to pile up over time, but realistically, you're never
    // going to have enough extensions for it to matter.
    isolated_world_ids_[extension->id()] = id;
  }

  // We need to set the isolated world origin and CSP even if it's not a new
  // world since these are stored per frame, and we might not have used this
  // isolated world in this frame before.
  frame->setIsolatedWorldSecurityOrigin(
      id, WebSecurityOrigin::create(extension->url()));
  frame->setIsolatedWorldContentSecurityPolicy(
      id, WebString::fromUTF8(CSPInfo::GetContentSecurityPolicy(extension)));

  return id;
}

std::string UserScriptSlave::GetExtensionIdForIsolatedWorld(
    int isolated_world_id) {
  for (IsolatedWorldMap::iterator iter = isolated_world_ids_.begin();
       iter != isolated_world_ids_.end();
       ++iter) {
    if (iter->second == isolated_world_id)
      return iter->first;
  }
  return std::string();
}

void UserScriptSlave::RemoveIsolatedWorld(const std::string& extension_id) {
  isolated_world_ids_.erase(extension_id);
}

UserScriptSlave::UserScriptSlave(const ExtensionSet* extensions)
    : extensions_(extensions) {
}

UserScriptSlave::~UserScriptSlave() {
}

void UserScriptSlave::GetActiveExtensions(
    std::set<std::string>* extension_ids) {
  DCHECK(extension_ids);
  for (ScopedVector<ScriptInjection>::const_iterator iter =
           script_injections_.begin();
       iter != script_injections_.end();
       ++iter) {
    DCHECK(!(*iter)->extension_id().empty());
    extension_ids->insert((*iter)->extension_id());
  }
}

const Extension* UserScriptSlave::GetExtension(
    const std::string& extension_id) {
  return extensions_->GetByID(extension_id);
}

bool UserScriptSlave::UpdateScripts(
    base::SharedMemoryHandle shared_memory,
    const std::set<std::string>& changed_extensions) {
  bool only_inject_incognito =
      ExtensionsRendererClient::Get()->IsIncognitoProcess();

  // Create the shared memory object (read only).
  shared_memory_.reset(new base::SharedMemory(shared_memory, true));
  if (!shared_memory_.get())
    return false;

  // First get the size of the memory block.
  if (!shared_memory_->Map(sizeof(Pickle::Header)))
    return false;
  Pickle::Header* pickle_header =
      reinterpret_cast<Pickle::Header*>(shared_memory_->memory());

  // Now map in the rest of the block.
  int pickle_size = sizeof(Pickle::Header) + pickle_header->payload_size;
  shared_memory_->Unmap();
  if (!shared_memory_->Map(pickle_size))
    return false;

  // Unpickle scripts.
  uint64 num_scripts = 0;
  Pickle pickle(reinterpret_cast<char*>(shared_memory_->memory()), pickle_size);
  PickleIterator iter(pickle);
  CHECK(pickle.ReadUInt64(&iter, &num_scripts));

  // If we pass no explicit extension ids, we should refresh all extensions.
  bool include_all_extensions = changed_extensions.empty();

  // If we include all extensions, then we clear the script injections and
  // start from scratch. If not, then clear only the scripts for extension ids
  // that we are updating. This is important to maintain pending script
  // injection state for each ScriptInjection.
  if (include_all_extensions) {
    script_injections_.clear();
  } else {
    for (ScopedVector<ScriptInjection>::iterator iter =
             script_injections_.begin();
         iter != script_injections_.end();) {
      if (changed_extensions.count((*iter)->extension_id()) > 0)
        iter = script_injections_.erase(iter);
      else
        ++iter;
    }
  }

  script_injections_.reserve(num_scripts);
  for (uint64 i = 0; i < num_scripts; ++i) {
    scoped_ptr<UserScript> script(new UserScript());
    script->Unpickle(pickle, &iter);

    // Note that this is a pointer into shared memory. We don't own it. It gets
    // cleared up when the last renderer or browser process drops their
    // reference to the shared memory.
    for (size_t j = 0; j < script->js_scripts().size(); ++j) {
      const char* body = NULL;
      int body_length = 0;
      CHECK(pickle.ReadData(&iter, &body, &body_length));
      script->js_scripts()[j].set_external_content(
          base::StringPiece(body, body_length));
    }
    for (size_t j = 0; j < script->css_scripts().size(); ++j) {
      const char* body = NULL;
      int body_length = 0;
      CHECK(pickle.ReadData(&iter, &body, &body_length));
      script->css_scripts()[j].set_external_content(
          base::StringPiece(body, body_length));
    }

    // Don't add the script if it shouldn't shouldn't run in this tab, or if
    // we don't need to reload that extension.
    // It's a shame we don't catch this sooner, but since we lump all the user
    // scripts together, we can't skip parsing any.
    if ((only_inject_incognito && !script->is_incognito_enabled()) ||
        (!include_all_extensions &&
             changed_extensions.count(script->extension_id()) == 0)) {
      continue;
    }

    script_injections_.push_back(new ScriptInjection(script.Pass(), this));
  }

  return true;
}

void UserScriptSlave::InjectScripts(WebFrame* frame,
                                    UserScript::RunLocation location) {
  GURL document_url = ScriptInjection::GetDocumentUrlForFrame(frame);
  if (document_url.is_empty())
    return;

  ScriptInjection::ScriptsRunInfo scripts_run_info;
  for (ScopedVector<ScriptInjection>::const_iterator iter =
           script_injections_.begin();
       iter != script_injections_.end();
       ++iter) {
    (*iter)->InjectIfAllowed(frame, location, document_url, &scripts_run_info);
  }

  LogScriptsRun(frame, location, scripts_run_info);
}

void UserScriptSlave::OnContentScriptGrantedPermission(
    content::RenderView* render_view, int request_id) {
  ScriptInjection::ScriptsRunInfo run_info;
  blink::WebFrame* frame = NULL;
  // Notify the injections that a request to inject has been granted.
  for (ScopedVector<ScriptInjection>::iterator iter =
           script_injections_.begin();
       iter != script_injections_.end();
       ++iter) {
    if ((*iter)->NotifyScriptPermitted(request_id,
                                       render_view,
                                       &run_info,
                                       &frame)) {
      DCHECK(frame);
      LogScriptsRun(frame, UserScript::RUN_DEFERRED, run_info);
      break;
    }
  }
}

void UserScriptSlave::FrameDetached(blink::WebFrame* frame) {
  for (ScopedVector<ScriptInjection>::iterator iter =
           script_injections_.begin();
       iter != script_injections_.end();
       ++iter) {
    (*iter)->FrameDetached(frame);
  }
}

void UserScriptSlave::LogScriptsRun(
    blink::WebFrame* frame,
    UserScript::RunLocation location,
    const ScriptInjection::ScriptsRunInfo& info) {
  // Notify the browser if any extensions are now executing scripts.
  if (!info.executing_scripts.empty()) {
    content::RenderView* render_view =
        content::RenderView::FromWebView(frame->view());
    render_view->Send(new ExtensionHostMsg_ContentScriptsExecuting(
        render_view->GetRoutingID(),
        info.executing_scripts,
        render_view->GetPageId(),
        ScriptContext::GetDataSourceURLForFrame(frame)));
  }

  switch (location) {
    case UserScript::DOCUMENT_START:
      UMA_HISTOGRAM_COUNTS_100("Extensions.InjectStart_CssCount",
                               info.num_css);
      UMA_HISTOGRAM_COUNTS_100("Extensions.InjectStart_ScriptCount",
                               info.num_js);
      if (info.num_css || info.num_js)
        UMA_HISTOGRAM_TIMES("Extensions.InjectStart_Time",
                            info.timer.Elapsed());
      break;
    case UserScript::DOCUMENT_END:
      UMA_HISTOGRAM_COUNTS_100("Extensions.InjectEnd_ScriptCount", info.num_js);
      if (info.num_js)
        UMA_HISTOGRAM_TIMES("Extensions.InjectEnd_Time", info.timer.Elapsed());
      break;
    case UserScript::DOCUMENT_IDLE:
      UMA_HISTOGRAM_COUNTS_100("Extensions.InjectIdle_ScriptCount",
                               info.num_js);
      if (info.num_js)
        UMA_HISTOGRAM_TIMES("Extensions.InjectIdle_Time", info.timer.Elapsed());
      break;
    case UserScript::RUN_DEFERRED:
      // TODO(rdevlin.cronin): Add histograms.
      break;
    case UserScript::UNDEFINED:
    case UserScript::RUN_LOCATION_LAST:
      NOTREACHED();
  }
}

}  // namespace extensions
