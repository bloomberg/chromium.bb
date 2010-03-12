// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/cookie_prompt_window_controller.h"

#include <string>
#include <vector>

#include "app/l10n_util_mac.h"
#include "app/resource_bundle.h"
#import "base/i18n/time_formatting.h"
#import "base/mac_util.h"
#include "base/sys_string_conversions.h"
#import "chrome/browser/cocoa/cookie_details_view_controller.h"
#include "chrome/browser/cocoa/cookie_tree_node.h"
#include "chrome/browser/cookie_modal_dialog.h"
#include "chrome/browser/cookies_tree_model.h"
#include "grit/generated_resources.h"
#import "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"

#pragma mark Window Controller

// This class is an apapter allows the cookie details view to be shared
// by the cookie prompt window and the cookie tree in the cookies and
// other site data window. As instance of the class is set as the
// content object of the object controller for the details view and
// implements the methods expected by bindings inside that view.
@interface CookiePromptSelectionAdapter : NSObject {
 @private
  // The type of the cookie prompt being displayed, used to
  // determine which subview of the details view is visible
  CookiePromptModalDialog::DialogType promptType_;

  // The following members are used to hold information used in the
  // cookie prompt detailed information for cookies and web databases.
  scoped_nsobject<NSString> name_;
  scoped_nsobject<NSString> domain_;
  scoped_nsobject<NSString> content_;

  // The following members are used to hold information used in the
  // cookie prompt detailed information for cookies only.
  scoped_nsobject<NSString> path_;
  scoped_nsobject<NSString> sendFor_;
  scoped_nsobject<NSString> created_;
  scoped_nsobject<NSString> expires_;

  // The following members are used to hold information used in the
  // cookie prompt detailed information for local storage only.
  scoped_nsobject<NSString> localStorageKey_;
  scoped_nsobject<NSString> localStorageValue_;
}

// Creates and returns an instance approriate for displaying information
// about a cookie.
- (id)initWithCookie:(const std::string&)cookie_line
                 url:(const GURL&)url;

// Creates and returns an instance approriate for displaying information
// about a local storage.
- (id)initWithLocalStorage:(const std::string&)domain
                       key:(const string16&)key
                     value:(const string16&)value;

// Creates and returns an instance approriate for displaying information
// about a web database.
- (id)initWithDatabase:(const std::string&)domain
                  name:(const string16&)name;

// The following methods are all used in the bindings inside the cookie
// detail view.
@property (readonly) BOOL isFolderOrCookieTreeDetails;
@property (readonly) BOOL isLocalStorageTreeDetails;
@property (readonly) BOOL isDatabaseTreeDetails;
@property (readonly) BOOL isDatabasePromptDetails;
@property (readonly) BOOL isLocalStoragePromptDetails;
@property (readonly) NSString* name;
@property (readonly) NSString* content;
@property (readonly) NSString* domain;
@property (readonly) NSString* path;
@property (readonly) NSString* sendFor;
@property (readonly) NSString* created;
@property (readonly) NSString* expires;
@property (readonly) NSString* fileSize;
@property (readonly) NSString* lastModified;
@property (readonly) NSString* databaseDescription;
@property (readonly) NSString* localStorageKey;
@property (readonly) NSString* localStorageValue;

@end

@implementation CookiePromptSelectionAdapter

- (id)initWithCookie:(const std::string&)cookie_line
                 url:(const GURL&)url {
  if ((self = [super init])) {
    promptType_ = CookiePromptModalDialog::DIALOG_TYPE_COOKIE;
    net::CookieMonster::ParsedCookie pc(cookie_line);
    net::CookieMonster::CanonicalCookie cookie(url, pc);
    const std::string& domain(pc.HasDomain() ? pc.Domain() : url.host());
    domain_.reset([base::SysUTF8ToNSString(domain) retain]);
    name_.reset([base::SysUTF8ToNSString(cookie.Name()) retain]);
    content_.reset([base::SysUTF8ToNSString(cookie.Value()) retain]);
    path_.reset([base::SysUTF8ToNSString(cookie.Path()) retain]);

    if (cookie.DoesExpire()) {
      expires_.reset([base::SysWideToNSString(
          base::TimeFormatFriendlyDateAndTime(cookie.ExpiryDate()))
          retain]);
    } else {
      expires_.reset([l10n_util::GetNSStringWithFixup(
          IDS_COOKIES_COOKIE_EXPIRES_SESSION) retain]);
    }

    created_.reset([base::SysWideToNSString(
        base::TimeFormatFriendlyDateAndTime(cookie.CreationDate()))
        retain]);

    if (cookie.IsSecure()) {
      sendFor_.reset([l10n_util::GetNSStringWithFixup(
          IDS_COOKIES_COOKIE_SENDFOR_SECURE) retain]);
    } else {
      sendFor_.reset([l10n_util::GetNSStringWithFixup(
          IDS_COOKIES_COOKIE_SENDFOR_ANY) retain]);
    }
  }
  return self;
}

- (id)initWithLocalStorage:(const std::string&)domain
                       key:(const string16&)key
                     value:(const string16&)value {
  if ((self = [super init])) {
    promptType_ = CookiePromptModalDialog::DIALOG_TYPE_LOCAL_STORAGE;
    domain_.reset([base::SysUTF8ToNSString(domain) retain]);
    localStorageKey_.reset([base::SysUTF16ToNSString(key) retain]);
    localStorageValue_.reset([base::SysUTF16ToNSString(value) retain]);
  }
  return self;
}

- (id)initWithDatabase:(const std::string&)domain
                  name:(const string16&)name {
  if ((self = [super init])) {
    promptType_ = CookiePromptModalDialog::DIALOG_TYPE_DATABASE;
    name_.reset([base::SysUTF16ToNSString(name) retain]);
    domain_.reset([base::SysUTF8ToNSString(domain) retain]);
  }
  return self;
}

- (BOOL)isFolderOrCookieTreeDetails {
  return promptType_ == CookiePromptModalDialog::DIALOG_TYPE_COOKIE;
}

- (BOOL)isLocalStorageTreeDetails {
  return false;
}

- (BOOL)isDatabaseTreeDetails {
  return false;
}

- (BOOL) isDatabasePromptDetails {
  return promptType_ == CookiePromptModalDialog::DIALOG_TYPE_DATABASE;
}

- (BOOL) isLocalStoragePromptDetails {
  return promptType_ == CookiePromptModalDialog::DIALOG_TYPE_LOCAL_STORAGE;
}

- (NSString*)name {
  return name_;
}

- (NSString*)content {
  return content_;
}

- (NSString*)domain {
  return domain_;
}

- (NSString*)path {
  return path_;
}

- (NSString*)sendFor {
  return sendFor_;
}

- (NSString*)created {
  return created_;
}

- (NSString*)expires {
  return expires_;
}

- (NSString*)fileSize {
  return nil;
}

- (NSString*)lastModified {
  return nil;
}

- (NSString*)databaseDescription {
  return nil;
}

- (NSString*)localStorageKey {
  return localStorageKey_;
}

- (NSString*)localStorageValue {
  return localStorageValue_;
}

@end

@implementation CookiePromptWindowController

- (id)initWithDialog:(CookiePromptModalDialog*)dialog {
  NSString* nibpath = [mac_util::MainAppBundle()
      pathForResource:@"CookiePrompt"
               ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibpath owner:self])) {
    dialog_ = dialog;
    CookiePromptModalDialog::DialogType type(dialog_->dialog_type());
    if (type == CookiePromptModalDialog::DIALOG_TYPE_COOKIE) {
      selectionAdapterObject_.reset([[CookiePromptSelectionAdapter alloc]
          initWithCookie:dialog_->cookie_line()
                     url:dialog_->origin()]);
    } else if (type == CookiePromptModalDialog::DIALOG_TYPE_LOCAL_STORAGE) {
      selectionAdapterObject_.reset([[CookiePromptSelectionAdapter alloc]
          initWithLocalStorage:dialog_->origin().host()
                           key:dialog_->local_storage_key()
                         value:dialog_->local_storage_value()]);
    } else if (type == CookiePromptModalDialog::DIALOG_TYPE_DATABASE) {
      selectionAdapterObject_.reset([[CookiePromptSelectionAdapter alloc]
          initWithDatabase:dialog_->origin().host()
                      name:dialog_->database_name()]);
    } else {
      NOTIMPLEMENTED();
    }
  }
  return self;
}

// Ensures that all parameterized localized strings are filled in.
- (void)doLocalizationTweaks {
  int descriptionStringId = 0;
  switch (dialog_->dialog_type()) {
    case CookiePromptModalDialog::DIALOG_TYPE_COOKIE:
      descriptionStringId = IDS_COOKIE_ALERT_LABEL;
      break;
    case CookiePromptModalDialog::DIALOG_TYPE_LOCAL_STORAGE:
      descriptionStringId = IDS_DATA_ALERT_LABEL;
      break;
    case CookiePromptModalDialog::DIALOG_TYPE_DATABASE:
      descriptionStringId = IDS_DATA_ALERT_LABEL;
      break;
    default:
      NOTREACHED();
      break;
  }

  string16 displayHost = UTF8ToUTF16(dialog_->origin().host());
  NSString* description = l10n_util::GetNSStringF(
      descriptionStringId, displayHost);
  [description_ setStringValue:description];

  // Add the host to the "remember for future prompts" radio button.
  NSString* allowText = l10n_util::GetNSStringWithFixup(
      IDS_COOKIE_ALERT_REMEMBER_RADIO);
  NSString* replacedCellText = base::SysUTF16ToNSString(
      ReplaceStringPlaceholders(base::SysNSStringToUTF16(allowText),
          displayHost, NULL));
  [rememberChoiceCell_ setTitle:replacedCellText];
}

// Adjust the vertical layout of the views in the window so that
// they are spaced correctly with their fully localized contents.
- (void)doLayoutTweaks {
  // Wrap the description text.
  CGFloat sizeDelta = [GTMUILocalizerAndLayoutTweaker
      sizeToFitFixedWidthTextField:description_];
  NSRect descriptionFrame = [description_ frame];
  descriptionFrame.origin.y -= sizeDelta;
  [description_ setFrame:descriptionFrame];

  // Wrap the radio buttons to fit if necessary.
  [GTMUILocalizerAndLayoutTweaker
      wrapRadioGroupForWidth:radioGroupMatrix_];
  sizeDelta += [GTMUILocalizerAndLayoutTweaker
      sizeToFitView:radioGroupMatrix_].height;
  NSRect radioGroupFrame = [radioGroupMatrix_ frame];
  radioGroupFrame.origin.y -= sizeDelta;
  [radioGroupMatrix_ setFrame:radioGroupFrame];

  // Adjust views location, they may have moved through the
  // expansion of the radio buttons and description text.
  NSRect disclosureViewFrame = [disclosureTriangleSuperView_ frame];
  disclosureViewFrame.origin.y -= sizeDelta;
  [disclosureTriangleSuperView_ setFrame:disclosureViewFrame];

  // Adjust the final window size by the size of the cookie details
  // view, since it will be initially hidden.
  NSRect detailsViewRect = [disclosedViewPlaceholder_ frame];
  sizeDelta -= detailsViewRect.size.height;

  // Final resize the window to fit all of the adjustments
  NSRect frame = [[self window] frame];
  frame.origin.y -= sizeDelta;
  frame.size.height += sizeDelta;
  [[self window] setFrame:frame display:NO animate:NO];
}

- (void)replaceCookieDetailsView {
  detailsViewController_.reset([[CookieDetailsViewController alloc] init]);

  NSRect viewFrameRect = [disclosedViewPlaceholder_ frame];
  [[disclosedViewPlaceholder_ superview]
      replaceSubview:disclosedViewPlaceholder_
                with:[detailsViewController_.get() view]];
}

- (void)awakeFromNib {
  DCHECK(disclosureTriangle_);
  DCHECK(radioGroupMatrix_);
  DCHECK(disclosedViewPlaceholder_);
  DCHECK(disclosureTriangleSuperView_);

  [self doLocalizationTweaks];
  [self doLayoutTweaks];
  [self replaceCookieDetailsView];

  [detailsViewController_ setContentObject:selectionAdapterObject_.get()];
  [detailsViewController_ shrinkViewToFit];

  [[detailsViewController_ view] setHidden:YES];
}

- (void)windowWillClose:(NSNotification*)notif {
  [self autorelease];
}

// |contextInfo| is the bridge back to the C++ CookiePromptModalDialog.
- (void)processModalDialogResult:(void*)contextInfo
                      returnCode:(int)returnCode  {
  CookiePromptModalDialog* bridge =
      reinterpret_cast<CookiePromptModalDialog*>(contextInfo);
  bool remember = [radioGroupMatrix_ selectedRow] == 0;
  switch (returnCode) {
    case NSAlertFirstButtonReturn: {  // OK
      bool sessionExpire = false;
      bridge->AllowSiteData(remember, sessionExpire);
      break;
    }
    case NSAlertSecondButtonReturn: {  // Cancel
      bridge->BlockSiteData(remember);
      break;
    }
    default:  {
      NOTREACHED();
      remember = false;
      bridge->BlockSiteData(remember);
    }
  }
}

- (void)doModalDialog:(void*)context {
  NSInteger returnCode = [NSApp runModalForWindow:[self window]];
  [self processModalDialogResult:context returnCode:returnCode];
}

- (IBAction)disclosureTrianglePressed:(id)sender {
  NSWindow* window = [self window];
  NSRect frame = [[self window] frame];
  CGFloat sizeChange = [[detailsViewController_.get() view] frame].size.height;
  switch ([sender state]) {
    case NSOnState:
      frame.size.height += sizeChange;
      frame.origin.y -= sizeChange;
      break;
    case NSOffState:
      frame.size.height -= sizeChange;
      frame.origin.y += sizeChange;
      break;
    default:
      NOTREACHED();
      break;
  }
  if ([sender state] == NSOffState) {
    [[detailsViewController_ view] setHidden:YES];
  }
  [window setFrame:frame display:YES animate:YES];
  if ([sender state] == NSOnState) {
    [[detailsViewController_ view] setHidden:NO];
  }
}

// Callback for "accept" button.
- (IBAction)accept:(id)sender {
  [[self window] close];
  [NSApp stopModalWithCode:NSAlertFirstButtonReturn];
}

// Callback for "block" button.
- (IBAction)block:(id)sender {
  [[self window] close];
  [NSApp  stopModalWithCode:NSAlertSecondButtonReturn];
}

@end
