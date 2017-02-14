// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_TRANSLATE_DELEGATE_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_TRANSLATE_DELEGATE_H_

#import <Foundation/Foundation.h>

@protocol CRIWVTranslateManager;

typedef NS_ENUM(NSInteger, CRIWVTransateStep) {
  CRIWVTransateStepBeforeTranslate,
  CRIWVTransateStepTranslating,
  CRIWVTransateStepAfterTranslate,
  CRIWVTransateStepError,
};

// Delegate interface for the CRIWVTranslate.  Embedders can implement the
// functions in order to customize the behavior.
@protocol CWVTranslateDelegate

- (void)translateStepChanged:(CRIWVTransateStep)step
                     manager:(id<CRIWVTranslateManager>)manager;

@end

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_TRANSLATE_DELEGATE_H_
