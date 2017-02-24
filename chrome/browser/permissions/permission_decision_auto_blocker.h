// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PERMISSION_DECISION_AUTO_BLOCKER_H_
#define CHROME_BROWSER_PERMISSIONS_PERMISSION_DECISION_AUTO_BLOCKER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/time/default_clock.h"
#include "chrome/browser/permissions/permission_result.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

class GURL;
class Profile;

namespace content {
class WebContents;
}

namespace safe_browsing {
class SafeBrowsingDatabaseManager;
}

// The PermissionDecisionAutoBlocker decides whether or not a given origin
// should be automatically blocked from requesting a permission. When an origin
// is blocked, it is placed under an "embargo". Until the embargo expires, any
// requests made by the origin are automatically blocked. Once the embargo is
// lifted, the origin will be permitted to request a permission again, which may
// result in it being placed under embargo again. Currently, an origin can be
// placed under embargo if it appears on Safe Browsing's API blacklist, or if it
// has a number of prior dismissals greater than a threshold.
class PermissionDecisionAutoBlocker : public KeyedService {
 public:
  class Factory : public BrowserContextKeyedServiceFactory {
   public:
    static PermissionDecisionAutoBlocker* GetForProfile(Profile* profile);
    static PermissionDecisionAutoBlocker::Factory* GetInstance();

   private:
    friend struct base::DefaultSingletonTraits<Factory>;

    Factory();
    ~Factory() override;

    // BrowserContextKeyedServiceFactory
    KeyedService* BuildServiceInstanceFor(
        content::BrowserContext* context) const override;

    content::BrowserContext* GetBrowserContextToUse(
        content::BrowserContext* context) const override;
  };

  static PermissionDecisionAutoBlocker* GetForProfile(Profile* profile);

  // Updates the threshold to start blocking prompts from the field trial.
  static void UpdateFromVariations();

  // Makes an asynchronous call to Safe Browsing to check the API blacklist.
  // Places the (|request_origin|, |permission|) pair under embargo if they are
  // on the blacklist.
  void CheckSafeBrowsingBlacklist(content::WebContents* web_contents,
                                  const GURL& request_origin,
                                  ContentSettingsType permission,
                                  base::Callback<void(bool)> callback);

  // Checks the status of the content setting to determine if |request_origin|
  // is under embargo for |permission|. This checks both embargo for Permissions
  // Blacklisting and repeated dismissals.
  PermissionResult GetEmbargoResult(const GURL& request_origin,
                                    ContentSettingsType permission);

  // Returns the current number of dismisses recorded for |permission| type at
  // |url|.
  int GetDismissCount(const GURL& url, ContentSettingsType permission);

  // Returns the current number of ignores recorded for |permission|
  // type at |url|.
  int GetIgnoreCount(const GURL& url, ContentSettingsType permission);

  // Records that a dismissal of a prompt for |permission| was made. If the
  // total number of dismissals exceeds a threshhold and
  // features::kBlockPromptsIfDismissedOften is enabled it will place |url|
  // under embargo for |permission|.
  bool RecordDismissAndEmbargo(const GURL& url, ContentSettingsType permission);

  // Records that an ignore of a prompt for |permission| was made.
  int RecordIgnore(const GURL& url, ContentSettingsType permission);

  // Removes any recorded counts for urls which match |filter|.
  void RemoveCountsByUrl(base::Callback<bool(const GURL& url)> filter);

 private:
  friend class PermissionContextBaseTests;
  friend class PermissionDecisionAutoBlockerUnitTest;

  explicit PermissionDecisionAutoBlocker(Profile* profile);
  ~PermissionDecisionAutoBlocker() override;

  // Get the result of the Safe Browsing check, if |should_be_embargoed| is true
  // then |request_origin| will be placed under embargo for that |permission|.
  void CheckSafeBrowsingResult(const GURL& request_origin,
                               ContentSettingsType permission,
                               base::Callback<void(bool)> callback,
                               bool should_be_embargoed);

  void PlaceUnderEmbargo(const GURL& request_origin,
                         ContentSettingsType permission,
                         const char* key);

  void SetSafeBrowsingDatabaseManagerAndTimeoutForTesting(
      scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> db_manager,
      int timeout);

  void SetClockForTesting(std::unique_ptr<base::Clock> clock);

  // Keys used for storing count data in a website setting.
  static const char kPromptDismissCountKey[];
  static const char kPromptIgnoreCountKey[];
  static const char kPermissionDismissalEmbargoKey[];
  static const char kPermissionBlacklistEmbargoKey[];

  Profile* profile_;
  scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> db_manager_;

  // Timeout in ms.
  int safe_browsing_timeout_;

  std::unique_ptr<base::Clock> clock_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(PermissionDecisionAutoBlocker);
};
#endif  // CHROME_BROWSER_PERMISSIONS_PERMISSION_DECISION_AUTO_BLOCKER_H_
