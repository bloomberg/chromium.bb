// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_list.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/browser/ui/browser_list_impl.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/host_desktop.h"

using content::WebContents;

namespace {

chrome::BrowserListImpl* GetNativeList() {
  return chrome::BrowserListImpl::GetInstance(chrome::HOST_DESKTOP_TYPE_NATIVE);
}

}  // namespace

// static
void BrowserList::AddBrowser(Browser* browser) {
  chrome::BrowserListImpl::GetInstance(browser->host_desktop_type())->
      AddBrowser(browser);
}

// static
void BrowserList::RemoveBrowser(Browser* browser) {
  chrome::BrowserListImpl::GetInstance(browser->host_desktop_type())->
      RemoveBrowser(browser);
}

// static
void BrowserList::AddObserver(chrome::BrowserListObserver* observer) {
  for (chrome::HostDesktopType t = chrome::HOST_DESKTOP_TYPE_FIRST;
       t < chrome::HOST_DESKTOP_TYPE_COUNT;
       t = static_cast<chrome::HostDesktopType>(t + 1)) {
    chrome::BrowserListImpl::GetInstance(t)->AddObserver(observer);
  }
}

// static
void BrowserList::RemoveObserver(chrome::BrowserListObserver* observer) {
  for (chrome::HostDesktopType t = chrome::HOST_DESKTOP_TYPE_FIRST;
       t < chrome::HOST_DESKTOP_TYPE_COUNT;
       t = static_cast<chrome::HostDesktopType>(t + 1)) {
    chrome::BrowserListImpl::GetInstance(t)->RemoveObserver(observer);
  }
}

void BrowserList::CloseAllBrowsersWithProfile(Profile* profile) {
  BrowserVector browsers_to_close;
  for (chrome::BrowserIterator it; !it.done(); it.Next()) {
    if (it->profile()->GetOriginalProfile() == profile->GetOriginalProfile())
      browsers_to_close.push_back(*it);
  }

  for (BrowserVector::const_iterator it = browsers_to_close.begin();
       it != browsers_to_close.end(); ++it) {
    (*it)->window()->Close();
  }
}

// static
void BrowserList::SetLastActive(Browser* browser) {
  chrome::BrowserListImpl::GetInstance(browser->host_desktop_type())->
      SetLastActive(browser);
}

// static
bool BrowserList::IsOffTheRecordSessionActive() {
  return GetNativeList()->IsIncognitoWindowOpen();
}

// static
bool BrowserList::IsOffTheRecordSessionActiveForProfile(Profile* profile) {
  return GetNativeList()->IsIncognitoWindowOpenForProfile(profile);
}
