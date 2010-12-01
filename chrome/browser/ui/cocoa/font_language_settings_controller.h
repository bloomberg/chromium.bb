// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/cocoa_protocols_mac.h"
#include "base/scoped_nsobject.h"
#include "chrome/browser/prefs/pref_member.h"

class Profile;

// Used to keep track of which type of font the user is currently selecting.
enum FontSettingType {
  FontSettingSerif,
  FontSettingSansSerif,
  FontSettingFixed
};

// Keys for the dictionaries in the |encodings_| array.
extern NSString* const kCharacterInfoEncoding;  // NSString value.
extern NSString* const kCharacterInfoName;  // NSString value.
extern NSString* const kCharacterInfoID;  // NSNumber value.

// A window controller that allows the user to change the default WebKit fonts
// and language encodings for web pages. This window controller is meant to be
// used as a modal sheet on another window.
@interface FontLanguageSettingsController : NSWindowController
                                            <NSWindowDelegate> {
 @private
  // The font that we are currently changing.
  NSFont* currentFont_;  // weak
  FontSettingType currentType_;

  IBOutlet NSButton* serifButton_;
  IBOutlet NSTextField* serifField_;
  scoped_nsobject<NSFont> serifFont_;
  IBOutlet NSTextField* serifLabel_;
  BOOL changedSerif_;

  IBOutlet NSButton* sansSerifButton_;
  IBOutlet NSTextField* sansSerifField_;
  scoped_nsobject<NSFont> sansSerifFont_;
  IBOutlet NSTextField* sansSerifLabel_;
  BOOL changedSansSerif_;

  IBOutlet NSButton* fixedWidthButton_;
  IBOutlet NSTextField* fixedWidthField_;
  scoped_nsobject<NSFont> fixedWidthFont_;
  IBOutlet NSTextField* fixedWidthLabel_;
  BOOL changedFixedWidth_;

  // The actual preference members.
  StringPrefMember serifName_;
  StringPrefMember sansSerifName_;
  StringPrefMember fixedWidthName_;
  IntegerPrefMember serifSize_;
  IntegerPrefMember sansSerifSize_;
  IntegerPrefMember fixedWidthSize_;

  // Array of dictionaries that contain the canonical encoding name, human-
  // readable name, and the ID. See the constants defined at the top of this
  // file for the keys.
  scoped_nsobject<NSMutableArray> encodings_;

  IBOutlet NSPopUpButton* encodingsMenu_;
  NSInteger defaultEncodingIndex_;
  StringPrefMember defaultEncoding_;
  BOOL changedEncoding_;

  Profile* profile_;  // weak
}

// Profile cannot be NULL. Caller is responsible for showing the window as a
// modal sheet.
- (id)initWithProfile:(Profile*)profile;

// Action for all the font changing buttons. This starts the font picker.
- (IBAction)selectFont:(id)sender;

// Sent by the FontManager after the user has selected a font.
- (void)changeFont:(id)fontManager;

// Performs the closing of the window. This is used by both the cancel button
// and |-save:| after it persists the settings.
- (IBAction)closeSheet:(id)sender;

// Persists the new values into the preferences and closes the sheet.
- (IBAction)save:(id)sender;

// Returns the |encodings_| array. This is used by bindings for KVO/KVC.
- (NSArray*)encodings;

@end
