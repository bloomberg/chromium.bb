// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/user_script_injector.h"

#include <vector>

#include "base/lazy_instance.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/scripts_run_info.h"
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
  blink::WebScriptSource GetSource() const;

 private:
  std::string source_;
};

// The below constructor, monstrous as it is, just makes a WebScriptSource from
// the GreasemonkeyApiJs resource.
GreasemonkeyApiJsString::GreasemonkeyApiJsString()
    : source_(ResourceBundle::GetSharedInstance()
                  .GetRawDataResource(IDR_GREASEMONKEY_API_JS)
                  .as_string()) {
}

blink::WebScriptSource GreasemonkeyApiJsString::GetSource() const {
  return blink::WebScriptSource(blink::WebString::fromUTF8(source_));
}

base::LazyInstance<GreasemonkeyApiJsString> g_greasemonkey_api =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

UserScriptInjector::UserScriptInjector(
    const UserScript* script,
    UserScriptSet* script_list)
    : script_(script),
      script_id_(script_->id()),
      extension_id_(script_->extension_id()),
      user_script_set_observer_(this) {
  user_script_set_observer_.Add(script_list);
}

UserScriptInjector::~UserScriptInjector() {
}

void UserScriptInjector::OnUserScriptsUpdated(
    const std::set<std::string>& changed_extensions,
    const std::vector<UserScript*>& scripts) {
  // If the extension causing this injection changed, then this injection
  // will be removed, and there's no guarantee the backing script still exists.
  if (changed_extensions.count(extension_id_) > 0)
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

UserScript::InjectionType UserScriptInjector::script_type() const {
  return UserScript::CONTENT_SCRIPT;
}

bool UserScriptInjector::ShouldExecuteInChildFrames() const {
  return false;
}

bool UserScriptInjector::ShouldExecuteInMainWorld() const {
  return false;
}

bool UserScriptInjector::IsUserGesture() const {
  return false;
}

bool UserScriptInjector::ExpectsResults() const {
  return false;
}

bool UserScriptInjector::ShouldInjectJs(
    UserScript::RunLocation run_location) const {
  return script_->run_location() == run_location &&
         !script_->js_scripts().empty();
}

bool UserScriptInjector::ShouldInjectCss(
    UserScript::RunLocation run_location) const {
  return run_location == UserScript::DOCUMENT_START &&
         !script_->css_scripts().empty();
}

PermissionsData::AccessType UserScriptInjector::CanExecuteOnFrame(
    const Extension* extension,
    blink::WebFrame* web_frame,
    int tab_id,
    const GURL& top_url) const {
  // If we don't have a tab id, we have no UI surface to ask for user consent.
  // For now, we treat this as an automatic allow.
  if (tab_id == -1)
    return PermissionsData::ACCESS_ALLOWED;

  GURL effective_document_url = ScriptContext::GetEffectiveDocumentURL(
      web_frame, web_frame->document().url(), script_->match_about_blank());

  return extension->permissions_data()->GetContentScriptAccess(
      extension,
      effective_document_url,
      top_url,
      tab_id,
      -1,  // no process id
      NULL /* ignore error */);
}

std::vector<blink::WebScriptSource> UserScriptInjector::GetJsSources(
    UserScript::RunLocation run_location) const {
  DCHECK_EQ(script_->run_location(), run_location);

  std::vector<blink::WebScriptSource> sources;
  const UserScript::FileList& js_scripts = script_->js_scripts();
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
    sources.insert(sources.begin(), g_greasemonkey_api.Get().GetSource());

  return sources;
}

std::vector<std::string> UserScriptInjector::GetCssSources(
    UserScript::RunLocation run_location) const {
  DCHECK_EQ(UserScript::DOCUMENT_START, run_location);

  std::vector<std::string> sources;
  const UserScript::FileList& css_scripts = script_->css_scripts();
  for (UserScript::FileList::const_iterator iter = css_scripts.begin();
       iter != css_scripts.end();
       ++iter) {
    sources.push_back(iter->GetContent().as_string());
  }
  return sources;
}

void UserScriptInjector::OnInjectionComplete(
    scoped_ptr<base::ListValue> execution_results,
    ScriptsRunInfo* scripts_run_info,
    UserScript::RunLocation run_location) {
  if (ShouldInjectJs(run_location)) {
    const UserScript::FileList& js_scripts = script_->js_scripts();
    scripts_run_info->num_js += js_scripts.size();
    for (UserScript::FileList::const_iterator iter = js_scripts.begin();
         iter != js_scripts.end();
         ++iter) {
      scripts_run_info->executing_scripts[extension_id_].insert(
          iter->url().path());
    }
  }

  if (ShouldInjectCss(run_location))
    scripts_run_info->num_css += script_->css_scripts().size();
}

void UserScriptInjector::OnWillNotInject(InjectFailureReason reason) {
}

}  // namespace extensions
