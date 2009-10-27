// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/l10n_util.h"
#include "base/mac_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/profile.h"
#import "chrome/browser/cocoa/bookmark_name_folder_controller.h"
#include "grit/generated_resources.h"

@implementation BookmarkNameFolderController

- (id)initWithParentWindow:(NSWindow*)window
                   profile:(Profile*)profile
                      node:(const BookmarkNode*)node {
  NSString* nibpath = [mac_util::MainAppBundle()
                        pathForResource:@"BookmarkNameFolder"
                        ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibpath owner:self])) {
    parentWindow_ = window;
    profile_ = profile;
    node_ = node;
    std::wstring newFolderString =
      l10n_util::GetString(IDS_BOOMARK_EDITOR_NEW_FOLDER_NAME);
    initialName_.reset([base::SysWideToNSString(newFolderString) retain]);
  }
  return self;
}

- (void)awakeFromNib {
  [nameField_ setStringValue:initialName_.get()];
  [self controlTextDidChange:nil];
}

// Called as a side-effect of being the delegate of the text field. Ensure the
// OK button is only enabled when there is a valid name.
- (void)controlTextDidChange:(NSNotification*)ignore {
  [okButton_ setEnabled:[[nameField_ stringValue] length]];
}

// TODO(jrg): consider NSModalSession.
- (void)runAsModalSheet {
  [NSApp beginSheet:[self window]
     modalForWindow:parentWindow_
      modalDelegate:self
     didEndSelector:@selector(didEndSheet:returnCode:contextInfo:)
        contextInfo:nil];
}

- (IBAction)cancel:(id)sender {
  [NSApp endSheet:[self window]];
}

- (IBAction)ok:(id)sender {
  NSString* name = [nameField_ stringValue];
  if (![name isEqual:initialName_.get()]) {
    BookmarkModel* model = profile_->GetBookmarkModel();
    if (node_) {
      model->SetTitle(node_, base::SysNSStringToWide(name));
    } else {
      // TODO(jrg): check sender to accomodate creating a folder while
      // NOT over the bar (e.g. when over an expanded folder itself).
      // Need to wait until I add folders before I can do that
      // properly.
      // For now only add the folder at the top level.
      model->AddGroup(model->GetBookmarkBarNode(),
                      model->GetBookmarkBarNode()->GetChildCount(),
                      base::SysNSStringToWide(name));
    }
  }
  [NSApp endSheet:[self window]];
}

- (void)didEndSheet:(NSWindow*)sheet
         returnCode:(int)returnCode
        contextInfo:(void*)contextInfo {
  [[self window] orderOut:self];
  [self autorelease];
}

- (void)setFolderName:(NSString*)name {
  [nameField_ setStringValue:name];
  [self controlTextDidChange:nil];
}

- (NSButton*)okButton {
  return okButton_;
}

@end  // BookmarkNameFolderController
