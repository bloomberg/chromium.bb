// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/outdated_plugins.h"

#include "app/l10n_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "grit/generated_resources.h"
#include "webkit/glue/webplugininfo.h"

// A VersionPredicate function returns true iff the given version is considered
// to be out of date.
typedef bool (*VersionPredicate) (const std::wstring& version);

// static
bool OutdatedPlugins::CheckFlashVersion(const std::wstring& version) {
  // Returns true iff the given Flash version should be considered a security
  // risk.

  // Latest security advisory:
  //   http://www.adobe.com/support/security/bulletins/apsb09-10.html
  // suggests that versions < v9.0.246.0 and v10.0.32.18 are insecure.

  // The number of parts in the version number
  static const unsigned kExpectedNumParts = 4;

  std::vector<std::wstring> version_parts;
  SplitString(version, L'.', &version_parts);
  if (version_parts.size() != kExpectedNumParts)
    return false;  // can't parse version

  for (unsigned i = 0; i < kExpectedNumParts; ++i) {
    for (size_t j = 0; j < version_parts[i].size(); ++j) {
      if (!IsAsciiDigit(version_parts[i][j]))
        return false;  // parse failure
    }
  }

  unsigned version_numbers[kExpectedNumParts];
  for (unsigned i = 0; i < kExpectedNumParts; ++i)
    version_numbers[i] = StringToInt(WideToASCII(version_parts[i]));

  if (version_numbers[0] < 9) {
    return true;  // versions before 9 should be updated
  } else if (version_numbers[0] == 9) {
    if (version_numbers[1] == 0 &&
        version_numbers[2] < 246) {
      return true;  // versions before 9.0.246.0 are bad
    }

    return false;
  } else if (version_numbers[0] == 10) {
    if (version_numbers[1] == 0) {
      if (version_numbers[2] < 32) {
        return true;  // versions prior to 10.0.32.18 are bad
      } else if (version_numbers[2] == 32) {
        if (version_numbers[3] < 18)
          return true;  // versions prior to 10.0.32.18 are bad
      } else {
        return false;  // assume good
      }
    }

    return false;  // assume good
  } else {
    // version > 10 - we assume that it's good
    return false;
  }
}

struct OutdatedPluginInfo {
  const wchar_t name[16];
  const char update_url[34];
  VersionPredicate version_check_function;
} kOutdatedPlugins[] = {
  { L"Shockwave Flash", "http://get.adobe.com/flashplayer/",
    OutdatedPlugins::CheckFlashVersion },
  { L"", "", NULL },
};

// static
void OutdatedPlugins::Check(const std::vector<WebPluginInfo>& plugins,
                            MessageLoop* ui_loop) {
  static base::Time last_time_nagged;
  static bool last_time_nagged_valid = false;
  const base::Time current_time = base::Time::Now();

  if (last_time_nagged_valid) {
    static const base::TimeDelta one_day = base::TimeDelta::FromDays(1);
    // Bug the user, at most, once a day. Obviously, this resets every time the
    // user restarts Chrome. Because of that we also have a preferences entry
    // but we have to be on the UI thread in order to check that.
    if (last_time_nagged + one_day > current_time)
      return;
  }

  // This is a vector of indexes into |kOutdatedPlugins| for which we have
  // found that the given plugin is out of date.
  std::vector<unsigned>* outdated_plugins = NULL;

  for (std::vector<WebPluginInfo>::const_iterator
       i = plugins.begin(); i != plugins.end(); ++i) {
    for (size_t j = 0; kOutdatedPlugins[j].version_check_function; ++j) {
      if (i->name == kOutdatedPlugins[j].name) {
        if (kOutdatedPlugins[j].version_check_function(i->version)) {
          if (!outdated_plugins)
            outdated_plugins = new std::vector<unsigned>;
          outdated_plugins->push_back(j);
        }
      }
    }
  }

  if (!outdated_plugins)
    return;

  ui_loop->PostTask(FROM_HERE,
      NewRunnableFunction(&OutdatedPlugins::PluginAlert, outdated_plugins));

  last_time_nagged = current_time;
  last_time_nagged_valid = true;
}

class OutdatedPluginInfobarDelegate : public ConfirmInfoBarDelegate {
 public:
  OutdatedPluginInfobarDelegate(TabContents* tab_contents,
                                const std::wstring& plugin_name,
                                const char* update_url)
      : ConfirmInfoBarDelegate(tab_contents),
        plugin_name_(plugin_name),
        update_url_(update_url) {
  }

  virtual bool ShouldExpire(
      const NavigationController::LoadCommittedDetails& details) const {
    return false;
  }

  virtual std::wstring GetMessageText() const {
    return l10n_util::GetStringF(IDS_OUTDATED_PLUGIN, plugin_name_);
  }

  virtual int GetButtons() const {
    return BUTTON_OK;
  }

  virtual std::wstring GetButtonLabel(InfoBarButton button) const {
    DCHECK(button == BUTTON_OK);
    return l10n_util::GetString(IDS_ABOUT_CHROME_UPDATE_CHECK);
  }

  virtual bool Accept() {
    Browser* browser = BrowserList::GetLastActive();
    if (!browser)
      return true;

    browser->AddTabWithURL(GURL(update_url_), GURL(), PageTransition::LINK,
                           true, -1, false, NULL);
    return true;
  }

 private:
  const std::wstring plugin_name_;
  const char* update_url_;
};

// static
void OutdatedPlugins::PluginAlert(std::vector<unsigned>* in_outdated_plugins) {
  scoped_ptr<std::vector<unsigned> > outdated_plugins(in_outdated_plugins);

  // Runs on the UI thread.
  Browser* browser = BrowserList::GetLastActive();
  if (!browser)
    return;

  TabContents *contents = browser->GetSelectedTabContents();
  if (!contents)
    return;

  Profile* profile = browser->profile();
  if (profile) {
    // Now that we are on the UI thread, we can check the preferences value for
    // the last time that we nagged the user about plugins.
    PrefService* prefs = profile->GetPrefs();
    if (!prefs->IsPrefRegistered(prefs::kLastTimeNaggedAboutPlugins))
      prefs->RegisterRealPref(prefs::kLastTimeNaggedAboutPlugins, 0.);
    base::Time last_time_nagged = base::Time::FromDoubleT(
        prefs->GetReal(prefs::kLastTimeNaggedAboutPlugins));
    base::Time current_time = base::Time::Now();
    static const base::TimeDelta one_day = base::TimeDelta::FromDays(1);
    if (last_time_nagged + one_day > current_time)
      return;

    prefs->SetReal(prefs::kLastTimeNaggedAboutPlugins,
                   current_time.ToDoubleT());
  }

  for (std::vector<unsigned>::const_iterator
      i = outdated_plugins->begin(); i != outdated_plugins->end(); ++i) {
      contents->AddInfoBar(new OutdatedPluginInfobarDelegate(
            contents, kOutdatedPlugins[*i].name,
            kOutdatedPlugins[*i].update_url));
  }
}
