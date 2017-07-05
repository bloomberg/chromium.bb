// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/plugins/pdf_plugin_placeholder.h"

#include "chrome/grit/renderer_resources.h"
#include "gin/object_template_builder.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/jstemplate_builder.h"

gin::WrapperInfo PDFPluginPlaceholder::kWrapperInfo = {gin::kEmbedderNativeGin};

PDFPluginPlaceholder::PDFPluginPlaceholder(content::RenderFrame* render_frame,
                                           const blink::WebPluginParams& params,
                                           const std::string& html_data)
    : plugins::PluginPlaceholderBase(render_frame, params, html_data) {}

PDFPluginPlaceholder::~PDFPluginPlaceholder() {}

PDFPluginPlaceholder* PDFPluginPlaceholder::CreatePDFPlaceholder(
    content::RenderFrame* render_frame,
    const blink::WebPluginParams& params) {
  const base::StringPiece template_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_PDF_PLUGIN_HTML));
  base::DictionaryValue values;
  std::string html_data = webui::GetI18nTemplateHtml(template_html, &values);
  return new PDFPluginPlaceholder(render_frame, params, html_data);
}

v8::Local<v8::Value> PDFPluginPlaceholder::GetV8Handle(v8::Isolate* isolate) {
  return gin::CreateHandle(isolate, this).ToV8();
}

gin::ObjectTemplateBuilder PDFPluginPlaceholder::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<PDFPluginPlaceholder>::GetObjectTemplateBuilder(isolate)
      .SetMethod<void (PDFPluginPlaceholder::*)()>(
          "downloadPDF", &PDFPluginPlaceholder::DownloadPDFCallback);
}

void PDFPluginPlaceholder::DownloadPDFCallback() {
  // TODO(amberwon): Implement starting PDF download.
}
