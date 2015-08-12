// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_manager.h"

#include "chrome/browser/signin/signin_promo.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "google_apis/gaia/gaia_urls.h"

namespace {

bool AddToSet(std::set<content::WebContents*>* content_set,
              content::WebContents* web_contents) {
  content_set->insert(web_contents);
  return false;
}

}  // namespace

UserManager::ReauthDialogObserver::ReauthDialogObserver(
    content::WebContents* web_contents, const std::string& email_address)
    : email_address_(email_address) {
  // Observe navigations of the web contents so that the dialog can close itself
  // when the sign in process is done.
  Observe(web_contents);
}

void UserManager::ReauthDialogObserver::DidStopLoading() {
  // If the sign in process reaches the termination URL, close the dialog.
  // Make sure to remove any parts of the URL that gaia might append during
  // signin.
  GURL url = web_contents()->GetURL();
  url::Replacements<char> replacements;
  replacements.ClearQuery();
  replacements.ClearRef();
  if (url.ReplaceComponents(replacements) ==
        GaiaUrls::GetInstance()->signin_completed_continue_url()) {
    CloseReauthDialog();
    return;
  }

  // If still observing the top level web contents, try to find the embedded
  // webview and observe it instead.  The webview may not be found in the
  // initial page load since it loads asynchronously.
  if (url.GetOrigin() !=
        signin::GetReauthURLWithEmail(email_address_).GetOrigin()) {
    return;
  }

  std::set<content::WebContents*> content_set;
  guest_view::GuestViewManager* manager =
      guest_view::GuestViewManager::FromBrowserContext(
          web_contents()->GetBrowserContext());
  if (manager)
    manager->ForEachGuest(web_contents(), base::Bind(&AddToSet, &content_set));
  DCHECK_LE(content_set.size(), 1U);
  if (!content_set.empty())
    Observe(*content_set.begin());
}
