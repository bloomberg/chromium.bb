// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/script_injection.h"

#include <vector>

#include "base/lazy_instance.h"
#include "base/metrics/histogram.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/render_view.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/renderer/dom_activity_logger.h"
#include "extensions/renderer/extension_groups.h"
#include "extensions/renderer/extension_helper.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/user_script_slave.h"
#include "grit/renderer_resources.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebScriptSource.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

namespace extensions {

namespace {

// The id of the next pending injection.
int64 g_next_pending_id = 0;

// The number of an invalid request, which is used if the feature to delay
// script injection is not enabled.
const int64 kInvalidRequestId = -1;

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

struct ScriptInjection::PendingInjection {
  PendingInjection(blink::WebFrame* web_frame,
                   UserScript::RunLocation run_location,
                   int page_id);
  ~PendingInjection();

  // The globally-unique id of this request.
  int64 id;

  // The pointer to the web frame into which the script should be injected.
  // This is weak, but safe because we remove pending requests when a frame is
  // terminated.
  blink::WebFrame* web_frame;

  // The run location to inject at.
  // Note: This could be a lie - we might inject well after this run location
  // has come and gone. But we need to know it to know which scripts to inject.
  UserScript::RunLocation run_location;

  // The corresponding page id, to protect against races.
  int page_id;
};

ScriptInjection::PendingInjection::PendingInjection(
    blink::WebFrame* web_frame,
    UserScript::RunLocation run_location,
    int page_id)
    : id(g_next_pending_id++),
      web_frame(web_frame),
      run_location(run_location),
      page_id(page_id) {
}

ScriptInjection::PendingInjection::~PendingInjection() {
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

void ScriptInjection::InjectIfAllowed(blink::WebFrame* frame,
                                      UserScript::RunLocation run_location,
                                      const GURL& document_url,
                                      ScriptsRunInfo* scripts_run_info) {
  if (!WantsToRun(frame, run_location, document_url))
    return;

  const Extension* extension = user_script_slave_->GetExtension(extension_id_);
  DCHECK(extension);  // WantsToRun() should be false if there's no extension.

  // We use the top render view here (instead of the render view for the
  // frame), because script injection on any frame requires permission for
  // the top frame. Additionally, if we have to show any UI for permissions,
  // it should only be done on the top frame.
  content::RenderView* top_render_view =
      content::RenderView::FromWebView(frame->top()->view());

  int tab_id = ExtensionHelper::Get(top_render_view)->tab_id();

  // By default, we allow injection.
  bool should_inject = true;

  // Check if the extension requires user consent for injection *and* we have a
  // valid tab id (if we don't have a tab id, we have no UI surface to ask for
  // user consent).
  if (tab_id != -1 &&
      PermissionsData::RequiresActionForScriptExecution(
          extension,
          tab_id,
          frame->top()->document().url())) {
    int64 request_id = kInvalidRequestId;
    int page_id = top_render_view->GetPageId();

    // We only delay the injection if the feature is enabled.
    // Otherwise, we simply treat this as a notification by passing an invalid
    // id.
    if (FeatureSwitch::scripts_require_action()->IsEnabled()) {
      should_inject = false;
      ScopedVector<PendingInjection>::iterator pending_injection =
          pending_injections_.insert(
              pending_injections_.end(),
              new PendingInjection(frame, run_location, page_id));
      request_id = (*pending_injection)->id;
    }

    top_render_view->Send(
        new ExtensionHostMsg_RequestContentScriptPermission(
            top_render_view->GetRoutingID(),
            extension->id(),
            page_id,
            request_id));
  }

  if (should_inject)
    Inject(frame, run_location, scripts_run_info);
}

bool ScriptInjection::NotifyScriptPermitted(
    int64 request_id,
    content::RenderView* render_view,
    ScriptsRunInfo* scripts_run_info,
    blink::WebFrame** frame_out) {
  ScopedVector<PendingInjection>::iterator iter = pending_injections_.begin();
  while (iter != pending_injections_.end() && (*iter)->id != request_id)
    ++iter;

  // No matching request.
  if (iter == pending_injections_.end())
    return false;

  // We found the request, so pull it out of the pending list.
  scoped_ptr<PendingInjection> pending_injection(*iter);
  pending_injections_.weak_erase(iter);

  // Ensure the Page ID and Extension are still valid. Otherwise, don't inject.
  if (render_view->GetPageId() != pending_injection->page_id)
    return false;

  const Extension* extension = user_script_slave_->GetExtension(extension_id_);
  if (!extension)
    return false;

  // Everything matches! Inject the script.
  if (frame_out)
    *frame_out = pending_injection->web_frame;
  Inject(pending_injection->web_frame,
         pending_injection->run_location,
         scripts_run_info);
  return true;
}

void ScriptInjection::FrameDetached(blink::WebFrame* frame) {
  // Any pending injections associated with the given frame will never run.
  // Remove them.
  for (ScopedVector<PendingInjection>::iterator iter =
           pending_injections_.begin();
       iter != pending_injections_.end();) {
    if ((*iter)->web_frame == frame)
      pending_injections_.erase(iter);
    else
      ++iter;
  }
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
