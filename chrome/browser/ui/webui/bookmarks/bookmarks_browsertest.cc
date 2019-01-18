// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/bookmarks/bookmarks_browsertest.h"

#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/prefs/pref_service.h"

BookmarksBrowserTest::BookmarksBrowserTest() {}

BookmarksBrowserTest::~BookmarksBrowserTest() {}

void BookmarksBrowserTest::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "testSetIncognito",
      base::BindRepeating(&BookmarksBrowserTest::HandleSetIncognitoAvailability,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "testSetCanEdit",
      base::BindRepeating(&BookmarksBrowserTest::HandleSetCanEditBookmarks,
                          base::Unretained(this)));
}

void BookmarksBrowserTest::SetIncognitoAvailability(int availability) {
  ASSERT_TRUE(availability >= 0 &&
              availability < IncognitoModePrefs::AVAILABILITY_NUM_TYPES);
  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kIncognitoModeAvailability, availability);
}

void BookmarksBrowserTest::SetCanEditBookmarks(bool canEdit) {
  browser()->profile()->GetPrefs()->SetBoolean(
      bookmarks::prefs::kEditBookmarksEnabled, canEdit);
}

void BookmarksBrowserTest::HandleSetIncognitoAvailability(
    const base::ListValue* args) {
  AllowJavascript();

  ASSERT_EQ(2U, args->GetSize());
  const base::Value* callback_id;
  ASSERT_TRUE(args->Get(0, &callback_id));
  int pref_value;
  ASSERT_TRUE(args->GetInteger(1, &pref_value));

  SetIncognitoAvailability(pref_value);

  ResolveJavascriptCallback(*callback_id, base::Value());
}

void BookmarksBrowserTest::HandleSetCanEditBookmarks(
    const base::ListValue* args) {
  AllowJavascript();

  ASSERT_EQ(2U, args->GetSize());
  const base::Value* callback_id;
  ASSERT_TRUE(args->Get(0, &callback_id));
  bool pref_value;
  ASSERT_TRUE(args->GetBoolean(1, &pref_value));

  SetCanEditBookmarks(pref_value);

  ResolveJavascriptCallback(*callback_id, base::Value());
}

content::WebUIMessageHandler* BookmarksBrowserTest::GetMockMessageHandler() {
  return this;
}
