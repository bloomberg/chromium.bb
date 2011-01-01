// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/font_language_settings_controller.h"

#import <Cocoa/Cocoa.h>
#import "base/mac/mac_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/character_encoding.h"
#include "chrome/browser/fonts_languages_window.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"

NSString* const kCharacterInfoEncoding = @"encoding";
NSString* const kCharacterInfoName = @"name";
NSString* const kCharacterInfoID = @"id";

void ShowFontsLanguagesWindow(gfx::NativeWindow window,
                              FontsLanguagesPage page,
                              Profile* profile) {
  NOTIMPLEMENTED();
}

@interface FontLanguageSettingsController (Private)
- (void)updateDisplayField:(NSTextField*)field
                  withFont:(NSFont*)font
                 withLabel:(NSTextField*)label;
@end

@implementation FontLanguageSettingsController

- (id)initWithProfile:(Profile*)profile {
  DCHECK(profile);
  NSString* nibpath = [base::mac::MainAppBundle()
                        pathForResource:@"FontLanguageSettings"
                                 ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibpath owner:self])) {
    profile_ = profile;

    // Convert the name/size preference values to NSFont objects.
    serifName_.Init(prefs::kWebKitSerifFontFamily, profile->GetPrefs(), NULL);
    serifSize_.Init(prefs::kWebKitDefaultFontSize, profile->GetPrefs(), NULL);
    NSString* serif = base::SysUTF8ToNSString(serifName_.GetValue());
    serifFont_.reset(
        [[NSFont fontWithName:serif size:serifSize_.GetValue()] retain]);

    sansSerifName_.Init(prefs::kWebKitSansSerifFontFamily, profile->GetPrefs(),
                        NULL);
    sansSerifSize_.Init(prefs::kWebKitDefaultFontSize, profile->GetPrefs(),
                        NULL);
    NSString* sansSerif = base::SysUTF8ToNSString(sansSerifName_.GetValue());
    sansSerifFont_.reset(
        [[NSFont fontWithName:sansSerif
                         size:sansSerifSize_.GetValue()] retain]);

    fixedWidthName_.Init(prefs::kWebKitFixedFontFamily, profile->GetPrefs(),
                         NULL);
    fixedWidthSize_.Init(prefs::kWebKitDefaultFixedFontSize,
                         profile->GetPrefs(), NULL);
    NSString* fixedWidth = base::SysUTF8ToNSString(fixedWidthName_.GetValue());
    fixedWidthFont_.reset(
        [[NSFont fontWithName:fixedWidth
                         size:fixedWidthSize_.GetValue()] retain]);

    // Generate a list of encodings.
    NSInteger count = CharacterEncoding::GetSupportCanonicalEncodingCount();
    NSMutableArray* encodings = [NSMutableArray arrayWithCapacity:count];
    for (NSInteger i = 0; i < count; ++i) {
      int commandId = CharacterEncoding::GetEncodingCommandIdByIndex(i);
      string16 name = CharacterEncoding::\
          GetCanonicalEncodingDisplayNameByCommandId(commandId);
      std::string encoding =
          CharacterEncoding::GetCanonicalEncodingNameByCommandId(commandId);
      NSDictionary* strings = [NSDictionary dictionaryWithObjectsAndKeys:
          base::SysUTF16ToNSString(name), kCharacterInfoName,
          base::SysUTF8ToNSString(encoding), kCharacterInfoEncoding,
          [NSNumber numberWithInt:commandId], kCharacterInfoID,
          nil
      ];
      [encodings addObject:strings];
    }

    // Sort the encodings.
    scoped_nsobject<NSSortDescriptor> sorter(
        [[NSSortDescriptor alloc] initWithKey:kCharacterInfoName
                                    ascending:YES]);
    NSArray* sorterArray = [NSArray arrayWithObject:sorter.get()];
    encodings_.reset(
        [[encodings sortedArrayUsingDescriptors:sorterArray] retain]);

    // Find and set the default encoding.
    defaultEncoding_.Init(prefs::kDefaultCharset, profile->GetPrefs(), NULL);
    NSString* defaultEncoding =
        base::SysUTF8ToNSString(defaultEncoding_.GetValue());
    NSUInteger index = 0;
    for (NSDictionary* entry in encodings_.get()) {
      NSString* encoding = [entry objectForKey:kCharacterInfoEncoding];
      if ([encoding isEqualToString:defaultEncoding]) {
        defaultEncodingIndex_ = index;
        break;
      }
      ++index;
    }

    // Register as a KVO observer so we can receive updates when the encoding
    // changes.
    [self addObserver:self
           forKeyPath:@"defaultEncodingIndex_"
              options:NSKeyValueObservingOptionNew
              context:NULL];
  }
  return self;
}

- (void)dealloc {
  [self removeObserver:self forKeyPath:@"defaultEncodingIndex_"];
  [super dealloc];
}

- (void)awakeFromNib {
  DCHECK([self window]);
  [[self window] setDelegate:self];

  // Set up the font display.
  [self updateDisplayField:serifField_
                  withFont:serifFont_.get()
                 withLabel:serifLabel_];
  [self updateDisplayField:sansSerifField_
                  withFont:sansSerifFont_.get()
                 withLabel:sansSerifLabel_];
  [self updateDisplayField:fixedWidthField_
                  withFont:fixedWidthFont_.get()
                 withLabel:fixedWidthLabel_];
}

- (void)windowWillClose:(NSNotification*)notif {
  [self autorelease];
}

- (IBAction)selectFont:(id)sender {
  if (sender == serifButton_) {
    currentFont_ = serifFont_.get();
    currentType_ = FontSettingSerif;
  } else if (sender == sansSerifButton_) {
    currentFont_ = sansSerifFont_.get();
    currentType_ = FontSettingSansSerif;
  } else if (sender == fixedWidthButton_) {
    currentFont_ = fixedWidthFont_.get();
    currentType_ = FontSettingFixed;
  } else {
    NOTREACHED();
  }

  // Validate whatever editing is currently happening.
  if ([[self window] makeFirstResponder:nil]) {
    NSFontManager* manager = [NSFontManager sharedFontManager];
    [manager setTarget:self];
    [manager setSelectedFont:currentFont_ isMultiple:NO];
    [manager orderFrontFontPanel:self];
  }
}

// Called by the font manager when the user has selected a new font. We should
// then persist those changes into the preference system.
- (void)changeFont:(id)fontManager {
  switch (currentType_) {
    case FontSettingSerif:
      serifFont_.reset([[fontManager convertFont:serifFont_] retain]);
      [self updateDisplayField:serifField_
                      withFont:serifFont_.get()
                     withLabel:serifLabel_];
      changedSerif_ = YES;
      break;
    case FontSettingSansSerif:
      sansSerifFont_.reset([[fontManager convertFont:sansSerifFont_] retain]);
      [self updateDisplayField:sansSerifField_
                      withFont:sansSerifFont_.get()
                     withLabel:sansSerifLabel_];
      changedSansSerif_ = YES;
      break;
    case FontSettingFixed:
      fixedWidthFont_.reset(
          [[fontManager convertFont:fixedWidthFont_] retain]);
      [self updateDisplayField:fixedWidthField_
                      withFont:fixedWidthFont_.get()
                     withLabel:fixedWidthLabel_];
      changedFixedWidth_ = YES;
      break;
    default:
      NOTREACHED();
  }
}

- (IBAction)closeSheet:(id)sender {
  NSFontPanel* panel = [[NSFontManager sharedFontManager] fontPanel:NO];
  [panel close];
  [NSApp endSheet:[self window]];
}

- (IBAction)save:(id)sender {
  if (changedSerif_) {
    serifName_.SetValue(base::SysNSStringToUTF8([serifFont_ fontName]));
    serifSize_.SetValue([serifFont_ pointSize]);
  }
  if (changedSansSerif_) {
    sansSerifName_.SetValue(
        base::SysNSStringToUTF8([sansSerifFont_ fontName]));
    sansSerifSize_.SetValue([sansSerifFont_ pointSize]);
  }
  if (changedFixedWidth_) {
    fixedWidthName_.SetValue(
        base::SysNSStringToUTF8([fixedWidthFont_ fontName]));
    fixedWidthSize_.SetValue([fixedWidthFont_ pointSize]);
  }
  if (changedEncoding_) {
    NSDictionary* object = [encodings_ objectAtIndex:defaultEncodingIndex_];
    NSString* newEncoding = [object objectForKey:kCharacterInfoEncoding];
    std::string encoding = base::SysNSStringToUTF8(newEncoding);
    defaultEncoding_.SetValue(encoding);
  }
  [self closeSheet:sender];
}

- (NSArray*)encodings {
  return encodings_.get();
}

// KVO notification.
- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  // If this is the default encoding, then set the flag to persist the value.
  if ([keyPath isEqual:@"defaultEncodingIndex_"]) {
    changedEncoding_ = YES;
    return;
  }

  [super observeValueForKeyPath:keyPath
                       ofObject:object
                         change:change
                        context:context];
}

#pragma mark Private

// Set the baseline for the font field to be aligned with the baseline
// of its corresponding label.
- (NSPoint)getFontFieldOrigin:(NSTextField*)field
                     forLabel:(NSTextField*)label {
  [field sizeToFit];
  NSRect labelFrame = [label frame];
  NSPoint newOrigin =
      [[label superview] convertPoint:labelFrame.origin
                               toView:[field superview]];
  newOrigin.x = 0;  // Left-align font field.
  newOrigin.y += [[field font] descender] - [[label font] descender];
  return newOrigin;
}

// This will set the font on |field| to be |font|, and will set the string
// value to something human-readable.
- (void)updateDisplayField:(NSTextField*)field
                  withFont:(NSFont*)font
                 withLabel:(NSTextField*)label {
  if (!font) {
    // Something has gone really wrong. Don't make things worse by showing the
    // user "(null)".
    return;
  }
  [field setFont:font];
  NSString* value =
      [NSString stringWithFormat:@"%@, %g", [font fontName], [font pointSize]];
  [field setStringValue:value];
  [field setFrameOrigin:[self getFontFieldOrigin:field forLabel:label]];
}

@end
