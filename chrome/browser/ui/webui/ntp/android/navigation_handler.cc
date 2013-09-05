// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/android/navigation_handler.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/values.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/page_transition_types.h"

NavigationHandler::NavigationHandler() {}

NavigationHandler::~NavigationHandler() {
  // Record an omnibox-based navigation, if there is one.
  content::NavigationEntry* entry =
      web_ui()->GetWebContents()->GetController().GetActiveEntry();
  if (!entry)
    return;

  if (!(entry->GetTransitionType() &
        content::PAGE_TRANSITION_FROM_ADDRESS_BAR) ||
      entry->GetTransitionType() & content::PAGE_TRANSITION_FORWARD_BACK) {
    return;
  }

  if (entry->GetURL().SchemeIs(chrome::kChromeUIScheme) &&
      entry->GetURL().host() == chrome::kChromeUINewTabHost) {
    return;
  }

  RecordActionForNavigation(*entry);
}

void NavigationHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "openedMostVisited",
      base::Bind(&NavigationHandler::HandleOpenedMostVisited,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "openedRecentlyClosed",
      base::Bind(&NavigationHandler::HandleOpenedRecentlyClosed,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "openedBookmark",
      base::Bind(&NavigationHandler::HandleOpenedBookmark,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "openedForeignSession",
      base::Bind(&NavigationHandler::HandleOpenedForeignSession,
                 base::Unretained(this)));
}

void NavigationHandler::HandleOpenedMostVisited(const base::ListValue* args) {
  RecordAction(ACTION_OPENED_MOST_VISITED_ENTRY);
}

void NavigationHandler::HandleOpenedRecentlyClosed(
    const base::ListValue* args) {
  RecordAction(ACTION_OPENED_RECENTLY_CLOSED_ENTRY);
}

void NavigationHandler::HandleOpenedBookmark(const base::ListValue* args) {
  RecordAction(ACTION_OPENED_BOOKMARK);
}

void NavigationHandler::HandleOpenedForeignSession(
    const base::ListValue* args) {
  RecordAction(ACTION_OPENED_FOREIGN_SESSION);
}

// static
void NavigationHandler::RecordActionForNavigation(
    const content::NavigationEntry& entry) {
  Action action;
  if ((entry.GetTransitionType() & content::PAGE_TRANSITION_CORE_MASK) ==
      content::PAGE_TRANSITION_GENERATED) {
    action = ACTION_SEARCHED_USING_OMNIBOX;
  } else if (google_util::IsGoogleHomePageUrl(entry.GetURL())) {
    action = ACTION_NAVIGATED_TO_GOOGLE_HOMEPAGE;
  } else {
    action = ACTION_NAVIGATED_USING_OMNIBOX;
  }
  RecordAction(action);
}

// static
void NavigationHandler::RecordAction(Action action) {
  UMA_HISTOGRAM_ENUMERATION("NewTabPage.ActionAndroid", action, NUM_ACTIONS);
}
