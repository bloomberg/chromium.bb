// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

class TemplateURL;

#import "base/mac/cocoa_protocols.h"
#include "base/scoped_nsobject.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/ui/search_engines/edit_search_engine_controller.h"

// This controller presents a dialog that allows a user to add or edit a search
// engine. If constructed with a nil |templateURL| then it is an add operation,
// otherwise it will modify the passed URL. A |delegate| is necessary to
// perform the actual database modifications, and should probably be an
// instance of KeywordEditorModelObserver.

@interface EditSearchEngineCocoaController :
    NSWindowController<NSWindowDelegate> {
  IBOutlet NSTextField* nameField_;
  IBOutlet NSTextField* keywordField_;
  IBOutlet NSTextField* urlField_;
  IBOutlet NSImageView* nameImage_;
  IBOutlet NSImageView* keywordImage_;
  IBOutlet NSImageView* urlImage_;
  IBOutlet NSButton* doneButton_;
  IBOutlet NSTextField* urlDescriptionField_;
  IBOutlet NSView* labelContainer_;
  IBOutlet NSBox* fieldAndImageContainer_;

  // Refs to the good and bad images used in the interface validation.
  scoped_nsobject<NSImage> goodImage_;
  scoped_nsobject<NSImage> badImage_;

  Profile* profile_;  // weak
  const TemplateURL* templateURL_;  // weak
  scoped_ptr<EditSearchEngineController> controller_;
}

- (id)initWithProfile:(Profile*)profile
             delegate:(EditSearchEngineControllerDelegate*)delegate
          templateURL:(const TemplateURL*)url;

- (IBAction)cancel:(id)sender;
- (IBAction)save:(id)sender;

@end

@interface EditSearchEngineCocoaController (ExposedForTesting)
- (BOOL)validateFields;
@end
