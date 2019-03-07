// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_TESTING_EARL_GREY_EARL_GREY_APP_H_
#define IOS_TESTING_EARL_GREY_EARL_GREY_APP_H_

// Contains includes and typedefs to allow code to compile under both EarlGrey1
// and EarlGrey2 (App Process).

#if defined(CHROME_EARL_GREY_1)

#import <EarlGrey/EarlGrey.h>

typedef DescribeToBlock GREYDescribeToBlock;
typedef MatchesBlock GREYMatchesBlock;

#elif defined(CHROME_EARL_GREY_2)

#import <AppFramework/Action/GREYActionsShorthand.h>
#import <AppFramework/EarlGreyApp.h>
#import <AppFramework/Matcher/GREYMatchersShorthand.h>

#else
#error Must define either CHROME_EARL_GREY_1 or CHROME_EARL_GREY_2.
#endif

#endif  // IOS_TESTING_EARL_GREY_EARL_GREY_APP_H_
