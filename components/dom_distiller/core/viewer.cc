// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/viewer.h"

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/strings/string_util.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/dom_distiller/core/proto/distilled_article.pb.h"
#include "components/dom_distiller/core/proto/distilled_page.pb.h"
#include "components/dom_distiller/core/task_tracker.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/dom_distiller/core/url_utils.h"
#include "grit/component_resources.h"
#include "grit/component_strings.h"
#include "net/base/escape.h"
#include "net/url_request/url_request.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

namespace dom_distiller {

namespace {

std::string ReplaceHtmlTemplateValues(const std::string& title,
                                      const std::string& content,
                                      const std::string& original_url) {
  base::StringPiece html_template =
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_DOM_DISTILLER_VIEWER_HTML);
  std::vector<std::string> substitutions;
  substitutions.push_back(title);         // $1
  substitutions.push_back(kCssPath);      // $2
  substitutions.push_back(title);         // $3
  substitutions.push_back(content);       // $4
  substitutions.push_back(original_url);  // $5
  substitutions.push_back(
      l10n_util::GetStringUTF8(IDS_DOM_DISTILLER_VIEWER_VIEW_ORIGINAL));  // $6
  return ReplaceStringPlaceholders(html_template, substitutions, NULL);
}

}  // namespace

namespace viewer {

const std::string GetUnsafeHtml(const DistilledArticleProto* article_proto) {
  DCHECK(article_proto);
  std::string title;
  std::string unsafe_article_html;
  if (article_proto->has_title() && article_proto->pages_size() > 0 &&
      article_proto->pages(0).has_html()) {
    title = net::EscapeForHTML(article_proto->title());
    // TODO(shashishekhar): Add support for correcting displaying multiple pages
    // after discussing the right way to display them.
    std::ostringstream unsafe_output_stream;
    for (int page_num = 0; page_num < article_proto->pages_size(); ++page_num) {
      unsafe_output_stream << article_proto->pages(page_num).html();
    }
    unsafe_article_html = unsafe_output_stream.str();
  } else {
    title = l10n_util::GetStringUTF8(IDS_DOM_DISTILLER_VIEWER_NO_DATA_TITLE);
    unsafe_article_html =
        l10n_util::GetStringUTF8(IDS_DOM_DISTILLER_VIEWER_NO_DATA_CONTENT);
  }

  std::string original_url;
  if (article_proto->pages_size() > 0 && article_proto->pages(0).has_url()) {
    original_url = article_proto->pages(0).url();
  }

  return ReplaceHtmlTemplateValues(title, unsafe_article_html, original_url);
}

const std::string GetErrorPageHtml() {
  std::string title = l10n_util::GetStringUTF8(
      IDS_DOM_DISTILLER_VIEWER_FAILED_TO_FIND_ARTICLE_TITLE);
  std::string content = l10n_util::GetStringUTF8(
      IDS_DOM_DISTILLER_VIEWER_FAILED_TO_FIND_ARTICLE_CONTENT);
  return ReplaceHtmlTemplateValues(title, content, "");
}

const std::string GetCss() {
  return ResourceBundle::GetSharedInstance()
      .GetRawDataResource(IDR_DISTILLER_CSS)
      .as_string();
}

scoped_ptr<ViewerHandle> CreateViewRequest(
    DomDistillerServiceInterface* dom_distiller_service,
    const std::string& path,
    ViewRequestDelegate* view_request_delegate) {
  std::string entry_id =
      url_utils::GetValueForKeyInUrlPathQuery(path, kEntryIdKey);
  bool has_valid_entry_id = !entry_id.empty();
  entry_id = StringToUpperASCII(entry_id);

  std::string requested_url_str =
      url_utils::GetValueForKeyInUrlPathQuery(path, kUrlKey);
  GURL requested_url(requested_url_str);
  bool has_valid_url = url_utils::IsUrlDistillable(requested_url);

  if (has_valid_entry_id && has_valid_url) {
    // It is invalid to specify a query param for both |kEntryIdKey| and
    // |kUrlKey|.
    return scoped_ptr<ViewerHandle>();
  }

  if (has_valid_entry_id) {
    return dom_distiller_service->ViewEntry(view_request_delegate, entry_id)
        .Pass();
  } else if (has_valid_url) {
    return dom_distiller_service->ViewUrl(view_request_delegate, requested_url)
        .Pass();
  }

  // It is invalid to not specify a query param for |kEntryIdKey| or |kUrlKey|.
  return scoped_ptr<ViewerHandle>();
}

}  // namespace viewer

}  // namespace dom_distiller
