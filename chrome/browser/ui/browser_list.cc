// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_list.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list_impl.h"
#include "chrome/browser/ui/browser_list_observer.h"

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
  GetNativeList()->AddObserver(observer);
}

// static
void BrowserList::RemoveObserver(chrome::BrowserListObserver* observer) {
  GetNativeList()->RemoveObserver(observer);
}

void BrowserList::CloseAllBrowsersWithProfile(Profile* profile) {
  GetNativeList()->CloseAllBrowsersWithProfile(profile);
}

// static
BrowserList::const_iterator BrowserList::begin() {
  return GetNativeList()->begin();
}

// static
BrowserList::const_iterator BrowserList::end() {
  return GetNativeList()->end();
}

// static
bool BrowserList::empty() {
  return GetNativeList()->empty();
}

// static
size_t BrowserList::size() {
  return GetNativeList()->size();
}

// static
void BrowserList::SetLastActive(Browser* browser) {
  chrome::BrowserListImpl::GetInstance(browser->host_desktop_type())->
      SetLastActive(browser);
}

// static
BrowserList::const_reverse_iterator BrowserList::begin_last_active() {
  return GetNativeList()->begin_last_active();
}

// static
BrowserList::const_reverse_iterator BrowserList::end_last_active() {
  return GetNativeList()->end_last_active();
}

// static
bool BrowserList::IsOffTheRecordSessionActive() {
  return GetNativeList()->IsIncognitoWindowOpen();
}

// static
bool BrowserList::IsOffTheRecordSessionActiveForProfile(Profile* profile) {
  return GetNativeList()->IsIncognitoWindowOpenForProfile(profile);
}
