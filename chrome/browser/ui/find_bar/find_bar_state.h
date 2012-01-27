// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Stores per-profile state needed for find in page.  This includes the most
// recently searched for term.

#ifndef CHROME_BROWSER_UI_FIND_BAR_FIND_BAR_STATE_H_
#define CHROME_BROWSER_UI_FIND_BAR_FIND_BAR_STATE_H_
#pragma once

#include "base/basictypes.h"
#include "base/string16.h"
#include "chrome/browser/profiles/profile_keyed_service.h"

class Profile;

class FindBarState : public ProfileKeyedService {
 public:
  FindBarState() {}
  virtual ~FindBarState() {}

  string16 last_prepopulate_text() const {
    return last_prepopulate_text_;
  }

  void set_last_prepopulate_text(const string16& text) {
    last_prepopulate_text_ = text;
  }

 private:
  string16 last_prepopulate_text_;

  DISALLOW_COPY_AND_ASSIGN(FindBarState);
};

#endif  // CHROME_BROWSER_UI_FIND_BAR_FIND_BAR_STATE_H_
