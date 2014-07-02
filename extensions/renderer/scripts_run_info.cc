// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/scripts_run_info.h"

#include "base/metrics/histogram.h"
#include "content/public/renderer/render_view.h"
#include "extensions/common/extension_messages.h"
#include "extensions/renderer/script_context.h"
#include "third_party/WebKit/public/web/WebFrame.h"

namespace extensions {

ScriptsRunInfo::ScriptsRunInfo() : num_css(0u), num_js(0u) {
}

ScriptsRunInfo::~ScriptsRunInfo() {
}

void ScriptsRunInfo::LogRun(blink::WebFrame* frame,
                            UserScript::RunLocation location) {
  // Notify the browser if any extensions are now executing scripts.
  if (!executing_scripts.empty()) {
    content::RenderView* render_view =
        content::RenderView::FromWebView(frame->view());
    render_view->Send(new ExtensionHostMsg_ContentScriptsExecuting(
        render_view->GetRoutingID(),
        executing_scripts,
        ScriptContext::GetDataSourceURLForFrame(frame)));
  }

  switch (location) {
    case UserScript::DOCUMENT_START:
      UMA_HISTOGRAM_COUNTS_100("Extensions.InjectStart_CssCount", num_css);
      UMA_HISTOGRAM_COUNTS_100("Extensions.InjectStart_ScriptCount", num_js);
      if (num_css || num_js)
        UMA_HISTOGRAM_TIMES("Extensions.InjectStart_Time", timer.Elapsed());
      break;
    case UserScript::DOCUMENT_END:
      UMA_HISTOGRAM_COUNTS_100("Extensions.InjectEnd_ScriptCount", num_js);
      if (num_js)
        UMA_HISTOGRAM_TIMES("Extensions.InjectEnd_Time", timer.Elapsed());
      break;
    case UserScript::DOCUMENT_IDLE:
      UMA_HISTOGRAM_COUNTS_100("Extensions.InjectIdle_ScriptCount", num_js);
      if (num_js)
        UMA_HISTOGRAM_TIMES("Extensions.InjectIdle_Time", timer.Elapsed());
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
