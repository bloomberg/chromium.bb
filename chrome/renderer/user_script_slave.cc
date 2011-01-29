// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/user_script_slave.h"

#include <map>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/perftimer.h"
#include "base/pickle.h"
#include "base/shared_memory.h"
#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_set.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/extension_groups.h"
#include "chrome/renderer/render_thread.h"
#include "googleurl/src/gurl.h"
#include "grit/renderer_resources.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebVector.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/base/resource/resource_bundle.h"

using WebKit::WebFrame;
using WebKit::WebString;
using WebKit::WebVector;
using WebKit::WebView;

// These two strings are injected before and after the Greasemonkey API and
// user script to wrap it in an anonymous scope.
static const char kUserScriptHead[] = "(function (unsafeWindow) {\n";
static const char kUserScriptTail[] = "\n})(window);";

// Sets up the chrome.extension module. This may be run multiple times per
// context, but the init method deletes itself after the first time.
static const char kInitExtension[] =
  "if (chrome.initExtension) chrome.initExtension('%s', true, %s);";


int UserScriptSlave::GetIsolatedWorldId(const std::string& extension_id) {
  typedef std::map<std::string, int> IsolatedWorldMap;

  static IsolatedWorldMap g_isolated_world_ids;
  static int g_next_isolated_world_id = 1;

  IsolatedWorldMap::iterator iter = g_isolated_world_ids.find(extension_id);
  if (iter != g_isolated_world_ids.end())
    return iter->second;

  int new_id = g_next_isolated_world_id;
  ++g_next_isolated_world_id;

  // This map will tend to pile up over time, but realistically, you're never
  // going to have enough extensions for it to matter.
  g_isolated_world_ids[extension_id] = new_id;
  return new_id;
}

UserScriptSlave::UserScriptSlave(const ExtensionSet* extensions)
    : shared_memory_(NULL),
      script_deleter_(&scripts_),
      extensions_(extensions) {
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

  bool only_inject_incognito = RenderThread::current()->IsIncognitoProcess();

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
  void* iter = NULL;
  size_t num_scripts = 0;
  Pickle pickle(reinterpret_cast<char*>(shared_memory_->memory()),
                pickle_size);
  pickle.ReadSize(&iter, &num_scripts);

  scripts_.reserve(num_scripts);
  for (size_t i = 0; i < num_scripts; ++i) {
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

  // Push user styles down into WebCore
  WebView::removeAllUserContent();
  for (size_t i = 0; i < scripts_.size(); ++i) {
    UserScript* script = scripts_[i];
    if (script->css_scripts().empty())
      continue;

    WebVector<WebString> patterns;
    std::vector<WebString> temp_patterns;
    for (size_t k = 0; k < script->url_patterns().size(); ++k) {
      std::vector<URLPattern> explicit_patterns =
          script->url_patterns()[k].ConvertToExplicitSchemes();
      for (size_t m = 0; m < explicit_patterns.size(); ++m) {
        // Only include file schemes if the user has opted into that.
        if (!explicit_patterns[m].MatchesScheme(chrome::kFileScheme) ||
            script->allow_file_access()) {
          temp_patterns.push_back(WebString::fromUTF8(
              explicit_patterns[m].GetAsString()));
        }
      }
    }
    patterns.assign(temp_patterns);

    for (size_t j = 0; j < script->css_scripts().size(); ++j) {
      const UserScript::File& file = scripts_[i]->css_scripts()[j];
      std::string content = file.GetContent().as_string();

       WebView::addUserStyleSheet(
          WebString::fromUTF8(content),
          patterns,
           script->match_all_frames() ?
              WebView::UserContentInjectInAllFrames :
              WebView::UserContentInjectInTopFrameOnly);
    }
  }

  return true;
}

// static
void UserScriptSlave::InsertInitExtensionCode(
    std::vector<WebScriptSource>* sources, const std::string& extension_id) {
  DCHECK(sources);
  bool incognito = RenderThread::current()->IsIncognitoProcess();
  sources->insert(sources->begin(), WebScriptSource(WebString::fromUTF8(
      StringPrintf(kInitExtension, extension_id.c_str(),
                   incognito ? "true" : "false"))));
}

void UserScriptSlave::InjectScripts(WebFrame* frame,
                                    UserScript::RunLocation location) {
  GURL frame_url = GURL(frame->url());
  if (frame_url.is_empty())
    return;

  PerfTimer timer;
  int num_css = 0;
  int num_scripts = 0;

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

    if (!extension->CanExecuteScriptOnPage(frame_url, script, NULL))
      continue;

    if (frame_url.SchemeIsFile() && !script->allow_file_access())
      continue;  // This script isn't allowed to run on file URLs.

    // We rely on WebCore for CSS injection, but it's still useful to know how
    // many css files there are.
    if (location == UserScript::DOCUMENT_START)
      num_css += script->css_scripts().size();

    if (script->run_location() == location) {
      num_scripts += script->js_scripts().size();
      for (size_t j = 0; j < script->js_scripts().size(); ++j) {
        UserScript::File &file = script->js_scripts()[j];
        std::string content = file.GetContent().as_string();

        // We add this dumb function wrapper for standalone user script to
        // emulate what Greasemonkey does.
        if (script->is_standalone() || script->emulate_greasemonkey()) {
          content.insert(0, kUserScriptHead);
          content += kUserScriptTail;
        }
        sources.push_back(
            WebScriptSource(WebString::fromUTF8(content), file.url()));
      }
    }

    if (!sources.empty()) {
      int isolated_world_id = 0;

      // Emulate Greasemonkey API for scripts that were converted to extensions
      // and "standalone" user scripts.
      if (script->is_standalone() || script->emulate_greasemonkey()) {
        sources.insert(sources.begin(),
            WebScriptSource(WebString::fromUTF8(api_js_.as_string())));
      }

      // Setup chrome.self to contain an Extension object with the correct
      // ID.
      if (!script->extension_id().empty()) {
        InsertInitExtensionCode(&sources, script->extension_id());
        isolated_world_id = GetIsolatedWorldId(script->extension_id());
      }

      PerfTimer exec_timer;
      frame->executeScriptInIsolatedWorld(
          isolated_world_id, &sources.front(), sources.size(),
          EXTENSION_GROUP_CONTENT_SCRIPTS);
      UMA_HISTOGRAM_TIMES("Extensions.InjectScriptTime", exec_timer.Elapsed());
    }
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
