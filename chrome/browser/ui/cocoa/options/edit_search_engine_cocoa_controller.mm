// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/options/edit_search_engine_cocoa_controller.h"

#include "app/l10n_util_mac.h"
#include "base/logging.h"
#import "base/mac/mac_util.h"
#include "base/string16.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/search_engines/template_url.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

void ShiftOriginY(NSView* view, CGFloat amount) {
  NSPoint origin = [view frame].origin;
  origin.y += amount;
  [view setFrameOrigin:origin];
}

}  // namespace

@implementation EditSearchEngineCocoaController

- (id)initWithProfile:(Profile*)profile
             delegate:(EditSearchEngineControllerDelegate*)delegate
          templateURL:(const TemplateURL*)url {
  DCHECK(profile);
  NSString* nibpath = [base::mac::MainAppBundle()
                        pathForResource:@"EditSearchEngine"
                                 ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibpath owner:self])) {
    profile_ = profile;
    templateURL_ = url;
    controller_.reset(
        new EditSearchEngineController(templateURL_, delegate, profile_));
  }
  return self;
}

- (void)awakeFromNib {
  DCHECK([self window]);
  DCHECK_EQ(self, [[self window] delegate]);

  // Make sure the url description field fits the text in it.
  CGFloat descriptionShift = [GTMUILocalizerAndLayoutTweaker
      sizeToFitFixedWidthTextField:urlDescriptionField_];

  // Move the label container above the url description.
  ShiftOriginY(labelContainer_, descriptionShift);
  // There was no way via view containment to use a helper view to move all
  // the textfields and images at once, most move them all on their own so
  // they stay above the url description.
  ShiftOriginY(nameField_, descriptionShift);
  ShiftOriginY(keywordField_, descriptionShift);
  ShiftOriginY(urlField_, descriptionShift);
  ShiftOriginY(nameImage_, descriptionShift);
  ShiftOriginY(keywordImage_, descriptionShift);
  ShiftOriginY(urlImage_, descriptionShift);

  // Resize the containing box for the name/keyword/url fields/images since it
  // also contains the url description (which just grew).
  [[fieldAndImageContainer_ contentView] setAutoresizesSubviews:NO];
  NSRect rect = [fieldAndImageContainer_ frame];
  rect.size.height += descriptionShift;
  [fieldAndImageContainer_ setFrame:rect];
  [[fieldAndImageContainer_ contentView] setAutoresizesSubviews:YES];

  // Resize the window.
  NSWindow* window = [self window];
  NSSize windowDelta = NSMakeSize(0, descriptionShift);
  [GTMUILocalizerAndLayoutTweaker
      resizeWindowWithoutAutoResizingSubViews:window
                                        delta:windowDelta];

  ResourceBundle& bundle = ResourceBundle::GetSharedInstance();
  goodImage_.reset([bundle.GetNativeImageNamed(IDR_INPUT_GOOD) retain]);
  badImage_.reset([bundle.GetNativeImageNamed(IDR_INPUT_ALERT) retain]);
  if (templateURL_) {
    // Defaults to |..._NEW_WINDOW_TITLE|.
    [window setTitle:l10n_util::GetNSString(
      IDS_SEARCH_ENGINES_EDITOR_EDIT_WINDOW_TITLE)];
    [nameField_ setStringValue:
        base::SysUTF16ToNSString(templateURL_->short_name())];
    [keywordField_ setStringValue:
        base::SysUTF16ToNSString(templateURL_->keyword())];
    [urlField_ setStringValue:
        base::SysUTF16ToNSString(templateURL_->url()->DisplayURL())];
    [urlField_ setEnabled:(templateURL_->prepopulate_id() == 0)];
  }
  // When creating a new keyword, this will mark the fields as "invalid" and
  // will not let the user save. If this is an edit, then this will set all
  // the images to the "valid" state.
  [self validateFields];
}

// When the window closes, clean ourselves up.
- (void)windowWillClose:(NSNotification*)notif {
  [self autorelease];
}

// Performs the logic of closing the window. If we are a sheet, then it ends the
// modal session; otherwise, it closes the window.
- (void)doClose {
  if ([[self window] isSheet]) {
    [NSApp endSheet:[self window]];
  } else {
    [[self window] close];
  }
}

- (IBAction)cancel:(id)sender {
  [self doClose];
}

- (IBAction)save:(id)sender {
  DCHECK([self validateFields]);
  string16 title = base::SysNSStringToUTF16([nameField_ stringValue]);
  string16 keyword = base::SysNSStringToUTF16([keywordField_ stringValue]);
  std::string url = base::SysNSStringToUTF8([urlField_ stringValue]);
  controller_->AcceptAddOrEdit(title, keyword, url);
  [self doClose];
}

// Delegate method for the text fields.

- (void)controlTextDidChange:(NSNotification*)notif {
  [self validateFields];
}

- (void)controlTextDidEndEditing:(NSNotification*)notif {
  [self validateFields];
}

// Private --------------------------------------------------------------------

// Sets the appropriate image and tooltip based on a boolean |valid|.
- (void)setIsValid:(BOOL)valid
           toolTip:(int)messageID
      forImageView:(NSImageView*)imageView
         textField:(NSTextField*)textField {
  NSImage* image = (valid) ? goodImage_ : badImage_;
  [imageView setImage:image];

  NSString* toolTip = nil;
  if (!valid)
    toolTip = l10n_util::GetNSString(messageID);
  [textField setToolTip:toolTip];
  [imageView setToolTip:toolTip];
}

// This sets the image state for all the controls and enables or disables the
// done button. Returns YES if all the fields are valid.
- (BOOL)validateFields {
  string16 title = base::SysNSStringToUTF16([nameField_ stringValue]);
  BOOL titleValid = controller_->IsTitleValid(title);
  [self setIsValid:titleValid
           toolTip:IDS_SEARCH_ENGINES_INVALID_TITLE_TT
      forImageView:nameImage_
         textField:nameField_];

  string16 keyword = base::SysNSStringToUTF16([keywordField_ stringValue]);
  BOOL keywordValid = controller_->IsKeywordValid(keyword);
  [self setIsValid:keywordValid
           toolTip:IDS_SEARCH_ENGINES_INVALID_KEYWORD_TT
      forImageView:keywordImage_
         textField:keywordField_];

  std::string url = base::SysNSStringToUTF8([urlField_ stringValue]);
  BOOL urlValid = controller_->IsURLValid(url);
  [self setIsValid:urlValid
           toolTip:IDS_SEARCH_ENGINES_INVALID_URL_TT
      forImageView:urlImage_
         textField:urlField_];

  BOOL isValid = (titleValid && keywordValid && urlValid);
  [doneButton_ setEnabled:isValid];
  return isValid;
}

@end
