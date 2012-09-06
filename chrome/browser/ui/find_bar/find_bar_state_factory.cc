// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/find_bar/find_bar_state_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/ui/find_bar/find_bar_state.h"

// static
FindBarState* FindBarStateFactory::GetForProfile(Profile* profile) {
  return static_cast<FindBarState*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
string16 FindBarStateFactory::GetLastPrepopulateText(Profile* p) {
  FindBarState* state = GetForProfile(p);
  string16 text = state->last_prepopulate_text();

  if (text.empty() && p->IsOffTheRecord()) {
    // Fall back to the original profile.
    state = GetForProfile(p->GetOriginalProfile());
    text = state->last_prepopulate_text();
  }

  return text;
}

// static
FindBarStateFactory* FindBarStateFactory::GetInstance() {
  return Singleton<FindBarStateFactory>::get();
}

FindBarStateFactory::FindBarStateFactory()
    : ProfileKeyedServiceFactory("FindBarState",
                                 ProfileDependencyManager::GetInstance()) {
}

FindBarStateFactory::~FindBarStateFactory() {}

ProfileKeyedService* FindBarStateFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new FindBarState;
}

bool FindBarStateFactory::ServiceHasOwnInstanceInIncognito() const {
  return true;
}
