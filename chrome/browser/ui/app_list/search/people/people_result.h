// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_PEOPLE_PEOPLE_RESULT_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_PEOPLE_PEOPLE_RESULT_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "url/gurl.h"

class Profile;

namespace app_list {

struct Person;

class PeopleResult : public ChromeSearchResult {
 public:
  PeopleResult(Profile* profile, scoped_ptr<Person> person);
  virtual ~PeopleResult();

  // ChromeSearchResult overides:
  virtual void Open(int event_flags) OVERRIDE;
  virtual void InvokeAction(int action_index, int event_flags) OVERRIDE;
  virtual scoped_ptr<ChromeSearchResult> Duplicate() OVERRIDE;
  virtual ChromeSearchResultType GetType() OVERRIDE;

 private:
  void OnIconLoaded();
  void SetDefaultActions();
  void OpenChat();
  void SendEmail();

  bool IsChatAvailable();

  Profile* profile_;
  scoped_ptr<Person> person_;

  gfx::ImageSkia image_;
  base::WeakPtrFactory<PeopleResult> weak_factory_;

  // Caches whether or not the hangouts app is available for us to
  // send chat/video reqeusts to.
  bool is_chat_available_;

  DISALLOW_COPY_AND_ASSIGN(PeopleResult);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_PEOPLE_PEOPLE_RESULT_H_
