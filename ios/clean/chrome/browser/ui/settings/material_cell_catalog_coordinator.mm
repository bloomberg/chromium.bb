// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/settings/material_cell_catalog_coordinator.h"

#import "ios/chrome/browser/ui/settings/material_cell_catalog_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface MaterialCellCatalogCoordinator ()
@property(nonatomic, strong) UIViewController* viewController;
@end

@implementation MaterialCellCatalogCoordinator
@synthesize viewController = _viewController;

- (void)start {
  self.viewController = [[MaterialCellCatalogViewController alloc] init];
  [super start];
}

@end
