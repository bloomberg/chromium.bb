// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// #include <algorithm>

#include "chrome/browser/ui/webui/chromeos/login/login_ui_helpers.h"

#include <algorithm>

#include "base/values.h"
#include "chrome/common/jstemplate_builder.h"
#include "grit/browser_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace chromeos {

// HTMLOperationsInterface, public:---------------------------------------------
base::StringPiece HTMLOperationsInterface::GetLoginHTML() {
  base::StringPiece login_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_LOGIN_HTML));
  return login_html;
}

std::string HTMLOperationsInterface::GetFullHTML(
    base::StringPiece login_html,
    DictionaryValue* localized_strings) {
  return jstemplate_builder::GetI18nTemplateHtml(
      login_html,
      localized_strings);
}

RefCountedBytes* HTMLOperationsInterface::CreateHTMLBytes(
    std::string full_html) {
  RefCountedBytes* html_bytes = new RefCountedBytes();
  html_bytes->data.resize(full_html.size());
  std::copy(full_html.begin(),
            full_html.end(),
            html_bytes->data.begin());
  return html_bytes;
}

}  // namespace chromeos
