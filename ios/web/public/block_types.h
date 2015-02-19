// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_BLOCK_TYPES_H_
#define IOS_WEB_PUBLIC_BLOCK_TYPES_H_

#import <Foundation/Foundation.h>

namespace web {

// The type of the completion handler block that is called to inform about
// JavaScript evaluation completion.
typedef void (^JavaScriptCompletion)(NSString*, NSError*);

}  // namespace

#endif  // IOS_WEB_PUBLIC_BLOCK_TYPES_H_
