// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_ERROR_SERVICE_H_
#define CHROME_BROWSER_UI_GLOBAL_ERROR_SERVICE_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "chrome/browser/profiles/profile_keyed_service.h"

class GlobalError;

// This service manages a list of errors that are meant to be shown globally.
// If an error applies to an entire profile and not just to a tab then the
// error should be shown using this service. Examples of global errors are:
//   - the previous session crashed for a given profile.
//   - a sync error occurred
class GlobalErrorService : public ProfileKeyedService {
 public:
  GlobalErrorService();
  virtual ~GlobalErrorService();

  // Adds the given error to the list of global errors and displays it on
  // browswer windows. If no browser windows are open  then the error is
  // displayed once a browser window is opened. This class takes ownership of
  // the error.
  void AddGlobalError(GlobalError* error);

  // Hides the given error and removes it from the list of global errors. Caller
  // then takes ownership of the error and is responsible for deleting it.
  void RemoveGlobalError(GlobalError* error);

  // Gets the error with the given command ID or NULL if no such error exists.
  // This class maintains ownership of the returned error.
  GlobalError* GetGlobalErrorByMenuItemCommandID(int command_id) const;

  // Gets the badge icon resource ID for the first error with a badge.
  // Returns 0 if no such error exists.
  int GetFirstBadgeResourceID();

  // Gets all errors.
  const std::vector<GlobalError*>& errors() { return errors_; }

 private:
  std::vector<GlobalError*> errors_;

  DISALLOW_COPY_AND_ASSIGN(GlobalErrorService);
};

#endif  // CHROME_BROWSER_UI_GLOBAL_ERROR_SERVICE_H_
