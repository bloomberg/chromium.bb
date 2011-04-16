// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A helper function for using JsTemplate. See jstemplate_builder.h for more
// info.

#include "chrome/common/jstemplate_builder.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "content/common/json_value_serializer.h"
#include "grit/common_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace jstemplate_builder {

std::string GetTemplateHtml(const base::StringPiece& html_template,
                            const DictionaryValue* json,
                            const base::StringPiece& template_id) {
  std::string output(html_template.data(), html_template.size());
  AppendJsonHtml(json, &output);
  AppendJsTemplateSourceHtml(&output);
  AppendJsTemplateProcessHtml(template_id, &output);
  return output;
}

std::string GetI18nTemplateHtml(const base::StringPiece& html_template,
                                const DictionaryValue* json) {
  std::string output(html_template.data(), html_template.size());
  AppendJsonHtml(json, &output);
  AppendI18nTemplateSourceHtml(&output);
  AppendI18nTemplateProcessHtml(&output);
  return output;
}

std::string GetTemplatesHtml(const base::StringPiece& html_template,
                             const DictionaryValue* json,
                             const base::StringPiece& template_id) {
  std::string output(html_template.data(), html_template.size());
  AppendI18nTemplateSourceHtml(&output);
  AppendJsTemplateSourceHtml(&output);
  AppendJsonHtml(json, &output);
  AppendI18nTemplateProcessHtml(&output);
  AppendJsTemplateProcessHtml(template_id, &output);
  return output;
}

void AppendJsonHtml(const DictionaryValue* json, std::string* output) {
  // Convert the template data to a json string.
  DCHECK(json) << "must include json data structure";

  std::string jstext;
  JSONStringValueSerializer serializer(&jstext);
  serializer.Serialize(*json);
  // </ confuses the HTML parser because it could be a </script> tag.  So we
  // replace </ with <\/.  The extra \ will be ignored by the JS engine.
  ReplaceSubstringsAfterOffset(&jstext, 0, "</", "<\\/");

  output->append("<script>");
  output->append("var templateData = ");
  output->append(jstext);
  output->append(";");
  output->append("</script>");
}

void AppendJsTemplateSourceHtml(std::string* output) {
  // fetch and cache the pointer of the jstemplate resource source text.
  static const base::StringPiece jstemplate_src(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_JSTEMPLATE_JS));

  if (jstemplate_src.empty()) {
    NOTREACHED() << "Unable to get jstemplate src";
    return;
  }

  output->append("<script>");
  output->append(jstemplate_src.data(), jstemplate_src.size());
  output->append("</script>");
}

void AppendJsTemplateProcessHtml(const base::StringPiece& template_id,
                                 std::string* output) {
  output->append("<script>");
  output->append("var tp = document.getElementById('");
  output->append(template_id.data(), template_id.size());
  output->append("');");
  output->append("jstProcess(new JsEvalContext(templateData), tp);");
  output->append("</script>");
}

void AppendI18nTemplateSourceHtml(std::string* output) {
  // fetch and cache the pointer of the jstemplate resource source text.
  static const base::StringPiece i18n_template_src(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_I18N_TEMPLATE_JS));

  if (i18n_template_src.empty()) {
    NOTREACHED() << "Unable to get i18n template src";
    return;
  }

  output->append("<script>");
  output->append(i18n_template_src.data(), i18n_template_src.size());
  output->append("</script>");
}

void AppendI18nTemplateProcessHtml(std::string* output) {
  output->append("<script>");
  output->append("i18nTemplate.process(document, templateData);");
  output->append("</script>");
}

}  // namespace jstemplate_builder
