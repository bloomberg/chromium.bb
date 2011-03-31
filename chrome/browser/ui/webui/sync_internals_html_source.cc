// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sync_internals_html_source.h"

#include <algorithm>

#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/message_loop.h"
#include "base/string_piece.h"
#include "base/values.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/url_constants.h"
#include "grit/sync_internals_resources.h"
#include "ui/base/resource/resource_bundle.h"

SyncInternalsHTMLSource::SyncInternalsHTMLSource()
    : DataSource(chrome::kChromeUISyncInternalsHost,
                 MessageLoop::current()) {}

SyncInternalsHTMLSource::~SyncInternalsHTMLSource() {}

void SyncInternalsHTMLSource::StartDataRequest(const std::string& path,
                                               bool is_incognito,
                                               int request_id) {
  base::StringPiece html_template(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_SYNC_INTERNALS_INDEX_HTML));
  DictionaryValue localized_strings;
  SetFontAndTextDirection(&localized_strings);

  std::string html(html_template.data(), html_template.size());
  jstemplate_builder::AppendI18nTemplateSourceHtml(&html);
  jstemplate_builder::AppendJsTemplateSourceHtml(&html);
  jstemplate_builder::AppendJsonHtml(&localized_strings, &html);
  jstemplate_builder::AppendI18nTemplateProcessHtml(&html);

  scoped_refptr<RefCountedBytes> bytes(new RefCountedBytes());
  bytes->data.resize(html.size());
  std::copy(html.begin(), html.end(), bytes->data.begin());
  SendResponse(request_id, bytes);
}

std::string SyncInternalsHTMLSource::GetMimeType(
    const std::string& path) const {
  return "text/html";
}
