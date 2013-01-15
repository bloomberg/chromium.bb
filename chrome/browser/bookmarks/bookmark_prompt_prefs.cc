// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_prompt_prefs.h"

#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/common/pref_names.h"

BookmarkPromptPrefs::BookmarkPromptPrefs(PrefServiceBase* user_prefs)
    : prefs_(user_prefs) {
}

BookmarkPromptPrefs::~BookmarkPromptPrefs() {
}

void BookmarkPromptPrefs::DisableBookmarkPrompt() {
  prefs_->SetBoolean(prefs::kBookmarkPromptEnabled, false);
}

int BookmarkPromptPrefs::GetPromptImpressionCount() const {
  return prefs_->GetInteger(prefs::kBookmarkPromptImpressionCount);
}

void BookmarkPromptPrefs::IncrementPromptImpressionCount() {
  prefs_->SetInteger(prefs::kBookmarkPromptImpressionCount,
                     GetPromptImpressionCount() + 1);
}

bool BookmarkPromptPrefs::IsBookmarkPromptEnabled() const {
  return prefs_->GetBoolean(prefs::kBookmarkPromptEnabled);
}

// static
void BookmarkPromptPrefs::RegisterUserPrefs(PrefServiceSyncable* user_prefs) {
  // We always register preferences without checking FieldTrial, because
  // we may not receive field trial list from the server yet.
  user_prefs->RegisterBooleanPref(prefs::kBookmarkPromptEnabled, true,
                                  PrefServiceSyncable::UNSYNCABLE_PREF);
  user_prefs->RegisterIntegerPref(prefs::kBookmarkPromptImpressionCount, 0,
                                  PrefServiceSyncable::UNSYNCABLE_PREF);
}
