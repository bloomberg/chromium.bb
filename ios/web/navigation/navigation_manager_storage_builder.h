// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_SERIALIZED_NAVIGATION_MANAGER_BUILDER_H_
#define IOS_WEB_NAVIGATION_SERIALIZED_NAVIGATION_MANAGER_BUILDER_H_

#include <memory>

@class CRWNavigationManagerStorage;

namespace web {

class NavigationManagerImpl;

// Class that can serialize and deserialize NavigationManagers.
class NavigationManagerStorageBuilder {
 public:
  // Creates a serialized NavigationManager from |navigation_manager|.
  CRWNavigationManagerStorage* BuildStorage(
      NavigationManagerImpl* navigation_manager) const;
  // Creates a NavigationManager from |navigation_manager_storage|.
  std::unique_ptr<NavigationManagerImpl> BuildNavigationManagerImpl(
      CRWNavigationManagerStorage* navigation_manager_storage) const;
};

}  // namespace web

#endif  // IOS_WEB_NAVIGATION_SERIALIZED_NAVIGATION_MANAGER_BUILDER_H_
