// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "base/mac_util.h"
#include "base/sys_string_conversions.h"
#import "chrome/browser/cocoa/edit_search_engine_cocoa_controller.h"
#import "chrome/browser/cocoa/keyword_editor_cocoa_controller.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/search_engines/template_url_table_model.h"
#include "grit/generated_resources.h"

@interface KeywordEditorCocoaController (Private)
- (void)adjustEditingButtons;
- (void)editKeyword:(id)sender;
@end

// KeywordEditorModelObserver -------------------------------------------------

KeywordEditorModelObserver::KeywordEditorModelObserver(
    KeywordEditorCocoaController* controller) : controller_(controller) {
}

KeywordEditorModelObserver::~KeywordEditorModelObserver() {
}

// Notification that the template url model has changed in some way.
void KeywordEditorModelObserver::OnTemplateURLModelChanged() {
  [controller_ modelChanged];
}

void KeywordEditorModelObserver::OnEditedKeyword(
    const TemplateURL* template_url,
    const std::wstring& title,
    const std::wstring& keyword,
    const std::wstring& url) {
  KeywordEditorController* controller = [controller_ controller];
  if (template_url) {
    controller->ModifyTemplateURL(template_url, title, keyword, url);
  } else {
    controller->AddTemplateURL(title, keyword, url);
  }
}

// KeywordEditorCocoaController -----------------------------------------------

@implementation KeywordEditorCocoaController

- (id)initWithProfile:(Profile*)profile {
  DCHECK(profile);
  NSString* nibpath = [mac_util::MainAppBundle()
                        pathForResource:@"KeywordEditor"
                                 ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibpath owner:self])) {
    profile_ = profile;
    controller_.reset(new KeywordEditorController(profile_));
    observer_.reset(new KeywordEditorModelObserver(self));
    controller_->url_model()->AddObserver(observer_.get());
  }
  return self;
}

- (void)dealloc {
  controller_->url_model()->RemoveObserver(observer_.get());
  [super dealloc];
}

- (void)awakeFromNib {
  [self adjustEditingButtons];
  [tableView_ setDoubleAction:@selector(editKeyword:)];
  [tableView_ setTarget:self];
}

// When the window closes, clean ourselves up.
- (void)windowWillClose:(NSNotification*)notif {
  [self autorelease];
}

- (void)modelChanged {
  [tableView_ reloadData];
}

- (KeywordEditorController*)controller {
  return controller_.get();
}

- (IBAction)addKeyword:(id)sender {
  // The controller will release itself when the window closes.
  EditSearchEngineCocoaController* editor =
      [[EditSearchEngineCocoaController alloc] initWithProfile:profile_
                                                      delegate:observer_.get()
                                                   templateURL:NULL];
  [[editor window] makeKeyAndOrderFront:self];
}

- (void)editKeyword:(id)sender {
  const NSInteger clickedRow = [tableView_ clickedRow];
  if (clickedRow == -1)
    return;
  const TemplateURL* url = controller_->GetTemplateURL(clickedRow);
  // The controller will release itself when the window closes.
  EditSearchEngineCocoaController* editor =
      [[EditSearchEngineCocoaController alloc] initWithProfile:profile_
                                                      delegate:observer_.get()
                                                   templateURL:url];
  [[editor window] makeKeyAndOrderFront:self];
}

- (IBAction)deleteKeyword:(id)sender {
  NSIndexSet* selection = [tableView_ selectedRowIndexes];
  DCHECK_GT([selection count], 0U);
  NSUInteger index = [selection lastIndex];
  while (index != NSNotFound) {
    controller_->RemoveTemplateURL(index);
    index = [selection indexLessThanIndex:index];
  }
}

- (IBAction)makeDefault:(id)sender {
  NSIndexSet* selection = [tableView_ selectedRowIndexes];
  DCHECK_EQ([selection count], 1U);
  controller_->MakeDefaultTemplateURL([selection firstIndex]);
}

// Table View Data Source -----------------------------------------------------

- (NSInteger)numberOfRowsInTableView:(NSTableView*)aTableView {
  return controller_->table_model()->RowCount();
}

- (id)tableView:(NSTableView*)tv
objectValueForTableColumn:(NSTableColumn*)tableColumn
            row:(NSInteger)row {
  NSString* identifier = [tableColumn identifier];
  int columnID = -1;
  if ([identifier isEqualToString:@"name"]) {
    columnID = IDS_SEARCH_ENGINES_EDITOR_DESCRIPTION_COLUMN;
  } else if ([identifier isEqualToString:@"keyword"]) {
    columnID = IDS_SEARCH_ENGINES_EDITOR_KEYWORD_COLUMN;
  }
  DCHECK_NE(columnID, -1);
  std::wstring text = controller_->table_model()->GetText(row, columnID);
  return base::SysWideToNSString(text);
}

// Table View Delegate --------------------------------------------------------

// When the selection in the table view changes, we need to adjust buttons.
- (void)tableViewSelectionDidChange:(NSNotification*)aNotification {
  [self adjustEditingButtons];
}

- (NSCell*)tableView:(NSTableView *)tableView
dataCellForTableColumn:(NSTableColumn*)tableColumn
                 row:(NSInteger)row {
  static const CGFloat kCellFontSize = 12.0;
  NSCell* cell = [tableColumn dataCellForRow:row];
  const TemplateURL* defaultEngine =
      controller_->url_model()->GetDefaultSearchProvider();
  if (controller_->table_model()->IndexOfTemplateURL(defaultEngine) == row) {
    [cell setFont:[NSFont boldSystemFontOfSize:kCellFontSize]];
  } else {
    [cell setFont:[NSFont systemFontOfSize:kCellFontSize]];
  }
  return cell;
}

// Private --------------------------------------------------------------------

// This function appropriately sets the enabled states on the table's editing
// buttons.
- (void)adjustEditingButtons {
  // Delete button.
  NSIndexSet* selection = [tableView_ selectedRowIndexes];
  BOOL canRemove = ([selection count] > 0);
  NSUInteger index = [selection firstIndex];
  while (canRemove && index != NSNotFound) {
    const TemplateURL& url =
        controller_->table_model()->GetTemplateURL(index);
    if (!controller_->CanRemove(&url))
      canRemove = NO;
    index = [selection indexGreaterThanIndex:index];
  }
  [removeButton_ setEnabled:canRemove];

  // Make default button.
  if ([selection count] != 1) {
    [makeDefaultButton_ setEnabled:NO];
  } else {
    const TemplateURL& url =
        controller_->table_model()->GetTemplateURL([selection firstIndex]);
    [makeDefaultButton_ setEnabled:controller_->CanMakeDefault(&url)];
  }
}

@end
