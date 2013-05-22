// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_WEBSTORE_RESULT_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_WEBSTORE_RESULT_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "googleurl/src/gurl.h"

class Profile;

namespace app_list {

class WebstoreResult : public ChromeSearchResult {
 public:
  WebstoreResult(Profile* profile,
                 const std::string& app_id,
                 const std::string& localized_name,
                 const GURL& icon_url);
  virtual ~WebstoreResult();

  // ChromeSearchResult overides:
  virtual void Open(int event_flags) OVERRIDE;
  virtual void InvokeAction(int action_index, int event_flags) OVERRIDE;
  virtual scoped_ptr<ChromeSearchResult> Duplicate() OVERRIDE;

 private:
  void SetDefaultDetails();
  void OnIconLoaded();

  Profile* profile_;
  const std::string app_id_;
  const std::string localized_name_;
  const GURL icon_url_;

  gfx::ImageSkia icon_;
  base::WeakPtrFactory<WebstoreResult> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebstoreResult);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_WEBSTORE_RESULT_H_
