// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/plugins/non_loadable_plugin_placeholder.h"

#include "base/files/file_path.h"
#include "base/values.h"
#include "chrome/common/render_messages.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/renderer_resources.h"
#include "components/plugins/renderer/plugin_placeholder.h"
#include "content/app/strings/grit/content_strings.h"
#include "content/public/renderer/render_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/jstemplate_builder.h"

// static
plugins::PluginPlaceholder*
NonLoadablePluginPlaceholder::CreateNotSupportedPlugin(
    content::RenderFrame* render_frame,
    blink::WebLocalFrame* frame,
    const blink::WebPluginParams& params) {
  const base::StringPiece template_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_BLOCKED_PLUGIN_HTML));

  base::DictionaryValue values;
  values.SetString("message",
                   l10n_util::GetStringUTF8(IDS_PLUGIN_NOT_SUPPORTED));

  std::string html_data = webui::GetI18nTemplateHtml(template_html, &values);

  // PluginPlaceholder will destroy itself when its WebViewPlugin is going away.
  return new plugins::PluginPlaceholder(render_frame, frame, params, html_data);
}

// static
plugins::PluginPlaceholder* NonLoadablePluginPlaceholder::CreateErrorPlugin(
    content::RenderFrame* render_frame,
    const base::FilePath& file_path) {
  base::DictionaryValue values;
  values.SetString("message",
                   l10n_util::GetStringUTF8(IDS_PLUGIN_INITIALIZATION_ERROR));

  const base::StringPiece template_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_BLOCKED_PLUGIN_HTML));
  std::string html_data = webui::GetI18nTemplateHtml(template_html, &values);

  blink::WebPluginParams params;
  // PluginPlaceholder will destroy itself when its WebViewPlugin is going away.
  plugins::PluginPlaceholder* plugin =
      new plugins::PluginPlaceholder(render_frame, nullptr, params, html_data);

  content::RenderThread::Get()->Send(new ChromeViewHostMsg_CouldNotLoadPlugin(
      plugin->routing_id(), file_path));
  return plugin;
}
