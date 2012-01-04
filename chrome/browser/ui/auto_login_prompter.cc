// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/auto_login_prompter.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/string_split.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/net/gaia/token_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/signin_manager.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/auto_login_info_bar_delegate.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "net/url_request/url_request.h"

using content::BrowserThread;
using content::NavigationController;
using content::WebContents;

AutoLoginPrompter::AutoLoginPrompter(
    WebContents* web_contents,
    const std::string& username,
    const std::string& args)
    : web_contents_(web_contents),
      username_(username),
      args_(args) {
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
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kEnableAutologin))
    return;

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
      realm = pair.second;
    } else if (pair.first == "account") {
      account = pair.second;
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
                 child_id, route_id));
}

// static
void AutoLoginPrompter::ShowInfoBarUIThread(const std::string& account,
                                            const std::string& args,
                                            int child_id,
                                            int route_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  TabContents* tab_contents = tab_util::GetTabContentsByID(child_id, route_id);
  if (!tab_contents)
    return;

  // If auto-login is turned off, then simply return.
  Profile* profile =
      Profile::FromBrowserContext(tab_contents->GetBrowserContext());
  if (!profile->GetPrefs()->GetBoolean(prefs::kAutologinEnabled))
    return;

  // In an incognito window, there may not be a profile sync service and/or
  // signin manager.
  if (!profile->HasProfileSyncService())
    return;
  SigninManager* signin_manager = profile->GetProfileSyncService()->signin();
  if (!signin_manager)
    return;

  // Make sure that |account|, if specified, matches the logged in user.
  // However, |account| is usually empty.
  const std::string& username = signin_manager->GetAuthenticatedUsername();
  if (!account.empty() && (username != account))
    return;

  // Make sure there are credentials in the token manager, otherwise there is
  // no way to craft the TokenAuth URL.
  if (!profile->GetTokenService()->AreCredentialsValid())
    return;

  // We can't add the infobar just yet, since we need to wait for the tab to
  // finish loading.  If we don't, the info bar appears and then disappears
  // immediately.  Create an AutoLoginPrompter instance to listen for the
  // relevant notifications; it will delete itself.
  new AutoLoginPrompter(tab_contents, username, args);
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
      infobar_helper->AddInfoBar(new AutoLoginInfoBarDelegate(
          infobar_helper, &web_contents_->GetController(),
          profile->GetTokenService(), profile->GetPrefs(),
          username_, args_));
    }
  }
  // Either we couldn't add the infobar, we added the infobar, or the tab
  // contents was destroyed before the navigation completed.  In any case
  // there's no reason to live further.
  delete this;
}
