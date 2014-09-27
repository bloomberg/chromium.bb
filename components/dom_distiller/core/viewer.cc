// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/viewer.h"

#include <string>
#include <vector>

#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "components/dom_distiller/core/distilled_page_prefs.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/dom_distiller/core/proto/distilled_article.pb.h"
#include "components/dom_distiller/core/proto/distilled_page.pb.h"
#include "components/dom_distiller/core/task_tracker.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/dom_distiller/core/url_utils.h"
#include "grit/components_resources.h"
#include "grit/components_strings.h"
#include "net/base/escape.h"
#include "net/url_request/url_request.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

namespace dom_distiller {

namespace {

// JS Themes. Must agree with useTheme() in dom_distiller_viewer.js.
const char kDarkJsTheme[] = "dark";
const char kLightJsTheme[] = "light";
const char kSepiaJsTheme[] = "sepia";

// CSS Theme classes.  Must agree with classes in distilledpage.css.
const char kDarkCssClass[] = "dark";
const char kLightCssClass[] = "light";
const char kSepiaCssClass[] = "sepia";

// JS FontFamilies. Must agree with useFontFamily() in dom_distiller_viewer.js.
const char kSerifJsFontFamily[] = "serif";
const char kSansSerifJsFontFamily[] = "sans-serif";
const char kMonospaceJsFontFamily[] = "monospace";

// CSS FontFamily classes.  Must agree with classes in distilledpage.css.
const char kSerifCssClass[] = "serif";
const char kSansSerifCssClass[] = "sans-serif";
const char kMonospaceCssClass[] = "monospace";

// Maps themes to JS themes.
const std::string GetJsTheme(DistilledPagePrefs::Theme theme) {
  if (theme == DistilledPagePrefs::DARK) {
    return kDarkJsTheme;
  } else if (theme == DistilledPagePrefs::SEPIA) {
    return kSepiaJsTheme;
  }
  return kLightJsTheme;
}

// Maps themes to CSS classes.
const std::string GetThemeCssClass(DistilledPagePrefs::Theme theme) {
  if (theme == DistilledPagePrefs::DARK) {
    return kDarkCssClass;
  } else if (theme == DistilledPagePrefs::SEPIA) {
    return kSepiaCssClass;
  }
  return kLightCssClass;
}

// Maps font families to JS font families.
const std::string GetJsFontFamily(DistilledPagePrefs::FontFamily font_family) {
  if (font_family == DistilledPagePrefs::SERIF) {
    return kSerifJsFontFamily;
  } else if (font_family == DistilledPagePrefs::MONOSPACE) {
    return kMonospaceJsFontFamily;
  }
  return kSansSerifJsFontFamily;
}

// Maps fontFamilies to CSS fontFamily classes.
const std::string GetFontCssClass(DistilledPagePrefs::FontFamily font_family) {
  if (font_family == DistilledPagePrefs::SERIF) {
    return kSerifCssClass;
  } else if (font_family == DistilledPagePrefs::MONOSPACE) {
    return kMonospaceCssClass;
  }
  return kSansSerifCssClass;
}

void EnsureNonEmptyTitleAndContent(std::string* title, std::string* content) {
  if (title->empty())
    *title = l10n_util::GetStringUTF8(IDS_DOM_DISTILLER_VIEWER_NO_DATA_TITLE);
  UMA_HISTOGRAM_BOOLEAN("DomDistiller.PageHasDistilledData", !content->empty());
  if (content->empty()) {
    *content = l10n_util::GetStringUTF8(
        IDS_DOM_DISTILLER_VIEWER_NO_DATA_CONTENT);
  }
}

std::string ReplaceHtmlTemplateValues(
    const std::string& title,
    const std::string& content,
    const std::string& loading_indicator_class,
    const std::string& original_url,
    const DistilledPagePrefs::Theme theme,
    const DistilledPagePrefs::FontFamily font_family) {
  base::StringPiece html_template =
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_DOM_DISTILLER_VIEWER_HTML);
  std::vector<std::string> substitutions;
  substitutions.push_back(title);                                         // $1
  substitutions.push_back(kViewerCssPath);                                // $2
  substitutions.push_back(kViewerJsPath);                                 // $3
  substitutions.push_back(GetThemeCssClass(theme) + " " +
                          GetFontCssClass(font_family));                  // $4
  substitutions.push_back(content);                                       // $5
  substitutions.push_back(loading_indicator_class);                       // $6
  substitutions.push_back(original_url);                                  // $7
  substitutions.push_back(
      l10n_util::GetStringUTF8(IDS_DOM_DISTILLER_VIEWER_VIEW_ORIGINAL));  // $8
  return ReplaceStringPlaceholders(html_template, substitutions, NULL);
}

}  // namespace

namespace viewer {

const std::string GetUnsafeIncrementalDistilledPageJs(
    const DistilledPageProto* page_proto,
    const bool is_last_page) {
  std::string output;
  base::StringValue value(page_proto->html());
  base::JSONWriter::Write(&value, &output);
  std::string page_update("addToPage(");
  page_update += output + ");";
  return page_update + GetToggleLoadingIndicatorJs(
      is_last_page);

}

const std::string GetToggleLoadingIndicatorJs(const bool is_last_page) {
  if (is_last_page)
    return "showLoadingIndicator(true);";
  else
    return "showLoadingIndicator(false);";
}

const std::string GetUnsafePartialArticleHtml(
    const DistilledPageProto* page_proto,
    const DistilledPagePrefs::Theme theme,
    const DistilledPagePrefs::FontFamily font_family) {
  DCHECK(page_proto);
  std::string title = net::EscapeForHTML(page_proto->title());
  std::ostringstream unsafe_output_stream;
  unsafe_output_stream << page_proto->html();
  std::string unsafe_article_html = unsafe_output_stream.str();
  EnsureNonEmptyTitleAndContent(&title, &unsafe_article_html);
  std::string original_url = page_proto->url();
  return ReplaceHtmlTemplateValues(
      title, unsafe_article_html, "visible", original_url, theme, font_family);
}

const std::string GetUnsafeArticleHtml(
    const DistilledArticleProto* article_proto,
    const DistilledPagePrefs::Theme theme,
    const DistilledPagePrefs::FontFamily font_family) {
  DCHECK(article_proto);
  std::string title;
  std::string unsafe_article_html;
  if (article_proto->has_title() && article_proto->pages_size() > 0 &&
      article_proto->pages(0).has_html()) {
    title = net::EscapeForHTML(article_proto->title());
    std::ostringstream unsafe_output_stream;
    for (int page_num = 0; page_num < article_proto->pages_size(); ++page_num) {
      unsafe_output_stream << article_proto->pages(page_num).html();
    }
    unsafe_article_html = unsafe_output_stream.str();
  }

  EnsureNonEmptyTitleAndContent(&title, &unsafe_article_html);

  std::string original_url;
  if (article_proto->pages_size() > 0 && article_proto->pages(0).has_url()) {
    original_url = article_proto->pages(0).url();
  }

  return ReplaceHtmlTemplateValues(
      title, unsafe_article_html, "hidden", original_url, theme, font_family);
}

const std::string GetErrorPageHtml(
    const DistilledPagePrefs::Theme theme,
    const DistilledPagePrefs::FontFamily font_family) {
  std::string title = l10n_util::GetStringUTF8(
      IDS_DOM_DISTILLER_VIEWER_FAILED_TO_FIND_ARTICLE_TITLE);
  std::string content = l10n_util::GetStringUTF8(
      IDS_DOM_DISTILLER_VIEWER_FAILED_TO_FIND_ARTICLE_CONTENT);
  return ReplaceHtmlTemplateValues(
      title, content, "hidden", "", theme, font_family);
}

const std::string GetCss() {
  return ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_DISTILLER_CSS).as_string();
}

const std::string GetJavaScript() {
  return ResourceBundle::GetSharedInstance()
      .GetRawDataResource(IDR_DOM_DISTILLER_VIEWER_JS)
      .as_string();
}

scoped_ptr<ViewerHandle> CreateViewRequest(
    DomDistillerServiceInterface* dom_distiller_service,
    const std::string& path,
    ViewRequestDelegate* view_request_delegate,
    const gfx::Size& render_view_size) {
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
    return dom_distiller_service->ViewEntry(
        view_request_delegate,
        dom_distiller_service->CreateDefaultDistillerPage(render_view_size),
        entry_id).Pass();
  } else if (has_valid_url) {
    return dom_distiller_service->ViewUrl(
        view_request_delegate,
        dom_distiller_service->CreateDefaultDistillerPage(render_view_size),
        requested_url).Pass();
  }

  // It is invalid to not specify a query param for |kEntryIdKey| or |kUrlKey|.
  return scoped_ptr<ViewerHandle>();
}

const std::string GetDistilledPageThemeJs(DistilledPagePrefs::Theme theme) {
  return "useTheme('" + GetJsTheme(theme) + "');";
}

const std::string GetDistilledPageFontFamilyJs(
    DistilledPagePrefs::FontFamily font_family) {
  return "useFontFamily('" + GetJsFontFamily(font_family) + "');";
}

}  // namespace viewer

}  // namespace dom_distiller
