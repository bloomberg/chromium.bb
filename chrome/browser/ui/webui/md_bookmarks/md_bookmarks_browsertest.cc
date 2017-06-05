// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/md_bookmarks/md_bookmarks_browsertest.h"

#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

MdBookmarksBrowserTest::MdBookmarksBrowserTest() {}

MdBookmarksBrowserTest::~MdBookmarksBrowserTest() {}

void MdBookmarksBrowserTest::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "testSetIncognito",
      base::Bind(&MdBookmarksBrowserTest::HandleSetIncognitoAvailability,
                 base::Unretained(this)));
}

void MdBookmarksBrowserTest::SetIncognitoAvailability(int availability) {
  ASSERT_TRUE(availability >= 0 &&
              availability < IncognitoModePrefs::AVAILABILITY_NUM_TYPES);
  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kIncognitoModeAvailability, availability);
}

void MdBookmarksBrowserTest::HandleSetIncognitoAvailability(
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

content::WebUIMessageHandler* MdBookmarksBrowserTest::GetMockMessageHandler() {
  return this;
}
