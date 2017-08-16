// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/settings/material_cell_catalog_coordinator.h"

#import "ios/chrome/browser/ui/coordinators/browser_coordinator_test.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

typedef BrowserCoordinatorTest MaterialCellCatalogCoordinatorTest;

TEST_F(MaterialCellCatalogCoordinatorTest, Start) {
  MaterialCellCatalogCoordinator* coordinator =
      [[MaterialCellCatalogCoordinator alloc] init];
  [coordinator start];
}
