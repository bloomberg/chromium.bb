// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/declarative_user_script_manager.h"

#include "chrome/browser/extensions/declarative_user_script_master.h"

namespace extensions {

DeclarativeUserScriptManager::DeclarativeUserScriptManager(Profile* profile)
    : profile_(profile) {
}

DeclarativeUserScriptManager::~DeclarativeUserScriptManager() {
}

DeclarativeUserScriptMaster*
DeclarativeUserScriptManager::GetDeclarativeUserScriptMasterByID(
    const std::string& id) {
  UserScriptMasterMap::iterator it = declarative_user_script_masters_.find(id);

  if (it != declarative_user_script_masters_.end())
    return it->second.get();

  linked_ptr<DeclarativeUserScriptMaster> master(
      new DeclarativeUserScriptMaster(profile_, id));
  declarative_user_script_masters_[id] = master;
  return master.get();
}

}  // extensions
