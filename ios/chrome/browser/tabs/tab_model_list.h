// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_TAB_MODEL_LIST_H_
#define IOS_CHROME_BROWSER_TABS_TAB_MODEL_LIST_H_

#import <Foundation/Foundation.h>

// This file contains free functions to help maintain a 1:N relationship
// between an ios::ChromeBrowserState and multiple TabModels.

@class TabModel;

namespace ios {
class ChromeBrowserState;
}

// Registers |tab_model| as associated to |browser_state|. The object will
// be retained until |UnregisterTabModelFromChromeBrowserState| is called.
// It is an error if |tab_model is already registered as associated to
// |browser_state|.
void RegisterTabModelWithChromeBrowserState(
    ios::ChromeBrowserState* browser_state,
    TabModel* tab_model);

// Unregisters the association between |tab_model| and |browser_state|.
// It is an error if no such association exists.
void UnregisterTabModelFromChromeBrowserState(
    ios::ChromeBrowserState* browser_state,
    TabModel* tab_model);

// Returns the list of all TabModels associated with |browser_state|.
NSArray<TabModel*>* GetTabModelsForChromeBrowserState(
    ios::ChromeBrowserState* browser_state);

// Returns the last active TabModel associated with |browser_state|.
TabModel* GetLastActiveTabModelForChromeBrowserState(
    ios::ChromeBrowserState* browser_state);

// Returns true if a incognito session is currently active (i.e. at least
// one incognito tab is open).
bool IsOffTheRecordSessionActive();

#endif  // IOS_CHROME_BROWSER_TABS_TAB_MODEL_LIST_H_
