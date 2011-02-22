// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_INFO_MODEL_H_
#define CHROME_BROWSER_PAGE_INFO_MODEL_H_
#pragma once

#include <vector>

#include "base/string16.h"
#include "chrome/browser/cancelable_request.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "googleurl/src/gurl.h"
#include "ui/gfx/image.h"

class PrefService;
class Profile;

// The model that provides the information that should be displayed in the page
// info dialog/bubble.
class PageInfoModel {
 public:
  class PageInfoModelObserver {
   public:
    virtual ~PageInfoModelObserver() {}

    virtual void ModelChanged() = 0;
  };

  enum SectionInfoType {
    SECTION_INFO_IDENTITY = 0,
    SECTION_INFO_CONNECTION,
    SECTION_INFO_FIRST_VISIT,
  };

  // NOTE: ICON_STATE_OK ... ICON_STATE_ERROR must be listed in increasing
  // order of severity.  Code may depend on this order.
  enum SectionStateIcon {
    // No icon.
    ICON_NONE = -1,
    // State is OK.
    ICON_STATE_OK,
    // For example, if state is OK but contains mixed content.
    ICON_STATE_WARNING_MINOR,
    // For example, if content was served over HTTP.
    ICON_STATE_WARNING_MAJOR,
    // For example, unverified identity over HTTPS.
    ICON_STATE_ERROR,
    // An information icon.
    ICON_STATE_INFO
  };

  struct SectionInfo {
    SectionInfo(SectionStateIcon icon_id,
                const string16& headline,
                const string16& description,
                SectionInfoType type)
        : icon_id(icon_id),
          headline(headline),
          description(description),
          type(type) {
    }

    // The overall state of the connection (error, warning, ok).
    SectionStateIcon icon_id;

    // A single line describing the section, optional.
    string16 headline;

    // The full description of what this section is.
    string16 description;

    // The type of SectionInfo we are dealing with, for example: Identity,
    // Connection, First Visit.
    SectionInfoType type;
  };

  PageInfoModel(Profile* profile,
                const GURL& url,
                const NavigationEntry::SSLStatus& ssl,
                bool show_history,
                PageInfoModelObserver* observer);
  ~PageInfoModel();

  int GetSectionCount();
  SectionInfo GetSectionInfo(int index);

  // Returns the native image type for an icon with the given id.
  gfx::Image* GetIconImage(SectionStateIcon icon_id);

  // Callback from history service with number of visits to url.
  void OnGotVisitCountToHost(HistoryService::Handle handle,
                             bool found_visits,
                             int count,
                             base::Time first_visit);

 protected:
  // Testing constructor. DO NOT USE.
  PageInfoModel();

  // Shared initialization for default and testing constructor.
  void Init();

  PageInfoModelObserver* observer_;

  std::vector<SectionInfo> sections_;

  // All possible icons that go next to the text descriptions to indicate state.
  std::vector<gfx::Image*> icons_;

  // Used to request number of visits.
  CancelableRequestConsumer request_consumer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PageInfoModel);
};

#endif  // CHROME_BROWSER_PAGE_INFO_MODEL_H_
