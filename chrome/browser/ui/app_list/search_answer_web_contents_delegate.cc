// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search_answer_web_contents_delegate.h"

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/search_box_model.h"
#include "ui/views/controls/webview/webview.h"

namespace app_list {

SearchAnswerWebContentsDelegate::SearchAnswerWebContentsDelegate(
    content::BrowserContext* browser_context,
    app_list::AppListModel* model)
    : model_(model),
      web_view_(base::MakeUnique<views::WebView>(browser_context)),
      web_contents_(
          content::WebContents::Create(content::WebContents::CreateParams(
              browser_context,
              content::SiteInstance::Create(browser_context)))),
      answer_server_url_(switches::AnswerServerUrl()) {
  Observe(web_contents_.get());
  web_contents_->SetDelegate(this);
  web_view_->set_owned_by_client();
  web_view_->SetWebContents(web_contents_.get());
}

SearchAnswerWebContentsDelegate::~SearchAnswerWebContentsDelegate() {}

views::View* SearchAnswerWebContentsDelegate::web_view() {
  return web_view_.get();
}

void SearchAnswerWebContentsDelegate::Update() {
  if (!answer_server_url_.is_valid())
    return;

  // Reset the state.
  received_answer_ = false;
  model_->SetSearchAnswerAvailable(false);

  if (!model_->search_engine_is_google())
    return;

  // Start a request to the answer server.
  base::string16 query;
  base::TrimWhitespace(model_->search_box()->text(), base::TRIM_ALL, &query);
  if (query.empty())
    return;

  // Lifetime of |prefixed_query| should be longer than the one of
  // |replacements|.
  base::string16 prefixed_query(base::UTF8ToUTF16("q=") + query);
  GURL::ReplacementsW replacements;
  replacements.SetQueryStr(prefixed_query);
  current_request_url_ = answer_server_url_.ReplaceComponents(replacements);

  web_contents_->GetController().LoadURL(
      current_request_url_, content::Referrer(),
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL, std::string());

  // We are going to call WebContents::GetPreferredSize().
  web_contents_->GetRenderViewHost()->EnablePreferredSizeMode();
}

void SearchAnswerWebContentsDelegate::UpdatePreferredSize(
    content::WebContents* web_contents,
    const gfx::Size& pref_size) {
  web_view_->SetPreferredSize(pref_size);
}

void SearchAnswerWebContentsDelegate::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->GetURL() != current_request_url_)
    return;

  if (!navigation_handle->HasCommitted() || navigation_handle->IsErrorPage() ||
      !navigation_handle->IsInMainFrame()) {
    return;
  }

  const net::HttpResponseHeaders* headers =
      navigation_handle->GetResponseHeaders();
  if (!headers || headers->response_code() != net::HTTP_OK ||
      !headers->HasHeaderValue("has_answer", "true")) {
    return;
  }

  received_answer_ = true;
}

void SearchAnswerWebContentsDelegate::DidStopLoading() {
  if (!received_answer_)
    return;

  model_->SetSearchAnswerAvailable(true);
}

}  // namespace app_list
