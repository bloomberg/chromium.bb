// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"

#include <string>

#include "base/memory/ref_counted_memory.h"
#include "base/string_util.h"
#include "chrome/common/jstemplate_builder.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"

ChromeWebUIDataSource::ChromeWebUIDataSource(const std::string& source_name)
    : DataSource(source_name, MessageLoop::current()),
      default_resource_(-1),
      json_js_format_v2_(false) {
}

ChromeWebUIDataSource::ChromeWebUIDataSource(const std::string& source_name,
                                             MessageLoop* loop)
    : DataSource(source_name, loop),
      default_resource_(-1),
      json_js_format_v2_(false) {
}

ChromeWebUIDataSource::~ChromeWebUIDataSource() {
}

void ChromeWebUIDataSource::AddString(const std::string& name,
                                      const string16& value) {
  localized_strings_.SetString(name, value);
}

void ChromeWebUIDataSource::AddLocalizedString(const std::string& name,
                                               int ids) {
  localized_strings_.SetString(name, l10n_util::GetStringUTF16(ids));
}

void ChromeWebUIDataSource::AddLocalizedStrings(
    const DictionaryValue& localized_strings) {
  localized_strings_.MergeDictionary(&localized_strings);
}

std::string ChromeWebUIDataSource::GetMimeType(const std::string& path) const {
  if (EndsWith(path, ".js", false))
    return "application/javascript";

  if (EndsWith(path, ".json", false))
    return "application/json";

  if (EndsWith(path, ".pdf", false))
    return "application/pdf";

  return "text/html";
}

void ChromeWebUIDataSource::StartDataRequest(const std::string& path,
                                             bool is_incognito,
                                             int request_id) {
  if (!json_path_.empty() && path == json_path_) {
    SendLocalizedStringsAsJSON(request_id);
  } else {
    int resource_id = default_resource_;
    std::map<std::string, int>::iterator result;
    result = path_to_idr_map_.find(path);
    if (result != path_to_idr_map_.end())
      resource_id = result->second;
    DCHECK_NE(resource_id, -1);
    SendFromResourceBundle(request_id, resource_id);
  }
}

void ChromeWebUIDataSource::SendLocalizedStringsAsJSON(int request_id) {
  std::string template_data;
  SetFontAndTextDirection(&localized_strings_);

  scoped_ptr<jstemplate_builder::UseVersion2> version2;
  if (json_js_format_v2_)
    version2.reset(new jstemplate_builder::UseVersion2);

  jstemplate_builder::AppendJsonJS(&localized_strings_, &template_data);
  SendResponse(request_id, base::RefCountedString::TakeString(&template_data));
}

void ChromeWebUIDataSource::SendFromResourceBundle(int request_id, int idr) {
  scoped_refptr<base::RefCountedStaticMemory> response(
      ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
          idr, ui::SCALE_FACTOR_NONE));
  SendResponse(request_id, response);
}
