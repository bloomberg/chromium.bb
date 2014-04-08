// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/user_script_slave.h"

#include <map>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/shared_memory.h"
#include "base/metrics/histogram.h"
#include "base/pickle.h"
#include "base/strings/stringprintf.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/chrome_render_process_observer.h"
#include "chrome/renderer/extensions/dom_activity_logger.h"
#include "chrome/renderer/extensions/extension_groups.h"
#include "chrome/renderer/isolated_world_ids.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest_handlers/csp_info.h"
#include "extensions/common/permissions/permissions_data.h"
#include "grit/renderer_resources.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebDataSource.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebSecurityOrigin.h"
#include "third_party/WebKit/public/web/WebSecurityPolicy.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

using blink::WebFrame;
using blink::WebSecurityOrigin;
using blink::WebSecurityPolicy;
using blink::WebString;
using blink::WebVector;
using blink::WebView;
using content::RenderThread;

namespace extensions {

// These two strings are injected before and after the Greasemonkey API and
// user script to wrap it in an anonymous scope.
static const char kUserScriptHead[] = "(function (unsafeWindow) {\n";
static const char kUserScriptTail[] = "\n})(window);";

int UserScriptSlave::GetIsolatedWorldIdForExtension(const Extension* extension,
                                                    WebFrame* frame) {
  static int g_next_isolated_world_id = chrome::ISOLATED_WORLD_ID_EXTENSIONS;

  IsolatedWorldMap::iterator iter = isolated_world_ids_.find(extension->id());
  if (iter != isolated_world_ids_.end()) {
    // We need to set the isolated world origin and CSP even if it's not a new
    // world since these are stored per frame, and we might not have used this
    // isolated world in this frame before.
    frame->setIsolatedWorldSecurityOrigin(
        iter->second,
        WebSecurityOrigin::create(extension->url()));
    frame->setIsolatedWorldContentSecurityPolicy(
        iter->second,
        WebString::fromUTF8(CSPInfo::GetContentSecurityPolicy(extension)));
    return iter->second;
  }

  int new_id = g_next_isolated_world_id;
  ++g_next_isolated_world_id;

  // This map will tend to pile up over time, but realistically, you're never
  // going to have enough extensions for it to matter.
  isolated_world_ids_[extension->id()] = new_id;
  frame->setIsolatedWorldSecurityOrigin(
      new_id,
      WebSecurityOrigin::create(extension->url()));
  frame->setIsolatedWorldContentSecurityPolicy(
      new_id,
      WebString::fromUTF8(CSPInfo::GetContentSecurityPolicy(extension)));
  return new_id;
}

std::string UserScriptSlave::GetExtensionIdForIsolatedWorld(
    int isolated_world_id) {
  for (IsolatedWorldMap::iterator iter = isolated_world_ids_.begin();
       iter != isolated_world_ids_.end(); ++iter) {
    if (iter->second == isolated_world_id)
      return iter->first;
  }
  return std::string();
}

void UserScriptSlave::RemoveIsolatedWorld(const std::string& extension_id) {
  isolated_world_ids_.erase(extension_id);
}

UserScriptSlave::UserScriptSlave(const ExtensionSet* extensions)
    : script_deleter_(&scripts_), extensions_(extensions) {
  api_js_ = ResourceBundle::GetSharedInstance().GetRawDataResource(
      IDR_GREASEMONKEY_API_JS);
}

UserScriptSlave::~UserScriptSlave() {}

void UserScriptSlave::GetActiveExtensions(
    std::set<std::string>* extension_ids) {
  for (size_t i = 0; i < scripts_.size(); ++i) {
    DCHECK(!scripts_[i]->extension_id().empty());
    extension_ids->insert(scripts_[i]->extension_id());
  }
}

bool UserScriptSlave::UpdateScripts(base::SharedMemoryHandle shared_memory) {
  scripts_.clear();

  bool only_inject_incognito =
      ChromeRenderProcessObserver::is_incognito_process();

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
  Pickle pickle(reinterpret_cast<char*>(shared_memory_->memory()),
                pickle_size);
  PickleIterator iter(pickle);
  CHECK(pickle.ReadUInt64(&iter, &num_scripts));

  scripts_.reserve(num_scripts);
  for (uint64 i = 0; i < num_scripts; ++i) {
    scripts_.push_back(new UserScript());
    UserScript* script = scripts_.back();
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

    if (only_inject_incognito && !script->is_incognito_enabled()) {
      // This script shouldn't run in an incognito tab.
      delete script;
      scripts_.pop_back();
    }
  }

  return true;
}

GURL UserScriptSlave::GetDataSourceURLForFrame(const WebFrame* frame) {
  // Normally we would use frame->document().url() to determine the document's
  // URL, but to decide whether to inject a content script, we use the URL from
  // the data source. This "quirk" helps prevents content scripts from
  // inadvertently adding DOM elements to the compose iframe in Gmail because
  // the compose iframe's dataSource URL is about:blank, but the document URL
  // changes to match the parent document after Gmail document.writes into
  // it to create the editor.
  // http://code.google.com/p/chromium/issues/detail?id=86742
  blink::WebDataSource* data_source = frame->provisionalDataSource() ?
      frame->provisionalDataSource() : frame->dataSource();
  CHECK(data_source);
  return GURL(data_source->request().url());
}

void UserScriptSlave::InjectScripts(WebFrame* frame,
                                    UserScript::RunLocation location) {
  GURL data_source_url = GetDataSourceURLForFrame(frame);
  if (data_source_url.is_empty())
    return;

  if (frame->isViewSourceModeEnabled())
    data_source_url = GURL(content::kViewSourceScheme + std::string(":") +
                           data_source_url.spec());

  base::ElapsedTimer timer;
  int num_css = 0;
  int num_scripts = 0;

  ExecutingScriptsMap extensions_executing_scripts;

  for (size_t i = 0; i < scripts_.size(); ++i) {
    std::vector<WebScriptSource> sources;
    UserScript* script = scripts_[i];

    if (frame->parent() && !script->match_all_frames())
      continue;  // Only match subframes if the script declared it wanted to.

    const Extension* extension = extensions_->GetByID(script->extension_id());

    // Since extension info is sent separately from user script info, they can
    // be out of sync. We just ignore this situation.
    if (!extension)
      continue;

    // Content scripts are not tab-specific.
    const int kNoTabId = -1;
    // We don't have a process id in this context.
    const int kNoProcessId = -1;
    if (!PermissionsData::CanExecuteScriptOnPage(extension,
                                                 data_source_url,
                                                 frame->top()->document().url(),
                                                 kNoTabId,
                                                 script,
                                                 kNoProcessId,
                                                 NULL)) {
      continue;
    }

    if (location == UserScript::DOCUMENT_START) {
      num_css += script->css_scripts().size();
      for (UserScript::FileList::const_iterator iter =
               script->css_scripts().begin();
           iter != script->css_scripts().end();
           ++iter) {
        frame->document().insertStyleSheet(
            WebString::fromUTF8(iter->GetContent().as_string()));
      }
    }

    if (script->run_location() == location) {
      num_scripts += script->js_scripts().size();
      for (size_t j = 0; j < script->js_scripts().size(); ++j) {
        UserScript::File &file = script->js_scripts()[j];
        std::string content = file.GetContent().as_string();

        // We add this dumb function wrapper for standalone user script to
        // emulate what Greasemonkey does.
        // TODO(aa): I think that maybe "is_standalone" scripts don't exist
        // anymore. Investigate.
        if (script->is_standalone() || script->emulate_greasemonkey()) {
          content.insert(0, kUserScriptHead);
          content += kUserScriptTail;
        }
        sources.push_back(
            WebScriptSource(WebString::fromUTF8(content), file.url()));
      }
    }

    if (!sources.empty()) {
      // Emulate Greasemonkey API for scripts that were converted to extensions
      // and "standalone" user scripts.
      if (script->is_standalone() || script->emulate_greasemonkey()) {
        sources.insert(sources.begin(),
            WebScriptSource(WebString::fromUTF8(api_js_.as_string())));
      }

      int isolated_world_id = GetIsolatedWorldIdForExtension(extension, frame);

      base::ElapsedTimer exec_timer;
      DOMActivityLogger::AttachToWorld(isolated_world_id, extension->id());
      frame->executeScriptInIsolatedWorld(
          isolated_world_id, &sources.front(), sources.size(),
          EXTENSION_GROUP_CONTENT_SCRIPTS);
      UMA_HISTOGRAM_TIMES("Extensions.InjectScriptTime", exec_timer.Elapsed());

      for (std::vector<WebScriptSource>::const_iterator iter = sources.begin();
           iter != sources.end(); ++iter) {
        extensions_executing_scripts[extension->id()].insert(
            GURL(iter->url).path());
      }
    }
  }

  // Notify the browser if any extensions are now executing scripts.
  if (!extensions_executing_scripts.empty()) {
    blink::WebFrame* top_frame = frame->top();
    content::RenderView* render_view =
        content::RenderView::FromWebView(top_frame->view());
    render_view->Send(new ExtensionHostMsg_ContentScriptsExecuting(
        render_view->GetRoutingID(),
        extensions_executing_scripts,
        render_view->GetPageId(),
        GetDataSourceURLForFrame(top_frame)));
  }

  // Log debug info.
  if (location == UserScript::DOCUMENT_START) {
    UMA_HISTOGRAM_COUNTS_100("Extensions.InjectStart_CssCount", num_css);
    UMA_HISTOGRAM_COUNTS_100("Extensions.InjectStart_ScriptCount", num_scripts);
    if (num_css || num_scripts)
      UMA_HISTOGRAM_TIMES("Extensions.InjectStart_Time", timer.Elapsed());
  } else if (location == UserScript::DOCUMENT_END) {
    UMA_HISTOGRAM_COUNTS_100("Extensions.InjectEnd_ScriptCount", num_scripts);
    if (num_scripts)
      UMA_HISTOGRAM_TIMES("Extensions.InjectEnd_Time", timer.Elapsed());
  } else if (location == UserScript::DOCUMENT_IDLE) {
    UMA_HISTOGRAM_COUNTS_100("Extensions.InjectIdle_ScriptCount", num_scripts);
    if (num_scripts)
      UMA_HISTOGRAM_TIMES("Extensions.InjectIdle_Time", timer.Elapsed());
  } else {
    NOTREACHED();
  }
}

}  // namespace extensions
