// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PROFILE_ERROR_DIALOG_H_
#define CHROME_BROWSER_UI_PROFILE_ERROR_DIALOG_H_
#pragma once

// Shows an error dialog corresponding to the inability to open some portion of
// the profile. |message_id| is a string id corresponding to the message to
// show.
void ShowProfileErrorDialog(int message_id);

#endif  // CHROME_BROWSER_UI_PROFILE_ERROR_DIALOG_H_
