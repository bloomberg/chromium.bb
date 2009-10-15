// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/mac_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_editor.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/profile.h"
#import "chrome/browser/cocoa/bookmark_editor_controller.h"

// static; implemented for each platform.
void BookmarkEditor::Show(gfx::NativeWindow parent_hwnd,
                          Profile* profile,
                          const BookmarkNode* parent,
                          const BookmarkNode* node,
                          Configuration configuration,
                          Handler* handler) {
  BookmarkEditorController* controller = [[BookmarkEditorController alloc]
                                           initWithParentWindow:parent_hwnd
                                                        profile:profile
                                                         parent:parent
                                                           node:node
                                                  configuration:configuration
                                                        handler:handler];
  [controller runAsModalSheet];
}


@implementation BookmarkEditorController

- (id)initWithParentWindow:(NSWindow*)parentWindow
                   profile:(Profile*)profile
                    parent:(const BookmarkNode*)parent
                      node:(const BookmarkNode*)node
             configuration:(BookmarkEditor::Configuration)configuration
                   handler:(BookmarkEditor::Handler*)handler {
  NSString* nibpath = [mac_util::MainAppBundle()
                        pathForResource:@"BookmarkEditor"
                        ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibpath owner:self])) {
    parentWindow_ = parentWindow;
    profile_ = profile;
    parentNode_ = parent;
    // "Add Page..." has no "node" so this may be NULL.
    node_ = node;
    configuration_ = configuration;
    handler_.reset(handler);
  }
  return self;
}

- (void)awakeFromNib {
  // Set text fields to match our bookmark.  If the node is NULL we
  // arrived here from an "Add Page..." item in a context menu.
  if (node_) {
    initialName_.reset([base::SysWideToNSString(node_->GetTitle()) retain]);
    std::string url_string = node_->GetURL().possibly_invalid_spec();
    initialUrl_.reset([[NSString stringWithUTF8String:url_string.c_str()]
                        retain]);
  } else {
    initialName_.reset([@"" retain]);
    initialUrl_.reset([@"" retain]);
  }
  [nameField_ setStringValue:initialName_];
  [urlField_ setStringValue:initialUrl_];

  // Get a ping when the URL or name text fields change;
  // trigger an initial ping to set things up.
  [nameField_ setDelegate:self];
  [urlField_ setDelegate:self];
  [self controlTextDidChange:nil];

  if (configuration_ == BookmarkEditor::SHOW_TREE) {
    // build the tree et al
    NOTIMPLEMENTED();
  } else {
    // Remember the NSBrowser's height; we will shrink our frame by that
    // much.
    NSRect frame = [[self window] frame];
    CGFloat browserHeight = [browser_ frame].size.height;
    frame.size.height -= browserHeight;
    frame.origin.y += browserHeight;
    // Remove the NSBrowser and "new folder" button.
    [browser_ removeFromSuperview];
    [newFolderButton_ removeFromSuperview];
    // Finally, commit the size change.
    [[self window] setFrame:frame display:YES];
  }
}

/* TODO(jrg):
// Implementing this informal protocol allows us to open the sheet
// somewhere other than at the top of the window.  NOTE: this means
// that I, the controller, am also the window's delegate.
- (NSRect)window:(NSWindow*)window willPositionSheet:(NSWindow*)sheet
        usingRect:(NSRect)rect {
  // adjust rect.origin.y to be the bottom of the toolbar
  return rect;
}
*/

// TODO(jrg): consider NSModalSession.
- (void)runAsModalSheet {
  [NSApp beginSheet:[self window]
     modalForWindow:parentWindow_
      modalDelegate:self
     didEndSelector:@selector(didEndSheet:returnCode:contextInfo:)
        contextInfo:nil];
}

// TODO(jrg)
- (IBAction)newFolder:(id)sender {
  NOTIMPLEMENTED();
}

- (IBAction)cancel:(id)sender {
  [NSApp endSheet:[self window]];
}

// If possible, return a valid GURL from the URL text field.
- (GURL)GURLFromUrlField {
  NSString *url = [urlField_ stringValue];
  GURL newURL = GURL([url UTF8String]);
  if (!newURL.is_valid()) {
    // Mimic observed friendliness from Windows
    newURL = GURL([[NSString stringWithFormat:@"http://%@", url] UTF8String]);
  }
  return newURL;
}

// When the URL changes we may enable or disable the OK button.
// We set ourselves as the delegate of urlField_ so this gets called.
// (Yes, setting ourself as a delegate automatically registers us for
// the notification.)
- (void)controlTextDidChange:(NSNotification *)aNotification {
  GURL newURL = [self GURLFromUrlField];
  NSString* name = [nameField_ stringValue];

  // if empty or only whitespace, name is not valid.
  bool name_valid = true;
  if (([name length] == 0) ||
      ([[name stringByTrimmingCharactersInSet:[NSCharacterSet
              whitespaceAndNewlineCharacterSet]] length] == 0)) {
    name_valid = false;
  }

  if (node_ && (node_->is_folder() || newURL.is_valid()) && name_valid) {
    [okButton_ setEnabled:YES];
  } else {
    [okButton_ setEnabled:NO];
  }
}

// TODO(jrg): Once the tree is available edits may be more extensive
// than just name/url.
- (IBAction)ok:(id)sender {
  NSString *name = [nameField_ stringValue];
  NSString *url = [urlField_ stringValue];

  if ((![name isEqual:initialName_]) ||
      (![url isEqual:initialUrl_])) {
    std::wstring newTitle = base::SysNSStringToWide(name);
    GURL newURL = [self GURLFromUrlField];
    if (!newURL.is_valid()) {
      // Shouldn't be reached -- OK button disabled if not valid!
      NOTREACHED();
      return;
    }
    int index = 0;
    BookmarkModel* model = profile_->GetBookmarkModel();
    if (node_) {
      index = parentNode_->IndexOfChild(node_);
      model->Remove(parentNode_, index);
    } else {
      index = parentNode_->GetChildCount();
    }
    const BookmarkNode* node = model->AddURL(parentNode_, index,
                                             newTitle, newURL);
    // Honor handler semantics: callback on node creation
    if (handler_.get())
      handler_->NodeCreated(node);
  }

  [NSApp endSheet:[self window]];
}

- (void)didEndSheet:(NSWindow*)sheet
         returnCode:(int)returnCode
        contextInfo:(void*)contextInfo {
  // This is probably unnecessary but it feels cleaner since the
  // delegate of a text field can be automatically registered for
  // notifications.
  [nameField_ setDelegate:nil];
  [urlField_ setDelegate:nil];

  [[self window] orderOut:self];

  // BookmarkEditor::Show() will create us then run away.  Unusually
  // for a controller, we are responsible for deallocating ourself.
  [self autorelease];
}


- (NSString*)displayName {
  return [nameField_ stringValue];
}

- (NSString*)displayURL {
  return [urlField_ stringValue];
}

- (void)setDisplayName:(NSString*)name {
  [nameField_ setStringValue:name];
  [self controlTextDidChange:nil];
}

- (void)setDisplayURL:(NSString*)name {
  [urlField_ setStringValue:name];
  [self controlTextDidChange:nil];
}

- (BOOL)okButtonEnabled {
  return [okButton_ isEnabled];
}

@end  // BookmarkEditorController

