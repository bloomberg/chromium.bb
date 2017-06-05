// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/md_bookmarks/bookmarks_message_handler.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"

BookmarksMessageHandler::BookmarksMessageHandler() {}

BookmarksMessageHandler::~BookmarksMessageHandler() {}

void BookmarksMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getIncognitoAvailability",
      base::Bind(&BookmarksMessageHandler::HandleGetIncognitoAvailability,
                 base::Unretained(this)));
}

void BookmarksMessageHandler::OnJavascriptAllowed() {
  PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
  pref_change_registrar_.Init(prefs);
  pref_change_registrar_.Add(
      prefs::kIncognitoModeAvailability,
      base::Bind(&BookmarksMessageHandler::UpdateIncognitoAvailability,
                 base::Unretained(this)));
}

void BookmarksMessageHandler::OnJavascriptDisallowed() {
  pref_change_registrar_.RemoveAll();
}

void BookmarksMessageHandler::UpdateIncognitoAvailability() {
  FireWebUIListener("incognito-availability-changed",
                    base::Value(GetIncognitoAvailability()));
}

int BookmarksMessageHandler::GetIncognitoAvailability() {
  PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
  return prefs->GetInteger(prefs::kIncognitoModeAvailability);
}

void BookmarksMessageHandler::HandleGetIncognitoAvailability(
    const base::ListValue* args) {
  CHECK_EQ(1U, args->GetSize());
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));

  AllowJavascript();

  ResolveJavascriptCallback(*callback_id,
                            base::Value(GetIncognitoAvailability()));
}
