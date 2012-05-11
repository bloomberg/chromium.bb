// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_APPS_PROMO_H_
#define CHROME_BROWSER_EXTENSIONS_APPS_PROMO_H_
#pragma once

#include <set>
#include <string>

#include "base/gtest_prod_util.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/common/url_fetcher_delegate.h"

class PrefService;
class Profile;

// This encapsulates business logic for:
// - Whether to show the apps promo in the launcher
// - Whether to expire existing default apps
class AppsPromo {
 public:
  // Groups users by whether they have seen a web store promo before. This is
  // used for deciding to maximize the promo and apps section on the NTP.
  enum UserGroup {
    // Matches no users.
    USERS_NONE = 0,

    // Users who have not seen a promo (last promo id is default value).
    USERS_NEW = 1,

    // Users who have already seen a promo (last promo id is non-default).
    USERS_EXISTING = 1 << 1,
  };

  // Holds all the data that specifies a promo for the apps section of the NTP.
  struct PromoData {
    PromoData();
    PromoData(const std::string& id,
              const std::string& header,
              const std::string& button,
              const GURL& link,
              const std::string& expire,
              const GURL& logo,
              int user_group);
    ~PromoData();

    // See PromoResourceService::UnpackWebStoreSignal for descriptions of these
    // fields.
    std::string id;
    std::string header;
    std::string button;
    GURL link;
    std::string expire;
    GURL logo;
    int user_group;
  };

  // Register our preferences. Parts of the promo content are stored in Local
  // State since they're independent of the user profile.
  static void RegisterPrefs(PrefService* local_state);
  static void RegisterUserPrefs(PrefService* prefs);

  // Returns true if a promo is available for the current locale.
  static bool IsPromoSupportedForLocale();

  // Returns true if the web store is active for the existing locale.
  static bool IsWebStoreSupportedForLocale();

  // Sets whether the web store and apps section is supported for the current
  // locale.
  static void SetWebStoreSupportedForLocale(bool supported);

  // Accesses the current promo data. The default logo will be used if
  // |promo_data.logo| is empty or not a valid 'data' URL.
  static void ClearPromo();
  static PromoData GetPromo();
  static void SetPromo(const PromoData& promo_data);

  // Gets the original URL of the logo. This should only be set when the logo
  // was served over HTTPS.
  static GURL GetSourcePromoLogoURL();
  static void SetSourcePromoLogoURL(const GURL& original_url);

  explicit AppsPromo(PrefService* prefs);
  ~AppsPromo();

  // Gets the set of old default apps that may have been installed by previous
  // versions of Chrome.
  const ExtensionIdSet& old_default_apps() const {
    return old_default_app_ids_;
  }

  // Halts the special treatment of the default apps. The default apps may be
  // removed by the caller after calling this method. If the apps remain
  // installed, AppsPromo will no longer consider the apps "default".
  void ExpireDefaultApps();

  // Called to hide the promo from the apps section.
  void HidePromo();

  // Returns true if the app launcher should be displayed on the NTP.
  bool ShouldShowAppLauncher(const ExtensionIdSet& installed_ids);

  // Returns true if the apps promo should be displayed in the launcher.
  bool ShouldShowPromo(const ExtensionIdSet& installed_ids,
                       bool* just_expired);

 private:
  FRIEND_TEST_ALL_PREFIXES(ExtensionAppsPromo, HappyPath);
  FRIEND_TEST_ALL_PREFIXES(ExtensionAppsPromo, PromoPrefs);
  FRIEND_TEST_ALL_PREFIXES(ExtensionAppsPromo, UpdatePromoFocus);

  // The maximum number of times to show the apps promo. The promo counter
  // actually goes up to this number + 1 because we need to differentiate
  // between the first time we overflow and subsequent times.
  static const int kDefaultAppsCounterMax;

  bool GetDefaultAppsInstalled() const;

  // Gets the UserGroup classification of the current user.
  UserGroup GetCurrentUserGroup() const;

  // Gets/sets the ID of the last promo shown.
  std::string GetLastPromoId();
  void SetLastPromoId(const std::string& id);

  // Gets/sets the number of times the promo has been viewed. Promo views are
  // only counted when the default apps are installed.
  int GetPromoCounter() const;
  void SetPromoCounter(int val);

  // Our permanent state is stored in this PrefService instance.
  PrefService* prefs_;

  // The set of default extensions. Initialized to a static list in the
  // constructor.
  ExtensionIdSet old_default_app_ids_;

  DISALLOW_COPY_AND_ASSIGN(AppsPromo);
};

// Fetches logos over HTTPS, making sure we don't send cookies and that we
// cache the image until its source URL changes.
class AppsPromoLogoFetcher : public content::URLFetcherDelegate {
 public:
  AppsPromoLogoFetcher(Profile* profile,
                       const AppsPromo::PromoData& promo_data);
  virtual ~AppsPromoLogoFetcher();

  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

 private:
  // Fetches the logo and stores the result as a data URL.
  void FetchLogo();

  // Checks if the logo was downloaded previously.
  bool HaveCachedLogo();

  // Sets the apps promo based on the current data and then issues the
  // WEB_STORE_PROMO_LOADED notification so open NTPs can inject the promo.
  void SavePromo();

  // Checks if the promo logo matches https://*.google.com/*.png.
  bool SupportsLogoURL();

  Profile* profile_;
  AppsPromo::PromoData promo_data_;
  scoped_ptr<content::URLFetcher> url_fetcher_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_APPS_PROMO_H_
