// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_PREFS_USER_PREFS_H_
#define COMPONENTS_USER_PREFS_USER_PREFS_H_

#include "base/basictypes.h"
#include "base/supports_user_data.h"
#include "components/user_prefs/user_prefs_export.h"

class PrefService;

namespace content {
class BrowserContext;
}

namespace user_prefs {

// Components may use preferences associated with a given user. These
// hang off of content::BrowserContext and can be retrieved using
// UserPrefs::Get().
//
// It is up to the embedder to create and own the PrefService and
// attach it to BrowserContext using the UserPrefs::Set() function.
class USER_PREFS_EXPORT UserPrefs : public base::SupportsUserData::Data {
 public:
  // Retrieves the PrefService for a given BrowserContext, or NULL if
  // none is attached.
  static PrefService* Get(content::BrowserContext* context);

  // Hangs the specified |prefs| off of |context|. Should be called
  // only once per BrowserContext.
  static void Set(content::BrowserContext* context, PrefService* prefs);

 private:
  explicit UserPrefs(PrefService* prefs);
  virtual ~UserPrefs();

  // Non-owning; owned by embedder.
  PrefService* prefs_;

  DISALLOW_COPY_AND_ASSIGN(UserPrefs);
};

}  // namespace user_prefs

#endif  // COMPONENTS_USER_PREFS_USER_PREFS_H_
