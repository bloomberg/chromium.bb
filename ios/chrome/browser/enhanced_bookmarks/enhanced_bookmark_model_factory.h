// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENHANCED_BOOKMARKS_ENHANCED_BOOKMARK_MODEL_FACTORY_H_
#define IOS_CHROME_BROWSER_ENHANCED_BOOKMARKS_ENHANCED_BOOKMARK_MODEL_FACTORY_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace ios {
class ChromeBrowserState;
}

namespace enhanced_bookmarks {

class EnhancedBookmarkModel;

// A factory to create BookmarkServerClusterService and associate them to
// ios::ChromeBrowserState.
class EnhancedBookmarkModelFactory : public BrowserStateKeyedServiceFactory {
 public:
  static EnhancedBookmarkModelFactory* GetInstance();
  static EnhancedBookmarkModel* GetForBrowserState(
      ios::ChromeBrowserState* browser_state);

 private:
  friend struct base::DefaultSingletonTraits<EnhancedBookmarkModelFactory>;

  EnhancedBookmarkModelFactory();
  ~EnhancedBookmarkModelFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  scoped_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;

  DISALLOW_COPY_AND_ASSIGN(EnhancedBookmarkModelFactory);
};

}  // namespace enhanced_bookmarks

#endif  // IOS_CHROME_BROWSER_ENHANCED_BOOKMARKS_ENHANCED_BOOKMARK_MODEL_FACTORY_H_
