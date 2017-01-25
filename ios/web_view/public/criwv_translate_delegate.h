// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CRIWV_TRANSLATE_DELEGATE_H_
#define IOS_WEB_VIEW_PUBLIC_CRIWV_TRANSLATE_DELEGATE_H_

#import <Foundation/Foundation.h>

@protocol CRIWVTranslateManager;

namespace ios_web_view {

typedef NS_ENUM(NSInteger, CRIWVTransateStep) {
  CRIWVTransateStepBeforeTranslate,
  CRIWVTransateStepTranslating,
  CRIWVTransateStepAfterTranslate,
  CRIWVTransateStepError,
};

}  // namespace ios_web_view

// Delegate interface for the CRIWVTranslate.  Embedders can implement the
// functions in order to customize the behavior.
@protocol CRIWVTranslateDelegate

- (void)translateStepChanged:(ios_web_view::CRIWVTransateStep)step
                     manager:(id<CRIWVTranslateManager>)manager;

@end

#endif  // IOS_WEB_VIEW_PUBLIC_CRIWV_TRANSLATE_DELEGATE_H_
