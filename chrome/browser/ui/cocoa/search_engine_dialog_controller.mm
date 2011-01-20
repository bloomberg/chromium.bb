// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/search_engine_dialog_controller.h"

#include <algorithm>

#include "app/l10n_util_mac.h"
#include "base/mac/mac_util.h"
#include "base/sys_string_conversions.h"
#include "base/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/search_engines/template_url_model_observer.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#import "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#include "ui/base/resource/resource_bundle.h"

// Horizontal spacing between search engine choices.
const int kSearchEngineSpacing = 20;

// Vertical spacing between the search engine logo and the button underneath.
const int kLogoButtonSpacing = 10;

// Width of a label used in place of a logo.
const int kLogoLabelWidth = 170;

// Height of a label used in place of a logo.
const int kLogoLabelHeight = 25;

@interface SearchEngineDialogController (Private)
- (void)onTemplateURLModelChanged;
- (void)buildSearchEngineView;
- (NSView*)viewForSearchEngine:(const TemplateURL*)engine
                       atIndex:(size_t)index;
- (IBAction)searchEngineSelected:(id)sender;
@end

class SearchEngineDialogControllerBridge :
    public base::RefCounted<SearchEngineDialogControllerBridge>,
    public TemplateURLModelObserver {
 public:
  SearchEngineDialogControllerBridge(SearchEngineDialogController* controller);

  // TemplateURLModelObserver
  virtual void OnTemplateURLModelChanged();

 private:
  SearchEngineDialogController* controller_;
};

SearchEngineDialogControllerBridge::SearchEngineDialogControllerBridge(
    SearchEngineDialogController* controller) : controller_(controller) {
}

void SearchEngineDialogControllerBridge::OnTemplateURLModelChanged() {
  [controller_ onTemplateURLModelChanged];
  MessageLoop::current()->QuitNow();
}

@implementation SearchEngineDialogController

@synthesize profile = profile_;
@synthesize randomize = randomize_;

- (id)init {
  NSString* nibpath =
      [base::mac::MainAppBundle() pathForResource:@"SearchEngineDialog"
                                          ofType:@"nib"];
  self = [super initWithWindowNibPath:nibpath owner:self];
  if (self != nil) {
    bridge_ = new SearchEngineDialogControllerBridge(self);
  }
  return self;
}

- (void)dealloc {
  [super dealloc];
}

- (IBAction)showWindow:(id)sender {
  searchEnginesModel_ = profile_->GetTemplateURLModel();
  searchEnginesModel_->AddObserver(bridge_.get());

  if (searchEnginesModel_->loaded()) {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        NewRunnableMethod(
            bridge_.get(),
            &SearchEngineDialogControllerBridge::OnTemplateURLModelChanged));
  } else {
    searchEnginesModel_->Load();
  }
  MessageLoop::current()->Run();
}

- (void)onTemplateURLModelChanged {
  searchEnginesModel_->RemoveObserver(bridge_.get());

  // Add the search engines in the search_engines_model_ to the buttons list.
  // The first three will always be from prepopulated data.
  std::vector<const TemplateURL*> templateUrls =
      searchEnginesModel_->GetTemplateURLs();

  // If we have fewer than two search engines, end the search engine dialog
  // immediately, leaving the imported default search engine setting intact.
  if (templateUrls.size() < 2) {
    return;
  }

  NSWindow* win = [self window];

  [win setBackgroundColor:[NSColor whiteColor]];

  NSImage* headerImage = ResourceBundle::GetSharedInstance().
      GetNativeImageNamed(IDR_SEARCH_ENGINE_DIALOG_TOP);
  [headerImageView_ setImage:headerImage];

  // Is the user's default search engine included in the first three
  // prepopulated set? If not, we need to expand the dialog to include a fourth
  // engine.
  const TemplateURL* defaultSearchEngine =
      searchEnginesModel_->GetDefaultSearchProvider();

  std::vector<const TemplateURL*>::iterator engineIter =
      templateUrls.begin();
  for (int i = 0; engineIter != templateUrls.end(); ++i, ++engineIter) {
    if (i < 3) {
      choices_.push_back(*engineIter);
    } else {
      if (*engineIter == defaultSearchEngine)
        choices_.push_back(*engineIter);
    }
  }

  // Randomize the order of the logos if the option has been set.
  if (randomize_) {
    int seed = static_cast<int>(base::Time::Now().ToInternalValue());
    srand(seed);
    std::random_shuffle(choices_.begin(), choices_.end());
  }

  [self buildSearchEngineView];

  // Display the dialog.
  NSInteger choice = [NSApp runModalForWindow:win];
  searchEnginesModel_->SetDefaultSearchProvider(choices_.at(choice));
}

- (void)buildSearchEngineView {
  scoped_nsobject<NSMutableArray> searchEngineViews
      ([[NSMutableArray alloc] init]);

  for (size_t i = 0; i < choices_.size(); ++i)
    [searchEngineViews addObject:[self viewForSearchEngine:choices_.at(i)
                                                   atIndex:i]];

  NSSize newOverallSize = NSZeroSize;
  for (NSView* view in searchEngineViews.get()) {
    NSRect engineFrame = [view frame];
    engineFrame.origin = NSMakePoint(newOverallSize.width, 0);
    [searchEngineView_ addSubview:view];
    [view setFrame:engineFrame];
    newOverallSize = NSMakeSize(
        newOverallSize.width + NSWidth(engineFrame) + kSearchEngineSpacing,
        std::max(newOverallSize.height, NSHeight(engineFrame)));
  }
  newOverallSize.width -= kSearchEngineSpacing;

  // Resize the window to fit (and because it's bound on all sides it will
  // resize the search engine view).
  NSSize currentOverallSize = [searchEngineView_ bounds].size;
  NSSize deltaSize = NSMakeSize(
      newOverallSize.width - currentOverallSize.width,
      newOverallSize.height - currentOverallSize.height);
  NSSize windowDeltaSize = [searchEngineView_ convertSize:deltaSize toView:nil];
  NSRect windowFrame = [[self window] frame];
  windowFrame.size.width += windowDeltaSize.width;
  windowFrame.size.height += windowDeltaSize.height;
  [[self window] setFrame:windowFrame display:NO];
}

- (NSView*)viewForSearchEngine:(const TemplateURL*)engine
                       atIndex:(size_t)index {
  bool useImages = false;
#if defined(GOOGLE_CHROME_BUILD)
  useImages = true;
#endif

  // Make the engine identifier.
  NSView* engineIdentifier = nil;  // either the logo or the text label

  int logoId = engine->logo_id();
  if (useImages && logoId > 0) {
    NSImage* logoImage =
        ResourceBundle::GetSharedInstance().GetNativeImageNamed(logoId);
    NSRect logoBounds = NSZeroRect;
    logoBounds.size = [logoImage size];
    NSImageView* logoView =
        [[[NSImageView alloc] initWithFrame:logoBounds] autorelease];
    [logoView setImage:logoImage];
    [logoView setEditable:NO];

    // Tooltip text provides accessibility.
    [logoView setToolTip:base::SysUTF16ToNSString(engine->short_name())];
    engineIdentifier = logoView;
  } else {
    // No logo -- we must show a text label.
    NSRect labelBounds = NSMakeRect(0, 0, kLogoLabelWidth, kLogoLabelHeight);
    NSTextField* labelField =
        [[[NSTextField alloc] initWithFrame:labelBounds] autorelease];
    [labelField setBezeled:NO];
    [labelField setEditable:NO];
    [labelField setSelectable:NO];

    scoped_nsobject<NSMutableParagraphStyle> paragraphStyle(
        [[NSMutableParagraphStyle alloc] init]);
    [paragraphStyle setAlignment:NSCenterTextAlignment];
    NSDictionary* attrs = [NSDictionary dictionaryWithObjectsAndKeys:
        [NSFont boldSystemFontOfSize:13], NSFontAttributeName,
        paragraphStyle.get(), NSParagraphStyleAttributeName,
        nil];

    NSString* value = base::SysUTF16ToNSString(engine->short_name());
    scoped_nsobject<NSAttributedString> attrValue(
        [[NSAttributedString alloc] initWithString:value
                                        attributes:attrs]);

    [labelField setAttributedStringValue:attrValue.get()];

    engineIdentifier = labelField;
  }

  // Make the "Choose" button.
  scoped_nsobject<NSButton> chooseButton(
      [[NSButton alloc] initWithFrame:NSMakeRect(0, 0, 100, 34)]);
  [chooseButton setBezelStyle:NSRoundedBezelStyle];
  [[chooseButton cell] setFont:[NSFont systemFontOfSize:
      [NSFont systemFontSizeForControlSize:NSRegularControlSize]]];
  [chooseButton setTitle:l10n_util::GetNSStringWithFixup(IDS_FR_SEARCH_CHOOSE)];
  [GTMUILocalizerAndLayoutTweaker sizeToFitView:chooseButton.get()];
  [chooseButton setTag:index];
  [chooseButton setTarget:self];
  [chooseButton setAction:@selector(searchEngineSelected:)];

  // Put 'em together.
  NSRect engineIdentifierFrame = [engineIdentifier frame];
  NSRect chooseButtonFrame = [chooseButton frame];

  NSRect containingViewFrame = NSZeroRect;
  containingViewFrame.size.width += engineIdentifierFrame.size.width;
  containingViewFrame.size.height += engineIdentifierFrame.size.height;
  containingViewFrame.size.height += kLogoButtonSpacing;
  containingViewFrame.size.height += chooseButtonFrame.size.height;

  NSView* containingView =
      [[[NSView alloc] initWithFrame:containingViewFrame] autorelease];

  [containingView addSubview:engineIdentifier];
  engineIdentifierFrame.origin.y =
      chooseButtonFrame.size.height + kLogoButtonSpacing;
  [engineIdentifier setFrame:engineIdentifierFrame];

  [containingView addSubview:chooseButton];
  chooseButtonFrame.origin.x =
      int((containingViewFrame.size.width - chooseButtonFrame.size.width) / 2);
  [chooseButton setFrame:chooseButtonFrame];

  return containingView;
}

- (NSFont*)mainLabelFont {
  return [NSFont boldSystemFontOfSize:13];
}

- (IBAction)searchEngineSelected:(id)sender {
  [[self window] close];
  [NSApp stopModalWithCode:[sender tag]];
}

@end
