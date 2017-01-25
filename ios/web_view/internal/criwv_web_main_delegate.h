// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_CRIWV_WEB_MAIN_DELEGATE_H_
#define IOS_WEB_VIEW_INTERNAL_CRIWV_WEB_MAIN_DELEGATE_H_

#include <memory>
#include "base/macros.h"
#include "ios/web/public/app/web_main_delegate.h"

@protocol CRIWVDelegate;

namespace ios_web_view {
class CRIWVWebClient;

// CRIWV-specific implementation of WebMainDelegate.
class CRIWVWebMainDelegate : public web::WebMainDelegate {
 public:
  explicit CRIWVWebMainDelegate(id<CRIWVDelegate> delegate);
  ~CRIWVWebMainDelegate() override;

  // WebMainDelegate implementation.
  void BasicStartupComplete() override;

 private:
  // This object's delegate.
  id<CRIWVDelegate> delegate_;

  // The content and web clients registered by this object.
  std::unique_ptr<CRIWVWebClient> web_client_;

  DISALLOW_COPY_AND_ASSIGN(CRIWVWebMainDelegate);
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_CRIWV_WEB_MAIN_DELEGATE_H_
