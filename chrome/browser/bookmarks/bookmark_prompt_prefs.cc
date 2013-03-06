// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_prompt_prefs.h"

#include "base/prefs/pref_service.h"
#include "chrome/common/pref_names.h"
#include "components/user_prefs/pref_registry_syncable.h"

BookmarkPromptPrefs::BookmarkPromptPrefs(PrefService* user_prefs)
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
void BookmarkPromptPrefs::RegisterUserPrefs(PrefRegistrySyncable* registry) {
  // We always register preferences without checking FieldTrial, because
  // we may not receive field trial list from the server yet.
  registry->RegisterBooleanPref(prefs::kBookmarkPromptEnabled, true,
                                PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterIntegerPref(prefs::kBookmarkPromptImpressionCount, 0,
                                PrefRegistrySyncable::UNSYNCABLE_PREF);
}
