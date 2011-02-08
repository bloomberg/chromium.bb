// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"

#include "base/lazy_instance.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/password_manager_delegate_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/ui/find_bar/find_manager.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper_delegate.h"
#include "chrome/common/notification_service.h"

static base::LazyInstance<PropertyAccessor<TabContentsWrapper*> >
    g_tab_contents_wrapper_property_accessor(base::LINKER_INITIALIZED);

////////////////////////////////////////////////////////////////////////////////
// TabContentsWrapper, public:

TabContentsWrapper::TabContentsWrapper(TabContents* contents)
    : delegate_(NULL),
      is_starred_(false),
      tab_contents_(contents) {
  DCHECK(contents);
  // Stash this in the property bag so it can be retrieved without having to
  // go to a Browser.
  property_accessor()->SetProperty(contents->property_bag(), this);

  // Needed so that we initialize the password manager on first navigation.
  tab_contents()->AddObserver(this);

  // Register for notifications about URL starredness changing on any profile.
  registrar_.Add(this, NotificationType::URLS_STARRED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::BOOKMARK_MODEL_LOADED,
                 NotificationService::AllSources());
}

TabContentsWrapper::~TabContentsWrapper() {
  // We don't want any notifications while we're running our destructor.
  registrar_.RemoveAll();

  // Unregister observers.
  tab_contents()->RemoveObserver(this);
  if (password_manager_.get())
    tab_contents()->RemoveObserver(password_manager_.get());
}

PropertyAccessor<TabContentsWrapper*>* TabContentsWrapper::property_accessor() {
  return g_tab_contents_wrapper_property_accessor.Pointer();
}

TabContentsWrapper* TabContentsWrapper::Clone() {
  TabContents* new_contents = tab_contents()->Clone();
  TabContentsWrapper* new_wrapper = new TabContentsWrapper(new_contents);
  // Instantiate the password manager if it has been instantiated here.
  if (password_manager_.get())
    new_wrapper->GetPasswordManager();
  return new_wrapper;
}

TabContentsWrapper* TabContentsWrapper::GetCurrentWrapperForContents(
    TabContents* contents) {
  TabContentsWrapper** wrapper =
      property_accessor()->GetProperty(contents->property_bag());

  return wrapper ? *wrapper : NULL;
}

PasswordManager* TabContentsWrapper::GetPasswordManager() {
  if (!password_manager_.get()) {
    // Create the delegate then create the manager.
    password_manager_delegate_.reset(
        new PasswordManagerDelegateImpl(tab_contents()));
    password_manager_.reset(
        new PasswordManager(password_manager_delegate_.get()));
    // Register the manager to receive navigation notifications.
    tab_contents()->AddObserver(password_manager_.get());
  }
  return password_manager_.get();
}

FindManager* TabContentsWrapper::GetFindManager() {
  if (!find_manager_.get()) {
    find_manager_.reset(new FindManager(this));
    // Register the manager to receive navigation notifications.
    tab_contents()->AddObserver(find_manager_.get());
  }
  return find_manager_.get();
}

////////////////////////////////////////////////////////////////////////////////
// TabContentsWrapper, TabContentsObserver implementation:

void TabContentsWrapper::NavigateToPendingEntry() {
  // If any page loads, ensure that the password manager is loaded so that forms
  // get filled out.
  GetPasswordManager();
}

void TabContentsWrapper::DidNavigateMainFramePostCommit(
    const NavigationController::LoadCommittedDetails& /*details*/,
    const ViewHostMsg_FrameNavigate_Params& /*params*/) {
  UpdateStarredStateForCurrentURL();
}

////////////////////////////////////////////////////////////////////////////////
// TabContentsWrapper, NotificationObserver implementation:

void TabContentsWrapper::Observe(NotificationType type,
                                 const NotificationSource& source,
                                 const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::BOOKMARK_MODEL_LOADED:
      // BookmarkModel finished loading, fall through to update starred state.
    case NotificationType::URLS_STARRED: {
      // Somewhere, a URL has been starred.
      // Ignore notifications for profiles other than our current one.
      Profile* source_profile = Source<Profile>(source).ptr();
      if (!source_profile || !source_profile->IsSameProfile(profile()))
        return;

      UpdateStarredStateForCurrentURL();
      break;
    }

    default:
      NOTREACHED();
  }
}

////////////////////////////////////////////////////////////////////////////////
// Internal helpers

void TabContentsWrapper::UpdateStarredStateForCurrentURL() {
  BookmarkModel* model = tab_contents()->profile()->GetBookmarkModel();
  const bool old_state = is_starred_;
  is_starred_ = (model && model->IsBookmarked(tab_contents()->GetURL()));

  if (is_starred_ != old_state && delegate())
    delegate()->URLStarredChanged(this, is_starred_);
}
