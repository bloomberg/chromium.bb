// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_ANSWER_WEB_CONTENTS_DELEGATE_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_ANSWER_WEB_CONTENTS_DELEGATE_H_

#include <memory>
#include <string>

#include "base/time/time.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/app_list/app_list_model_observer.h"
#include "url/gurl.h"

class Profile;

namespace app_list {
class AppListModel;
}

namespace views {
class View;
class WebView;
}

namespace app_list {

// Manages the web contents for the search answer web view.
class SearchAnswerWebContentsDelegate : public content::WebContentsDelegate,
                                        public content::WebContentsObserver,
                                        public AppListModelObserver {
 public:
  SearchAnswerWebContentsDelegate(Profile* profile,
                                  app_list::AppListModel* model);

  ~SearchAnswerWebContentsDelegate() override;

  // Updates the state after the query string or any other relevant condition
  // changes.
  void Update();

  // Returns a pointer to the web view for the web contents managed by this
  // class. The object is owned by this class and has property
  // 'set_owned_by_client()' set.
  views::View* web_view();

  // content::WebContentsDelegate overrides:
  void UpdatePreferredSize(content::WebContents* web_contents,
                           const gfx::Size& pref_size) override;

  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) override;

  bool HandleContextMenu(const content::ContextMenuParams& params) override;

  // content::WebContentsObserver overrides:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidStopLoading() override;
  void DidGetUserInteraction(const blink::WebInputEvent::Type type) override;

  // AppListModelObserver overrides:
  void OnSearchEngineIsGoogleChanged(bool is_google) override;

 private:
  // Unowned pointer to the associated profile.
  Profile* const profile_;

  // Unowned pointer to app list model.
  app_list::AppListModel* const model_;

  // Web view for the web contents managed by this class.
  const std::unique_ptr<views::WebView> web_view_;

  // Whether have received a server response for the current query string, and
  // the response contains an answer.
  bool received_answer_ = false;

  // Web contents managed by this class.
  const std::unique_ptr<content::WebContents> web_contents_;

  // If valid, URL of the answer server. Otherwise, search answers are disabled.
  GURL answer_server_url_;

  // URL of the current answer server request.
  GURL current_request_url_;

  // Time when the current server request started.
  base::TimeTicks server_request_start_time_;

  // Time when the current server response loaded.
  base::TimeTicks answer_loaded_time_;

  // When in the dark run mode, indicates whether we mimic that the server
  // response contains an answer.
  bool dark_run_received_answer_ = false;

  DISALLOW_COPY_AND_ASSIGN(SearchAnswerWebContentsDelegate);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_ANSWER_WEB_CONTENTS_DELEGATE_H_
