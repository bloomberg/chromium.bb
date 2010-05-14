// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_INFO_MODEL_H_
#define CHROME_BROWSER_PAGE_INFO_MODEL_H_

#include <vector>

#include "base/string16.h"
#include "chrome/browser/cancelable_request.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "googleurl/src/gurl.h"

class PrefService;
class Profile;

// The model that provides the information that should be displayed in the page
// info dialog.
class PageInfoModel {
 public:
  class PageInfoModelObserver {
    public:
     virtual void ModelChanged() = 0;
  };

  struct SectionInfo {
    SectionInfo(bool state,
                const string16& title,
                const string16& head_line,
                const string16& description)
        : state(state),
          title(title),
          head_line(head_line),
          description(description) {
    }

    bool state;  // True if state is OK, false otherwise (ex of bad states:
                 // unverified identity over HTTPS).

    // The title of the section.
    string16 title;

    // A single line describing the section, optional.
    string16 head_line;

    // The full description of what this section is.
    string16 description;
  };

  PageInfoModel(Profile* profile,
                const GURL& url,
                const NavigationEntry::SSLStatus& ssl,
                bool show_history,
                PageInfoModelObserver* observer);

  int GetSectionCount();
  SectionInfo GetSectionInfo(int index);

  // Callback from history service with number of visits to url.
  void OnGotVisitCountToHost(HistoryService::Handle handle,
                             bool found_visits,
                             int count,
                             base::Time first_visit);

  static void RegisterPrefs(PrefService* prefs);

 protected:
  // Testing constructor. DO NOT USE.
  PageInfoModel() {}

  PageInfoModelObserver* observer_;

  std::vector<SectionInfo> sections_;

  // Used to request number of visits.
  CancelableRequestConsumer request_consumer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PageInfoModel);
};

#endif  // CHROME_BROWSER_PAGE_INFO_MODEL_H_
