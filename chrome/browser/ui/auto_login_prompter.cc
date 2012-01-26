// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/auto_login_prompter.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/string_split.h"
#include "chrome/browser/google/google_url_tracker.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/auto_login_info_bar_delegate.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "googleurl/src/gurl.h"
#include "net/base/escape.h"
#include "net/url_request/url_request.h"

using content::BrowserThread;
using content::NavigationController;
using content::WebContents;

static std::string ExtractContinueUrl(const GURL& url) {
  DCHECK(url.is_valid());
  const std::string& spec = url.spec();
  url_parse::Component query = url.parsed_for_possibly_invalid_spec().query;
  url_parse::Component key;
  url_parse::Component value;
  while (url_parse::ExtractQueryKeyValue(spec.c_str(), &query, &key, &value)) {
    if (key.is_nonempty() && spec.substr(key.begin, key.len) == "continue") {
      if (value.is_nonempty())
        return spec.substr(value.begin, value.len);
      break;
    }
  }

  // If no continue URL is found, redirect user to Google home page.
  return GoogleURLTracker::GoogleURL().spec();
}

AutoLoginPrompter::AutoLoginPrompter(
    WebContents* web_contents,
    const std::string& username,
    const std::string& args,
    const std::string& continue_url,
    bool use_normal_auto_login_infobar)
    : web_contents_(web_contents),
      username_(username),
      args_(args),
      continue_url_(continue_url),
      use_normal_auto_login_infobar_(use_normal_auto_login_infobar){
  registrar_.Add(this, content::NOTIFICATION_LOAD_STOP,
                 content::Source<NavigationController>(
                    &web_contents_->GetController()));
  registrar_.Add(this, content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                 content::Source<WebContents>(web_contents_));
}

AutoLoginPrompter::~AutoLoginPrompter() {
}

// static
void AutoLoginPrompter::ShowInfoBarIfPossible(net::URLRequest* request,
                                              int child_id,
                                              int route_id) {
  // See if the response contains the X-Auto-Login header.  If so, this was
  // a request for a login page, and the server is allowing the browser to
  // suggest auto-login, if available.
  std::string value;
  request->GetResponseHeaderByName("X-Auto-Login", &value);
  if (value.empty())
    return;

  std::vector<std::pair<std::string, std::string> > pairs;
  if (!base::SplitStringIntoKeyValuePairs(value, '=', '&', &pairs))
    return;

  // Parse the information from the value string.
  std::string realm;
  std::string account;
  std::string args;
  for (size_t i = 0; i < pairs.size(); ++i) {
    const std::pair<std::string, std::string>& pair = pairs[i];
    if (pair.first == "realm") {
      realm = net::UnescapeURLComponent(pair.second,
                                        net::UnescapeRule::URL_SPECIAL_CHARS);
    } else if (pair.first == "account") {
      account = net::UnescapeURLComponent(pair.second,
                                          net::UnescapeRule::URL_SPECIAL_CHARS);
    } else if (pair.first == "args") {
      args = pair.second;
    }
  }

  // Currently we only accept GAIA credentials.
  if (realm != "com.google")
    return;

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&AutoLoginPrompter::ShowInfoBarUIThread, account, args,
                 request->url(), child_id, route_id));
}

// static
void AutoLoginPrompter::ShowInfoBarUIThread(const std::string& account,
                                            const std::string& args,
                                            const GURL& url,
                                            int child_id,
                                            int route_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  WebContents* web_contents = tab_util::GetWebContentsByID(child_id, route_id);
  if (!web_contents)
    return;

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  // In an incognito window, there may not be a profile sync service and/or
  // signin manager.
  if (!profile->HasProfileSyncService())
    return;

  SigninManager* signin_manager = profile->GetProfileSyncService()->signin();
  if (!signin_manager)
    return;

  // If there are credentials in the token manager, then we want to show the
  // user the regular auto-login infobar, asking if they want to turn on
  // auto-login.  If there are no credentials in the token manager, then we
  // want to show the reverse auto-login infobar, asking if they want to
  // connect their profile to a Google account.
  bool use_normal_auto_login_infobar =
      profile->GetTokenService()->AreCredentialsValid();
  const std::string& username = signin_manager->GetAuthenticatedUsername();
  std::string continue_url;

  if (use_normal_auto_login_infobar) {
    if (!CommandLine::ForCurrentProcess()->
        HasSwitch(switches::kEnableAutologin))
      return;

    // Make sure that |account|, if specified, matches the logged in user.
    // However, |account| is usually empty.
    if (!account.empty() && (username != account))
      return;

    if (!profile->GetPrefs()->GetBoolean(prefs::kAutologinEnabled))
      return;
  } else if (profile->GetPrefs()->GetBoolean(prefs::kReverseAutologinEnabled)) {
    continue_url = ExtractContinueUrl(url);
  } else {
    return;
  }

  // We can't add the infobar just yet, since we need to wait for the tab to
  // finish loading.  If we don't, the info bar appears and then disappears
  // immediately.  Create an AutoLoginPrompter instance to listen for the
  // relevant notifications; it will delete itself.
  new AutoLoginPrompter(web_contents, username, args, continue_url,
                        use_normal_auto_login_infobar);
}

void AutoLoginPrompter::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  if (type == content::NOTIFICATION_LOAD_STOP) {
    TabContentsWrapper* wrapper =
        TabContentsWrapper::GetCurrentWrapperForContents(web_contents_);
    // |wrapper| is NULL for TabContents hosted in HTMLDialog.
    if (wrapper) {
      InfoBarTabHelper* infobar_helper = wrapper->infobar_tab_helper();
      Profile* profile = wrapper->profile();
      InfoBarDelegate* delegate = NULL;
      if (use_normal_auto_login_infobar_) {
        delegate = new AutoLoginInfoBarDelegate(
            infobar_helper, &web_contents_->GetController(),
            profile->GetTokenService(), profile->GetPrefs(),
            username_, args_);
      } else {
        delegate = new ReverseAutoLoginInfoBarDelegate(
            infobar_helper, &web_contents_->GetController(),
            profile->GetPrefs(), continue_url_);
      }

      infobar_helper->AddInfoBar(delegate);
    }
  }
  // Either we couldn't add the infobar, we added the infobar, or the tab
  // contents was destroyed before the navigation completed.  In any case
  // there's no reason to live further.
  delete this;
}
