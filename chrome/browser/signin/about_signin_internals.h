// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_ABOUT_SIGNIN_INTERNALS_H_
#define CHROME_BROWSER_SIGNIN_ABOUT_SIGNIN_INTERNALS_H_

#include <map>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/signin/signin_internals_util.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/chrome_version_info.h"

class Profile;

// This class collects authentication, signin and token information
// to propagate to about:signin-internals via SigninInternalsUI.
class AboutSigninInternals
    : public ProfileKeyedService,
      public signin_internals_util::SigninDiagnosticsObserver {
 public:
  class Observer {
   public:
    // |info| will contain the dictionary of signin_status_ values as indicated
    // in the comments for GetSigninStatus() below.
    virtual void OnSigninStateChanged(scoped_ptr<DictionaryValue> info) = 0;
  };

  AboutSigninInternals();
  virtual ~AboutSigninInternals();

  // Each instance of SigninInternalsUI adds itself as an observer to be
  // notified of all updates that AboutSigninInternals receives.
  void AddSigninObserver(Observer* observer);
  void RemoveSigninObserver(Observer* observer);

  // Pulls all signin values that have been persisted in the user prefs.
  void RefreshSigninPrefs();

  // SigninManager::SigninDiagnosticsObserver implementation.
  virtual void NotifySigninValueChanged(
      const signin_internals_util::UntimedSigninStatusField& field,
      const std::string& value) OVERRIDE;

  virtual void NotifySigninValueChanged(
      const signin_internals_util::TimedSigninStatusField& field,
      const std::string& value) OVERRIDE;

  virtual void NotifyTokenReceivedSuccess(const std::string& token_name,
                                          const std::string& token,
                                          bool update_time) OVERRIDE;

  virtual void NotifyTokenReceivedFailure(const std::string& token_name,
                                          const std::string& error) OVERRIDE;

  virtual void NotifyClearStoredToken(const std::string& token_name) OVERRIDE;

  void Initialize(Profile* profile);

  // ProfileKeyedService implementation.
  virtual void Shutdown() OVERRIDE;

  // Returns a dictionary of values in signin_status_ for use in
  // about:signin-internals. The values are formatted as shown -
  //
  // { "signin_info" :
  //     [ {"title": "Basic Information",
  //        "data": [List of {"label" : "foo-field", "value" : "foo"} elems]
  //       },
  //       { "title": "Detailed Information",
  //        "data": [List of {"label" : "foo-field", "value" : "foo"} elems]
  //       }],
  //   "token_info" :
  //     [ List of {"name": "foo-name", "token" : "foo-token",
  //                 "status": "foo_stat", "time" : "foo_time"} elems]
  //  }
  scoped_ptr<DictionaryValue> GetSigninStatus();

 private:
  void NotifyObservers();

  // Weak pointer to parent profile.
  Profile* profile_;

  // Encapsulates the actual signin and token related values.
  // Most of the values are mirrored in the prefs for persistance.
  signin_internals_util::SigninStatus signin_status_;

  ObserverList<Observer> signin_observers_;

  DISALLOW_COPY_AND_ASSIGN(AboutSigninInternals);
};

#endif  // CHROME_BROWSER_SIGNIN_ABOUT_SIGNIN_INTERNALS_H_
