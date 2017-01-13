// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/tab_model_list.h"

#include "base/logging.h"
#include "base/macros.h"
#include "base/supports_user_data.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/tabs/tab_model.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Wrapper around a NSMutableArray<TabModel> allowing to bind it to a
// base::SupportsUserData. Any base::SupportsUserData that owns this
// wrapper will owns the reference to the TabModels.
class TabModelList : public base::SupportsUserData::Data {
 public:
  TabModelList();
  ~TabModelList() override;

  static TabModelList* GetForBrowserState(
      ios::ChromeBrowserState* browser_state,
      bool create);

  NSMutableSet<TabModel*>* tab_models() const { return tab_models_; }

 private:
  NSMutableSet<TabModel*>* tab_models_;

  DISALLOW_COPY_AND_ASSIGN(TabModelList);
};

const char kTabModelListKey = 0;

TabModelList::TabModelList() : tab_models_([[NSMutableSet alloc] init]) {}

TabModelList::~TabModelList() {
  // TabModelList is destroyed during base::SupportsUserData destruction. At
  // that point, all TabModels must have been unregistered already.
  DCHECK_EQ([tab_models_ count], 0u);
}

// static
TabModelList* TabModelList::GetForBrowserState(
    ios::ChromeBrowserState* browser_state,
    bool create) {
  TabModelList* tab_model_list =
      static_cast<TabModelList*>(browser_state->GetUserData(&kTabModelListKey));
  if (!tab_model_list && create) {
    // The ownership of TabModelList is transfered to base::SupportsUserData
    // via the SetUserData call (should take a std::unique_ptr<>).
    tab_model_list = new TabModelList;
    browser_state->SetUserData(&kTabModelListKey, tab_model_list);
  }
  return tab_model_list;
}
}  // namespace

void RegisterTabModelWithChromeBrowserState(
    ios::ChromeBrowserState* browser_state,
    TabModel* tab_model) {
  NSMutableSet<TabModel*>* tab_models =
      TabModelList::GetForBrowserState(browser_state, true)->tab_models();
  DCHECK(![tab_models containsObject:tab_model]);
  [tab_models addObject:tab_model];
}

void UnregisterTabModelFromChromeBrowserState(
    ios::ChromeBrowserState* browser_state,
    TabModel* tab_model) {
  NSMutableSet<TabModel*>* tab_models =
      TabModelList::GetForBrowserState(browser_state, false)->tab_models();
  DCHECK([tab_models containsObject:tab_model]);
  [tab_models removeObject:tab_model];
}

NSArray<TabModel*>* GetTabModelsForChromeBrowserState(
    ios::ChromeBrowserState* browser_state) {
  TabModelList* tab_model_list =
      TabModelList::GetForBrowserState(browser_state, false);
  return tab_model_list ? [tab_model_list->tab_models() allObjects] : nil;
}
