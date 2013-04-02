// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_ERROR_GLOBAL_ERROR_SERVICE_H_
#define CHROME_BROWSER_UI_GLOBAL_ERROR_GLOBAL_ERROR_SERVICE_H_

#include <vector>

#include "base/basictypes.h"
#include "chrome/browser/profiles/profile_keyed_service.h"

class GlobalError;
class Profile;

// This service manages a list of errors that are meant to be shown globally.
// If an error applies to an entire profile and not just to a tab then the
// error should be shown using this service. Examples of global errors are:
//   - the previous session crashed for a given profile.
//   - a sync error occurred
class GlobalErrorService : public ProfileKeyedService {
 public:
  // Type used to represent the list of currently active errors.
  typedef std::vector<GlobalError*> GlobalErrorList;

  // Constructs a GlobalErrorService object for the given profile. The profile
  // maybe NULL for tests.
  explicit GlobalErrorService(Profile* profile);
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

  // Gets the highest severity error that has a wrench menu item.
  // Returns NULL if no such error exists.
  GlobalError* GetHighestSeverityGlobalErrorWithWrenchMenuItem() const;

  // Gets the first error that has a bubble view which hasn't been shown yet.
  // Returns NULL if no such error exists.
  GlobalError* GetFirstGlobalErrorWithBubbleView() const;

  // Gets all errors.
  const GlobalErrorList& errors() { return errors_; }

  // Post a notification that a global error has changed and that the error UI
  // should update it self. Pass NULL for the given error to mean all error UIs
  // should update.
  void NotifyErrorsChanged(GlobalError* error);

 private:
  GlobalErrorList errors_;
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(GlobalErrorService);
};

#endif  // CHROME_BROWSER_UI_GLOBAL_ERROR_GLOBAL_ERROR_SERVICE_H_
