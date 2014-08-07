// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FONT_FAMILY_CACHE_H_
#define CHROME_BROWSER_FONT_FAMILY_CACHE_H_

#include "base/containers/hash_tables.h"
#include "base/gtest_prod_util.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/strings/string16.h"
#include "base/supports_user_data.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/common/web_preferences.h"

class PrefService;
class Profile;

FORWARD_DECLARE_TEST(FontFamilyCacheTest, Caching);

// Caches font family preferences associated with a PrefService. This class
// relies on the assumption that each concatenation of map_name + '.' + script
// is a unique string. It also relies on the assumption that the (const char*)
// keys used in both inner and outer hash_maps are compile time constants.
class FontFamilyCache : public base::SupportsUserData::Data,
                        public content::NotificationObserver {
 public:
  explicit FontFamilyCache(Profile* profile);
  virtual ~FontFamilyCache();

  // Gets or creates the relevant FontFamilyCache, and then fills |map|.
  static void FillFontFamilyMap(Profile* profile,
                                const char* map_name,
                                content::ScriptFontFamilyMap* map);

  // Fills |map| with font family preferences.
  void FillFontFamilyMap(const char* map_name,
                         content::ScriptFontFamilyMap* map);

 protected:
  // Exposed and virtual for testing.
  // Fetches the font without checking the cache.
  virtual base::string16 FetchFont(const char* script, const char* map_name);

 private:
  FRIEND_TEST_ALL_PREFIXES(::FontFamilyCacheTest, Caching);

  // Map from script to font.
  // Key comparison uses pointer equality.
  typedef base::hash_map<const char*, base::string16> ScriptFontMap;

  // Map from font family to ScriptFontMap.
  // Key comparison uses pointer equality.
  typedef base::hash_map<const char*, ScriptFontMap> FontFamilyMap;

  // Checks the cache for the font. If not present, fetches the font and stores
  // the result in the cache.
  // This method needs to be very fast, because it's called ~20,000 times on a
  // fresh launch with an empty profile. It's important to avoid unnecessary
  // object construction, hence the heavy use of const char* and the minimal use
  // of std::string.
  // |script| and |map_name| must be compile time constants. Two behaviors rely
  // on this: key comparison uses pointer equality, and keys must outlive the
  // hash_maps.
  base::string16 FetchAndCacheFont(const char* script, const char* map_name);

  // Called when font family preferences changed.
  // Invalidates the cached entry, and removes the relevant observer.
  // Note: It is safe to remove the observer from the pref change callback.
  void OnPrefsChanged(const std::string& pref_name);

  // content::NotificationObserver override.
  // Called when the profile is being destructed.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Cache of font family preferences.
  FontFamilyMap font_family_map_;

  // Weak reference.
  // Note: The lifetime of this object is tied to the lifetime of the
  // PrefService, so there is no worry about an invalid pointer.
  const PrefService* prefs_;

  // Reacts to profile changes.
  PrefChangeRegistrar profile_pref_registrar_;

  // Listens for profile destruction.
  content::NotificationRegistrar notification_registrar_;

  DISALLOW_COPY_AND_ASSIGN(FontFamilyCache);
};

#endif  // CHROME_BROWSER_FONT_FAMILY_CACHE_H_
