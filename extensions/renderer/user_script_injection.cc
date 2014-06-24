// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/user_script_injection.h"

#include <vector>

#include "base/lazy_instance.h"
#include "base/metrics/histogram.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/renderer/dom_activity_logger.h"
#include "extensions/renderer/extension_groups.h"
#include "extensions/renderer/script_context.h"
#include "grit/extensions_renderer_resources.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebScriptSource.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

namespace extensions {

namespace {

// These two strings are injected before and after the Greasemonkey API and
// user script to wrap it in an anonymous scope.
const char kUserScriptHead[] = "(function (unsafeWindow) {\n";
const char kUserScriptTail[] = "\n})(window);";

// Greasemonkey API source that is injected with the scripts.
struct GreasemonkeyApiJsString {
  GreasemonkeyApiJsString();
  blink::WebScriptSource source;
};

// The below constructor, monstrous as it is, just makes a WebScriptSource from
// the GreasemonkeyApiJs resource.
GreasemonkeyApiJsString::GreasemonkeyApiJsString()
    : source(blink::WebScriptSource(blink::WebString::fromUTF8(
          ResourceBundle::GetSharedInstance()
              .GetRawDataResource(IDR_GREASEMONKEY_API_JS)
              .as_string()))) {
}

base::LazyInstance<GreasemonkeyApiJsString> g_greasemonkey_api =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

UserScriptInjection::UserScriptInjection(
    blink::WebFrame* web_frame,
    const std::string& extension_id,
    UserScript::RunLocation run_location,
    int tab_id,
    UserScriptSet* script_list,
    const UserScript* script)
    : ScriptInjection(web_frame,
                      extension_id,
                      run_location,
                      tab_id),
      script_(script),
      script_id_(script_->id()),
      user_script_set_observer_(this) {
  user_script_set_observer_.Add(script_list);
}

UserScriptInjection::~UserScriptInjection() {
}

void UserScriptInjection::OnUserScriptsUpdated(
    const std::set<std::string>& changed_extensions,
    const std::vector<UserScript*>& scripts) {
  // If the extension causing this injection changed, then this injection
  // will be removed, and there's no guarantee the backing script still exists.
  if (changed_extensions.count(extension_id()) > 0)
    return;

  for (std::vector<UserScript*>::const_iterator iter = scripts.begin();
       iter != scripts.end();
       ++iter) {
    // We need to compare to |script_id_| (and not to script_->id()) because the
    // old |script_| may be deleted by now.
    if ((*iter)->id() == script_id_) {
      script_ = *iter;
      break;
    }
  }
}

ScriptInjection::AccessType UserScriptInjection::Allowed(
    const Extension* extension) const {
  // If we don't have a tab id, we have no UI surface to ask for user consent.
  // For now, we treat this as an automatic allow.
  if (tab_id() == -1)
    return ALLOW_ACCESS;

  GURL effective_document_url = ScriptContext::GetEffectiveDocumentURL(
      web_frame(), web_frame()->document().url(), script_->match_about_blank());

  return extension->permissions_data()->RequiresActionForScriptExecution(
      extension, tab_id(), web_frame()->top()->document().url())
      ? REQUEST_ACCESS : ALLOW_ACCESS;
}

void UserScriptInjection::Inject(const Extension* extension,
                                 ScriptsRunInfo* scripts_run_info) {
  if (!script_->css_scripts().empty() &&
      run_location() == UserScript::DOCUMENT_START) {
    InjectCSS(scripts_run_info);
  }
  if (!script_->js_scripts().empty() &&
      script_->run_location() == run_location()) {
    InjectJS(extension, scripts_run_info);
  }
}

void UserScriptInjection::InjectJS(const Extension* extension,
                                   ScriptsRunInfo* scripts_run_info) {
  const UserScript::FileList& js_scripts = script_->js_scripts();
  std::vector<blink::WebScriptSource> sources;
  scripts_run_info->num_js += js_scripts.size();

  bool is_standalone_or_emulate_greasemonkey =
      script_->is_standalone() || script_->emulate_greasemonkey();
  for (UserScript::FileList::const_iterator iter = js_scripts.begin();
       iter != js_scripts.end();
       ++iter) {
    std::string content = iter->GetContent().as_string();

    // We add this dumb function wrapper for standalone user script to
    // emulate what Greasemonkey does.
    // TODO(aa): I think that maybe "is_standalone" scripts don't exist
    // anymore. Investigate.
    if (is_standalone_or_emulate_greasemonkey) {
      content.insert(0, kUserScriptHead);
      content += kUserScriptTail;
    }
    sources.push_back(blink::WebScriptSource(
        blink::WebString::fromUTF8(content), iter->url()));
  }

  // Emulate Greasemonkey API for scripts that were converted to extensions
  // and "standalone" user scripts.
  if (is_standalone_or_emulate_greasemonkey)
    sources.insert(sources.begin(), g_greasemonkey_api.Get().source);

  int isolated_world_id =
      GetIsolatedWorldIdForExtension(extension, web_frame());
  base::ElapsedTimer exec_timer;
  DOMActivityLogger::AttachToWorld(isolated_world_id, script_->extension_id());
  web_frame()->executeScriptInIsolatedWorld(isolated_world_id,
                                            &sources.front(),
                                            sources.size(),
                                            EXTENSION_GROUP_CONTENT_SCRIPTS);
  UMA_HISTOGRAM_TIMES("Extensions.InjectScriptTime", exec_timer.Elapsed());

  for (std::vector<blink::WebScriptSource>::const_iterator iter =
           sources.begin();
       iter != sources.end();
       ++iter) {
    scripts_run_info->executing_scripts[script_->extension_id()].insert(
        GURL(iter->url).path());
  }
}

void UserScriptInjection::InjectCSS(ScriptsRunInfo* scripts_run_info) {
  const UserScript::FileList& css_scripts = script_->css_scripts();
  scripts_run_info->num_css += css_scripts.size();
  for (UserScript::FileList::const_iterator iter = css_scripts.begin();
       iter != css_scripts.end();
       ++iter) {
    web_frame()->document().insertStyleSheet(
        blink::WebString::fromUTF8(iter->GetContent().as_string()));
  }
}

}  // namespace extensions
