// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/plugins/shadow_dom_plugin_placeholder.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/render_messages.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/renderer/plugins/plugin_uma.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebPluginParams.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

class MissingPluginPlaceholder : public blink::WebPluginPlaceholder {
 public:
  // blink::WebPluginPlaceholder overrides
  blink::WebString message() const override {
    // TODO(jbroman): IDS_PLUGIN_SEARCHING if ENABLE_PLUGIN_INSTALLATION
    return l10n_util::GetStringUTF16(IDS_PLUGIN_NOT_SUPPORTED);
  }
};

}  // namespace

bool ShadowDOMPluginPlaceholderEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnablePluginPlaceholderShadowDom);
}

scoped_ptr<blink::WebPluginPlaceholder> CreateShadowDOMPlaceholderForPluginInfo(
    content::RenderFrame* render_frame,
    blink::WebLocalFrame* frame,
    const blink::WebPluginParams& orig_params) {
  using Status = ChromeViewHostMsg_GetPluginInfo_Status;

  if (!ShadowDOMPluginPlaceholderEnabled())
    return nullptr;

  std::string orig_mime_type = orig_params.mimeType.utf8();
  // TODO(jbroman): Investigate whether browser plugin needs special handling.
  ChromeViewHostMsg_GetPluginInfo_Output output;
#if defined(ENABLE_PLUGINS)
  render_frame->Send(
      new ChromeViewHostMsg_GetPluginInfo(render_frame->GetRoutingID(),
                                          GURL(orig_params.url),
                                          frame->top()->document().url(),
                                          orig_mime_type,
                                          &output));
#else
  output.status.value = Status::kNotFound;
#endif

  if (output.status.value == Status::kNotFound) {
    // TODO(jbroman): Handle YouTube specially here, as in
    // ChromeContentRendererClient::CreatePlugin.
    PluginUMAReporter::GetInstance()->ReportPluginMissing(orig_mime_type,
                                                          orig_params.url);
    return CreateShadowDOMPlaceholderForMissingPlugin();
  }

  return nullptr;
}

scoped_ptr<blink::WebPluginPlaceholder>
CreateShadowDOMPlaceholderForMissingPlugin() {
  return make_scoped_ptr(new MissingPluginPlaceholder);
}
