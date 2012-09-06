// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_MODEL_FACTORY_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_MODEL_FACTORY_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

template <typename T> struct DefaultSingletonTraits;

class Profile;
class BookmarkModel;

// Singleton that owns all BookmarkModel and associates them with
// Profiles.
class BookmarkModelFactory : public ProfileKeyedServiceFactory {
 public:
  static BookmarkModel* GetForProfile(Profile* profile);

  static BookmarkModel* GetForProfileIfExists(Profile* profile);

  static BookmarkModelFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<BookmarkModelFactory>;

  BookmarkModelFactory();
  virtual ~BookmarkModelFactory();

  // ProfileKeyedServiceFactory:
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;
  virtual void RegisterUserPrefs(PrefService* user_prefs) OVERRIDE;
  virtual bool ServiceRedirectedInIncognito() const OVERRIDE;
  virtual bool ServiceIsNULLWhileTesting() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(BookmarkModelFactory);
};

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_MODEL_FACTORY_H_
