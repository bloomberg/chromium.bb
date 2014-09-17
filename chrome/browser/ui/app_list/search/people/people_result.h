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

  // Check if we have any variant of the hangouts extension installed and
  // waiting on the onHangoutRequested event (the hangouts extension can have
  // a total of four possible id's, depending on which release type of it is
  // installed). If so, set the hangouts_extension_id_ to the id of the
  // extension that is waiting, or set it to an empty string if not.
  void RefreshHangoutsExtensionId();

  Profile* profile_;
  scoped_ptr<Person> person_;

  gfx::ImageSkia image_;

  // Caches the id of the hangouts extension.
  std::string hangouts_extension_id_;

  base::WeakPtrFactory<PeopleResult> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PeopleResult);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_PEOPLE_PEOPLE_RESULT_H_
