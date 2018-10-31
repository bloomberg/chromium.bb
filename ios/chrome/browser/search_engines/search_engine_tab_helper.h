// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEARCH_ENGINES_SEARCH_ENGINE_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_SEARCH_ENGINES_SEARCH_ENGINE_TAB_HELPER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#import "ios/web/public/web_state/web_state_observer.h"
#import "ios/web/public/web_state/web_state_user_data.h"

namespace web {
class WebState;
}  // namespace web

// Creates TemplateURLs from attached WebState and adds them to
// TemplateURLService. To create a TemplateURL, 3 basic elements are needed:
//   1. A short name (e.g. "Google");
//   2. A keyword (e.g. "google.com");
//   3. A searchable URL (e.g. "https://google.com?name=a&q={searchTerms}").
//
// Both short name and keyword can be generated from navigation history. For
// searchable URL, there are 2 methods to create it:
//   1. If a OSDD(Open Search Description Document) <link> is found in page,
//      use TemplateURLFetcher to download the XML file, parse it and get the
//      searchable URL;
//   2. If a <form> is submitted in page, a searchable URL can be generated
//      by analysing the <form>'s elements and concatenating "name" and
//      "value" attributes of them.
//
// Both these 2 methods depends on injected JavaScript.
//
class SearchEngineTabHelper
    : public web::WebStateObserver,
      public web::WebStateUserData<SearchEngineTabHelper> {
 public:
  ~SearchEngineTabHelper() override;

 private:
  friend class web::WebStateUserData<SearchEngineTabHelper>;

  explicit SearchEngineTabHelper(web::WebState* web_state);

  // Creates a TemplateURL by downloading and parsing the OSDD
  void AddTemplateURLByOSDD(const GURL& page_url, const GURL& osdd_url);

  // WebStateObserver implementation.
  void WebStateDestroyed(web::WebState* web_state) override;

  // Handles messages from JavaScript. Messages can be:
  //   1. A OSDD <link> is found;
  //   2. A searchable URL is generated from <form> submission.
  bool OnJsMessage(const base::DictionaryValue& message,
                   const GURL& page_url,
                   bool has_user_gesture,
                   bool form_in_main_frame,
                   web::WebFrame* sender_frame);

  // WebState this tab helper is attached to.
  web::WebState* web_state_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(SearchEngineTabHelper);
};

#endif  // IOS_CHROME_BROWSER_SEARCH_ENGINES_SEARCH_ENGINE_TAB_HELPER_H_
