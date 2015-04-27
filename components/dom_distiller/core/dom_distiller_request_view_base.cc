// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/dom_distiller_request_view_base.h"

#include <sstream>
#include <string>
#include <vector>

#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "components/dom_distiller/core/distilled_page_prefs.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/dom_distiller/core/task_tracker.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/dom_distiller/core/viewer.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/url_data_source.h"
#include "net/base/url_util.h"
#include "net/url_request/url_request.h"

namespace dom_distiller {

DomDistillerRequestViewBase::DomDistillerRequestViewBase(
    scoped_ptr<DistillerDataCallback> callback,
    DistilledPagePrefs* distilled_page_prefs)
    : callback_(callback.Pass()),
      page_count_(0),
      distilled_page_prefs_(distilled_page_prefs),
      is_error_page_(false) {
}

DomDistillerRequestViewBase::~DomDistillerRequestViewBase() {
}

void DomDistillerRequestViewBase::FlagAsErrorPage() {
  is_error_page_ = true;
  std::string error_page_html =
      viewer::GetErrorPageHtml(distilled_page_prefs_->GetTheme(),
                               distilled_page_prefs_->GetFontFamily());
  callback_->RunCallback(error_page_html);
}

bool DomDistillerRequestViewBase::IsErrorPage() {
  return is_error_page_;
}

void DomDistillerRequestViewBase::OnArticleReady(
    const DistilledArticleProto* article_proto) {
  if (page_count_ == 0) {
    std::string unsafe_page_html = viewer::GetUnsafeArticleTemplateHtml(
        &article_proto->pages(0), distilled_page_prefs_->GetTheme(),
        distilled_page_prefs_->GetFontFamily());
    callback_->RunCallback(unsafe_page_html);
    // Send first page to client.
    SendJavaScript(viewer::GetUnsafeArticleContentJs(article_proto));
    // If any content was loaded, show the feedback form.
    SendJavaScript(viewer::GetShowFeedbackFormJs());
  } else if (page_count_ == article_proto->pages_size()) {
    // We may still be showing the "Loading" indicator.
    SendJavaScript(viewer::GetToggleLoadingIndicatorJs(true));
  } else {
    // It's possible that we didn't get some incremental updates from the
    // distiller. Ensure all remaining pages are flushed to the viewer.
    for (; page_count_ < article_proto->pages_size(); page_count_++) {
      const DistilledPageProto& page = article_proto->pages(page_count_);
      SendJavaScript(viewer::GetUnsafeIncrementalDistilledPageJs(
          &page, page_count_ == article_proto->pages_size()));
    }
  }
  // No need to hold on to the ViewerHandle now that distillation is complete.
  viewer_handle_.reset();
}

void DomDistillerRequestViewBase::OnArticleUpdated(
    ArticleDistillationUpdate article_update) {
  for (; page_count_ < static_cast<int>(article_update.GetPagesSize());
       page_count_++) {
    const DistilledPageProto& page =
        article_update.GetDistilledPage(page_count_);
    // Send the page content to the client. This will execute after the page is
    // ready.
    SendJavaScript(viewer::GetUnsafeIncrementalDistilledPageJs(&page, false));

    if (page_count_ == 0) {
      // This is the first page, so send Viewer page scaffolding too.
      std::string unsafe_page_html = viewer::GetUnsafeArticleTemplateHtml(
          &page, distilled_page_prefs_->GetTheme(),
          distilled_page_prefs_->GetFontFamily());
      callback_->RunCallback(unsafe_page_html);
      // If any content was loaded, show the feedback form.
      SendJavaScript(viewer::GetShowFeedbackFormJs());
    }
  }
}

void DomDistillerRequestViewBase::OnChangeTheme(
    DistilledPagePrefs::Theme new_theme) {
  SendJavaScript(viewer::GetDistilledPageThemeJs(new_theme));
}

void DomDistillerRequestViewBase::OnChangeFontFamily(
    DistilledPagePrefs::FontFamily new_font) {
  SendJavaScript(viewer::GetDistilledPageFontFamilyJs(new_font));
}

void DomDistillerRequestViewBase::TakeViewerHandle(
    scoped_ptr<ViewerHandle> viewer_handle) {
  viewer_handle_ = viewer_handle.Pass();
}

}  // namespace dom_distiller
