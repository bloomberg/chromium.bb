// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PROFILE_ERROR_DIALOG_H_
#define CHROME_BROWSER_UI_PROFILE_ERROR_DIALOG_H_

// Be very careful while modifying this enum. Do NOT remove any elements from
// this enum. If you need to add one, add them to the end, right before
// PROFILE_ERROR_END. PROFILE_ERROR_END should ALWAYS be the last element in
// this enum. This is important because this enum is used to back a histogram,
// and these are implicit assumptions made in terms of how enumerated
// histograms are defined.
enum ProfileErrorType {
  PROFILE_ERROR_HISTORY,
  PROFILE_ERROR_PREFERENCES,
  PROFILE_ERROR_DB_AUTOFILL_WEB_DATA,
  PROFILE_ERROR_DB_TOKEN_WEB_DATA,
  PROFILE_ERROR_DB_WEB_DATA,
  PROFILE_ERROR_DB_KEYWORD_WEB_DATA,
  PROFILE_ERROR_END
};

// Shows an error dialog corresponding to the inability to open some portion of
// the profile. |message_id| is a string id corresponding to the message to
// show. The ProfileErrorType needs to correspond to one of the profile error
// types in the enum above. If your use case doesn't fit any of the ones listed
// above, please add your type to the enum and modify the enum
// definition in tools/metrics/histograms/histograms.xml accordingly.
void ShowProfileErrorDialog(ProfileErrorType type, int message_id);

#endif  // CHROME_BROWSER_UI_PROFILE_ERROR_DIALOG_H_
