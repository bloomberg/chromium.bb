// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include <vector>

#import "base/memory/ref_counted.h"
#import "base/memory/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"

class Profile;
class SearchEngineDialogControllerBridge;
class TemplateURL;
class TemplateURLModel;

// Class that acts as a controller for the search engine choice dialog.
@interface SearchEngineDialogController : NSWindowController {
 @private
  // Our current profile.
  Profile* profile_;

  // If logos are to be displayed in random order. Used for UX testing.
  bool randomize_;

  // Owned by the profile_.
  TemplateURLModel* searchEnginesModel_;

  // Bridge to the C++ world.
  scoped_refptr<SearchEngineDialogControllerBridge> bridge_;

  // Offered search engine choices.
  std::vector<const TemplateURL*> choices_;

  IBOutlet NSImageView* headerImageView_;
  IBOutlet NSView* searchEngineView_;
}

@property(assign, nonatomic) Profile* profile;
@property(assign, nonatomic) bool randomize;

// Properties for bindings.
@property(readonly) NSFont* mainLabelFont;

@end
