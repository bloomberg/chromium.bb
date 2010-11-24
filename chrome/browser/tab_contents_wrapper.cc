// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents_wrapper.h"

#include "base/singleton.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/password_manager_delegate_impl.h"
#include "chrome/browser/tab_contents/tab_contents.h"

////////////////////////////////////////////////////////////////////////////////
// TabContentsWrapper, public:

TabContentsWrapper::TabContentsWrapper(TabContents* contents)
    : tab_contents_(contents) {
  DCHECK(contents);
  // Stash this in the property bag so it can be retrieved without having to
  // go to a Browser.
  property_accessor()->SetProperty(contents->property_bag(), this);

  // Needed so that we initialize the password manager on first navigation.
  tab_contents()->AddNavigationObserver(this);
}

TabContentsWrapper::~TabContentsWrapper() {
  // Unregister observers (TabContents outlives supporting objects).
  tab_contents()->RemoveNavigationObserver(password_manager_.get());
}

PropertyAccessor<TabContentsWrapper*>* TabContentsWrapper::property_accessor() {
  return Singleton< PropertyAccessor<TabContentsWrapper*> >::get();
}

TabContentsWrapper* TabContentsWrapper::Clone() {
  TabContents* new_contents = tab_contents()->Clone();
  TabContentsWrapper* new_wrapper = new TabContentsWrapper(new_contents);
  // Instantiate the passowrd manager if it has been instantiated here.
  if (password_manager_.get())
    new_wrapper->GetPasswordManager();
  return new_wrapper;
}

PasswordManager* TabContentsWrapper::GetPasswordManager() {
  if (!password_manager_.get()) {
    // Create the delegate then create the manager.
    password_manager_delegate_.reset(
        new PasswordManagerDelegateImpl(tab_contents()));
    password_manager_.reset(
        new PasswordManager(password_manager_delegate_.get()));
    // Register the manager to receive navigation notifications.
    tab_contents()->AddNavigationObserver(password_manager_.get());
  }
  return password_manager_.get();
}

////////////////////////////////////////////////////////////////////////////////
// TabContentsWrapper, WebNavigationObserver implementation:

void TabContentsWrapper::NavigateToPendingEntry() {
  GetPasswordManager();
  tab_contents()->RemoveNavigationObserver(this);
}
