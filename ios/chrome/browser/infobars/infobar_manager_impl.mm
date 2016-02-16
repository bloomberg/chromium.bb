// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/infobars/infobar_manager_impl.h"

#include <utility>

#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "ios/chrome/browser/infobars/infobar_utils.h"
#include "ios/web/public/load_committed_details.h"
#include "ios/web/public/navigation_item.h"
#include "ios/web/public/navigation_manager.h"
#include "ios/web/public/web_state/web_state.h"
#include "ui/base/page_transition_types.h"

DEFINE_WEB_STATE_USER_DATA_KEY(InfoBarManagerImpl);

namespace {

infobars::InfoBarDelegate::NavigationDetails
NavigationDetailsFromLoadCommittedDetails(
    const web::LoadCommittedDetails& load_details) {
  infobars::InfoBarDelegate::NavigationDetails navigation_details;
  navigation_details.entry_id = load_details.item->GetUniqueID();
  const ui::PageTransition transition = load_details.item->GetTransitionType();
  navigation_details.is_navigation_to_different_page =
      ui::PageTransitionIsMainFrame(transition) && !load_details.is_in_page;
  // web::LoadCommittedDetails doesn't store this information, default to false.
  navigation_details.did_replace_entry = false;
  navigation_details.is_reload =
      ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_RELOAD);
  navigation_details.is_redirect = ui::PageTransitionIsRedirect(transition);

  return navigation_details;
}

}  // namespace

// static
web::WebState* InfoBarManagerImpl::WebStateFromInfoBar(
    infobars::InfoBar* infobar) {
  if (!infobar || !infobar->owner())
    return nullptr;
  return static_cast<InfoBarManagerImpl*>(infobar->owner())->web_state();
}

InfoBarManagerImpl::InfoBarManagerImpl(web::WebState* web_state)
    : web::WebStateObserver(web_state) {
  DCHECK(web_state);
}

InfoBarManagerImpl::~InfoBarManagerImpl() {
  ShutDown();
}

int InfoBarManagerImpl::GetActiveEntryID() {
  web::NavigationItem* visible_item =
      web_state()->GetNavigationManager()->GetVisibleItem();
  return visible_item ? visible_item->GetUniqueID() : 0;
}

scoped_ptr<infobars::InfoBar> InfoBarManagerImpl::CreateConfirmInfoBar(
    scoped_ptr<ConfirmInfoBarDelegate> delegate) {
  return ::CreateConfirmInfoBar(std::move(delegate));
}

void InfoBarManagerImpl::NavigationItemCommitted(
    const web::LoadCommittedDetails& load_details) {
  OnNavigation(NavigationDetailsFromLoadCommittedDetails(load_details));
}

void InfoBarManagerImpl::WebStateDestroyed() {
  // The WebState is going away; be aggressively paranoid and delete this
  // InfoBarManagerImpl lest other parts of the system attempt to add infobars
  // or use it otherwise during the destruction.
  web_state()->RemoveUserData(UserDataKey());
  // That was the equivalent of "delete this". This object is now destroyed;
  // returning from this function is the only safe thing to do.
}

void InfoBarManagerImpl::OpenURL(const GURL& url,
                                 WindowOpenDisposition disposition) {
  NOTIMPLEMENTED();
}
