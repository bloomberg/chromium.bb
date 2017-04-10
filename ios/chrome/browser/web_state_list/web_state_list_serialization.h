// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_STATE_LIST_WEB_STATE_LIST_SERIALIZATION_H_
#define IOS_CHROME_BROWSER_WEB_STATE_LIST_WEB_STATE_LIST_SERIALIZATION_H_

#import <Foundation/Foundation.h>

#include <memory>

#include "base/callback_forward.h"

@class CRWSessionStorage;
class WebStateList;

namespace web {
class WebState;
}

// Factory for creating WebStates.
using WebStateFactory =
    base::RepeatingCallback<std::unique_ptr<web::WebState>(CRWSessionStorage*)>;

// Returns an array of serialised sessions.
NSArray<CRWSessionStorage*>* SerializeWebStateList(
    WebStateList* web_state_list);

// Restores |sessions| into |web_state_list| using |web_state_factory| for
// creating the restored WebStates.
void DeserializeWebStateList(WebStateList* web_state_list,
                             NSArray<CRWSessionStorage*>* sessions,
                             const WebStateFactory& web_state_factory);

#endif  // IOS_CHROME_BROWSER_WEB_STATE_LIST_WEB_STATE_LIST_SERIALIZATION_H_
