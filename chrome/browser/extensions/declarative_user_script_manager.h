// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_DECLARATIVE_USER_SCRIPT_MANAGER_H_
#define CHROME_BROWSER_EXTENSIONS_DECLARATIVE_USER_SCRIPT_MANAGER_H_

#include <map>

#include "base/macros.h"
#include "base/memory/linked_ptr.h"

class Profile;

namespace extensions {
class DeclarativeUserScriptMaster;

// Manages a set of DeclarativeUserScriptMaster objects for script injections.
class DeclarativeUserScriptManager {
 public:
  explicit DeclarativeUserScriptManager(Profile* profile);
  ~DeclarativeUserScriptManager();

  // Get the user script master for declarative scripts; if one does not exist,
  // a new object will be created.
  DeclarativeUserScriptMaster* GetDeclarativeUserScriptMasterByID(
      const std::string& id);

 private:
  using UserScriptMasterMap =
      std::map<std::string, linked_ptr<DeclarativeUserScriptMaster>>;

  // A map of DeclarativeUserScriptMasters for ids; each master is lazily
  // initialized.
  UserScriptMasterMap declarative_user_script_masters_;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(DeclarativeUserScriptManager);
};

}  // extensions

#endif  // CHROME_BROWSER_EXTENSIONS_DECLARATIVE_USER_SCRIPT_MANAGER_H_
