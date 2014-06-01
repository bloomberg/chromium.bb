// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/script_injection.h"

#include <vector>

#include "base/lazy_instance.h"
#include "base/metrics/histogram.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/renderer/dom_activity_logger.h"
#include "extensions/renderer/extension_groups.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/user_script_slave.h"
#include "grit/renderer_resources.h"
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
          ResourceBundle::GetSharedInstance().GetRawDataResource(
              IDR_GREASEMONKEY_API_JS).as_string()))) {
}

base::LazyInstance<GreasemonkeyApiJsString> g_greasemonkey_api =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

ScriptInjection::ScriptsRunInfo::ScriptsRunInfo() : num_css(0u), num_js(0u) {
}

ScriptInjection::ScriptsRunInfo::~ScriptsRunInfo() {
}

// static
GURL ScriptInjection::GetDocumentUrlForFrame(blink::WebFrame* frame) {
  GURL data_source_url = ScriptContext::GetDataSourceURLForFrame(frame);
  if (!data_source_url.is_empty() && frame->isViewSourceModeEnabled()) {
    data_source_url = GURL(content::kViewSourceScheme + std::string(":") +
                           data_source_url.spec());
  }

  return data_source_url;
}

ScriptInjection::ScriptInjection(
    scoped_ptr<UserScript> script,
    UserScriptSlave* user_script_slave)
    : script_(script.Pass()),
      extension_id_(script_->extension_id()),
      user_script_slave_(user_script_slave),
      is_standalone_or_emulate_greasemonkey_(
          script_->is_standalone() || script_->emulate_greasemonkey()) {
}

ScriptInjection::~ScriptInjection() {
}

bool ScriptInjection::WantsToRun(blink::WebFrame* frame,
                                 UserScript::RunLocation run_location,
                                 const GURL& document_url) const {
  if (frame->parent() && !script_->match_all_frames())
    return false;  // Only match subframes if the script declared it wanted to.

  const Extension* extension = user_script_slave_->GetExtension(extension_id_);
  // Since extension info is sent separately from user script info, they can
  // be out of sync. We just ignore this situation.
  if (!extension)
    return false;

  // Content scripts are not tab-specific.
  static const int kNoTabId = -1;
  // We don't have a process id in this context.
  static const int kNoProcessId = -1;

  GURL effective_document_url = ScriptContext::GetEffectiveDocumentURL(
      frame, document_url, script_->match_about_blank());

  if (!PermissionsData::CanExecuteScriptOnPage(extension,
                                               effective_document_url,
                                               frame->top()->document().url(),
                                               kNoTabId,
                                               script_.get(),
                                               kNoProcessId,
                                               NULL /* ignore error */)) {
    return false;
  }

  return ShouldInjectCSS(run_location) || ShouldInjectJS(run_location);
}

void ScriptInjection::Inject(blink::WebFrame* frame,
                             UserScript::RunLocation run_location,
                             ScriptsRunInfo* scripts_run_info) const {
  DCHECK(frame);
  DCHECK(scripts_run_info);
  DCHECK(WantsToRun(frame, run_location, GetDocumentUrlForFrame(frame)));
  DCHECK(user_script_slave_->GetExtension(extension_id_));

  if (ShouldInjectCSS(run_location))
    InjectCSS(frame, scripts_run_info);
  if (ShouldInjectJS(run_location))
    InjectJS(frame, scripts_run_info);
}

bool ScriptInjection::ShouldInjectJS(UserScript::RunLocation run_location)
    const {
  return !script_->js_scripts().empty() &&
         script_->run_location() == run_location;
}

bool ScriptInjection::ShouldInjectCSS(UserScript::RunLocation run_location)
    const {
  return !script_->css_scripts().empty() &&
         run_location == UserScript::DOCUMENT_START;
}

void ScriptInjection::InjectJS(blink::WebFrame* frame,
                               ScriptsRunInfo* scripts_run_info) const {
  const UserScript::FileList& js_scripts = script_->js_scripts();
  std::vector<blink::WebScriptSource> sources;
  scripts_run_info->num_js += js_scripts.size();
  for (UserScript::FileList::const_iterator iter = js_scripts.begin();
       iter != js_scripts.end();
       ++iter) {
    std::string content = iter->GetContent().as_string();

    // We add this dumb function wrapper for standalone user script to
    // emulate what Greasemonkey does.
    // TODO(aa): I think that maybe "is_standalone" scripts don't exist
    // anymore. Investigate.
    if (is_standalone_or_emulate_greasemonkey_) {
      content.insert(0, kUserScriptHead);
      content += kUserScriptTail;
    }
    sources.push_back(blink::WebScriptSource(
        blink::WebString::fromUTF8(content), iter->url()));
  }

  // Emulate Greasemonkey API for scripts that were converted to extensions
  // and "standalone" user scripts.
  if (is_standalone_or_emulate_greasemonkey_)
    sources.insert(sources.begin(), g_greasemonkey_api.Get().source);

  int isolated_world_id =
      user_script_slave_->GetIsolatedWorldIdForExtension(
          user_script_slave_->GetExtension(extension_id_), frame);
  base::ElapsedTimer exec_timer;
  DOMActivityLogger::AttachToWorld(isolated_world_id, extension_id_);
  frame->executeScriptInIsolatedWorld(isolated_world_id,
                                      &sources.front(),
                                      sources.size(),
                                      EXTENSION_GROUP_CONTENT_SCRIPTS);
  UMA_HISTOGRAM_TIMES("Extensions.InjectScriptTime", exec_timer.Elapsed());

  for (std::vector<blink::WebScriptSource>::const_iterator iter =
           sources.begin();
       iter != sources.end();
       ++iter) {
    scripts_run_info->executing_scripts[extension_id_].insert(
        GURL(iter->url).path());
  }
}

void ScriptInjection::InjectCSS(blink::WebFrame* frame,
                                ScriptsRunInfo* scripts_run_info) const {
  const UserScript::FileList& css_scripts = script_->css_scripts();
  scripts_run_info->num_css += css_scripts.size();
  for (UserScript::FileList::const_iterator iter = css_scripts.begin();
       iter != css_scripts.end();
       ++iter) {
    frame->document().insertStyleSheet(
        blink::WebString::fromUTF8(iter->GetContent().as_string()));
  }
}

}  // namespace extensions
