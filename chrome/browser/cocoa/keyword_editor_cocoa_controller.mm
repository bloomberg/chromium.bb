// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "base/mac_util.h"
#include "base/singleton.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/browser_process.h"
#import "chrome/browser/cocoa/edit_search_engine_cocoa_controller.h"
#import "chrome/browser/cocoa/nswindow_local_state.h"
#import "chrome/browser/cocoa/keyword_editor_cocoa_controller.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/search_engines/template_url_table_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "grit/generated_resources.h"
#include "skia/ext/skia_utils_mac.h"
#include "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"

@interface KeywordEditorCocoaController (Private)
- (void)adjustEditingButtons;
- (void)editKeyword:(id)sender;
- (int)indexInModelForRow:(NSUInteger)row;
@end

// KeywordEditorModelObserver -------------------------------------------------

KeywordEditorModelObserver::KeywordEditorModelObserver(
    KeywordEditorCocoaController* controller)
    : controller_(controller),
      iconImages_([[NSPointerArray alloc] initWithOptions:
          NSPointerFunctionsStrongMemory |
          NSPointerFunctionsObjectPersonality]) {
  int count = [controller_ controller]->table_model()->RowCount();
  [iconImages_ setCount:count];
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

void KeywordEditorModelObserver::OnModelChanged() {
  int count = [controller_ controller]->table_model()->RowCount();
  [iconImages_ setCount:0];
  [iconImages_ setCount:count];
  [controller_ modelChanged];
}

void KeywordEditorModelObserver::OnItemsChanged(int start, int length) {
  DCHECK_LE(start + length, static_cast<int>([iconImages_ count]));
  for (int i = start; i < (start + length); ++i) {
    [iconImages_ replacePointerAtIndex:i withPointer:NULL];
  }
  DCHECK_EQ([controller_ controller]->table_model()->RowCount(),
            static_cast<int>([iconImages_ count]));
  [controller_ modelChanged];
}

void KeywordEditorModelObserver::OnItemsAdded(int start, int length) {
  DCHECK_LE(start, static_cast<int>([iconImages_ count]));

  // -[NSPointerArray insertPointer:atIndex:] throws if index == count.
  // Instead expand the array with NULLs.
  if (start == static_cast<int>([iconImages_ count])) {
    [iconImages_ setCount:start + length];
  } else {
    for (int i = 0; i < length; ++i) {
      [iconImages_ insertPointer:NULL atIndex:start];  // Values slide up.
    }
  }
  DCHECK_EQ([controller_ controller]->table_model()->RowCount(),
            static_cast<int>([iconImages_ count]));
  [controller_ modelChanged];
}

void KeywordEditorModelObserver::OnItemsRemoved(int start, int length) {
  DCHECK_LE(start + length, static_cast<int>([iconImages_ count]));
  for (int i = 0; i < length; ++i) {
    [iconImages_ removePointerAtIndex:start];  // Values slide down.
  }
  DCHECK_EQ([controller_ controller]->table_model()->RowCount(),
            static_cast<int>([iconImages_ count]));
  [controller_ modelChanged];
}

NSImage* KeywordEditorModelObserver::GetImageForRow(int row) {
  DCHECK_EQ([controller_ controller]->table_model()->RowCount(),
            static_cast<int>([iconImages_ count]));
  DCHECK_GE(row, 0);
  DCHECK_LT(row, static_cast<int>([iconImages_ count]));
  NSImage* image = static_cast<NSImage*>([iconImages_ pointerAtIndex:row]);
  if (!image) {
    const SkBitmap bitmapIcon =
        [controller_ controller]->table_model()->GetIcon(row);
    if (!bitmapIcon.isNull()) {
      image = gfx::SkBitmapToNSImage(bitmapIcon);
      DCHECK(image);
      [iconImages_ replacePointerAtIndex:row withPointer:image];
    }
  }
  return image;
}

// KeywordEditorCocoaController -----------------------------------------------

namespace {

typedef std::map<Profile*,KeywordEditorCocoaController*> ProfileControllerMap;

}  // namespace

@implementation KeywordEditorCocoaController

+ (KeywordEditorCocoaController*)sharedInstanceForProfile:(Profile*)profile {
  ProfileControllerMap* map = Singleton<ProfileControllerMap>::get();
  DCHECK(map != NULL);
  ProfileControllerMap::iterator it = map->find(profile);
  if (it != map->end()) {
    return it->second;
  }
  return nil;
}

// TODO(shess): The Windows code watches a single global window which
// is not distinguished by profile.  This code could distinguish by
// profile by checking the controller's class and profile.
+ (void)showKeywordEditor:(Profile*)profile {
  // http://crbug.com/23359 describes a case where this panel is
  // opened from an incognito window, which can leave the panel
  // holding onto a stale profile.  Since the same panel is used
  // either way, arrange to use the original profile instead.
  profile = profile->GetOriginalProfile();

  ProfileControllerMap* map = Singleton<ProfileControllerMap>::get();
  DCHECK(map != NULL);
  ProfileControllerMap::iterator it = map->find(profile);
  if (it == map->end()) {
    // Since we don't currently support multiple profiles, this class
    // has not been tested against them, so document that assumption.
    DCHECK_EQ(map->size(), 0U);

    KeywordEditorCocoaController* controller =
        [[self alloc] initWithProfile:profile];
    it = map->insert(std::make_pair(profile, controller)).first;
  }

  [it->second showWindow:nil];
}

- (id)initWithProfile:(Profile*)profile {
  DCHECK(profile);
  NSString* nibpath = [mac_util::MainAppBundle()
                        pathForResource:@"KeywordEditor"
                                 ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibpath owner:self])) {
    profile_ = profile;
    controller_.reset(new KeywordEditorController(profile_));
    observer_.reset(new KeywordEditorModelObserver(self));
    controller_->table_model()->SetObserver(observer_.get());
    controller_->url_model()->AddObserver(observer_.get());
    groupCell_.reset([[NSTextFieldCell alloc] init]);
  }
  return self;
}

- (void)dealloc {
  controller_->table_model()->SetObserver(NULL);
  controller_->url_model()->RemoveObserver(observer_.get());
  [tableView_ setDataSource:nil];
  observer_.reset();
  [super dealloc];
}

- (void)awakeFromNib {
  // Make sure the button fits its label, but keep it the same height as the
  // other two buttons.
  [GTMUILocalizerAndLayoutTweaker sizeToFitView:makeDefaultButton_];
  NSSize size = [makeDefaultButton_ frame].size;
  size.height = NSHeight([addButton_ frame]);
  [makeDefaultButton_ setFrameSize:size];

  // Restore the window position.
  if (g_browser_process && g_browser_process->local_state()) {
    PrefService* prefs = g_browser_process->local_state();
    NSWindow* window = [self window];
    [window restoreWindowPositionFromPrefs:prefs
                                withPath:prefs::kKeywordEditorWindowPlacement];
  }

  [self adjustEditingButtons];
  [tableView_ setDoubleAction:@selector(editKeyword:)];
  [tableView_ setTarget:self];
}

// When the window closes, clean ourselves up.
- (void)windowWillClose:(NSNotification*)notif {
  [self autorelease];

  ProfileControllerMap* map = Singleton<ProfileControllerMap>::get();
  ProfileControllerMap::iterator it = map->find(profile_);
  // It should not be possible for this to be missing.
  // TODO(shess): Except that the unit test reaches in directly.
  // Consider circling around and refactoring that.
  //DCHECK(it != map->end());
  if (it != map->end()) {
    map->erase(it);
  }
}

// Remeber the position of the keyword editor.
- (void)windowDidMove:(NSNotification*)notif {
  if (g_browser_process && g_browser_process->local_state()) {
    NSWindow* window = [self window];
    [window saveWindowPositionToPrefs:g_browser_process->local_state()
                             withPath:prefs::kKeywordEditorWindowPlacement];
  }
}

- (void)modelChanged {
  [tableView_ reloadData];
  [self adjustEditingButtons];
}

- (KeywordEditorController*)controller {
  return controller_.get();
}

- (void)sheetDidEnd:(NSWindow*)sheet
         returnCode:(NSInteger)code
            context:(void*)context {
  [sheet orderOut:self];
}

- (IBAction)addKeyword:(id)sender {
  // The controller will release itself when the window closes.
  EditSearchEngineCocoaController* editor =
      [[EditSearchEngineCocoaController alloc] initWithProfile:profile_
                                                      delegate:observer_.get()
                                                   templateURL:NULL];
  [NSApp beginSheet:[editor window]
     modalForWindow:[self window]
      modalDelegate:self
     didEndSelector:@selector(sheetDidEnd:returnCode:context:)
        contextInfo:NULL];
}

- (void)editKeyword:(id)sender {
  const NSInteger clickedRow = [tableView_ clickedRow];
  if (clickedRow < 0 || [self tableView:tableView_ isGroupRow:clickedRow])
    return;
  const TemplateURL* url = controller_->GetTemplateURL(
      [self indexInModelForRow:clickedRow]);
  // The controller will release itself when the window closes.
  EditSearchEngineCocoaController* editor =
      [[EditSearchEngineCocoaController alloc] initWithProfile:profile_
                                                      delegate:observer_.get()
                                                   templateURL:url];
  [NSApp beginSheet:[editor window]
     modalForWindow:[self window]
      modalDelegate:self
     didEndSelector:@selector(sheetDidEnd:returnCode:context:)
        contextInfo:NULL];
}

- (IBAction)deleteKeyword:(id)sender {
  NSIndexSet* selection = [tableView_ selectedRowIndexes];
  DCHECK_GT([selection count], 0U);
  NSUInteger index = [selection lastIndex];
  while (index != NSNotFound) {
    controller_->RemoveTemplateURL([self indexInModelForRow:index]);
    index = [selection indexLessThanIndex:index];
  }
}

- (IBAction)makeDefault:(id)sender {
  NSIndexSet* selection = [tableView_ selectedRowIndexes];
  DCHECK_EQ([selection count], 1U);
  int row = [self indexInModelForRow:[selection firstIndex]];
  controller_->MakeDefaultTemplateURL(row);
}

// Table View Data Source -----------------------------------------------------

- (NSInteger)numberOfRowsInTableView:(NSTableView*)table {
  int rowCount = controller_->table_model()->RowCount();
  int numGroups = controller_->table_model()->GetGroups().size();
  if ([self tableView:table isGroupRow:rowCount + numGroups - 1]) {
    // Don't show a group header with no rows underneath it.
    --numGroups;
  }
  return rowCount + numGroups;
}

- (id)tableView:(NSTableView*)tv
    objectValueForTableColumn:(NSTableColumn*)tableColumn
                          row:(NSInteger)row {
  if ([self tableView:tv isGroupRow:row]) {
    DCHECK(!tableColumn);
    TableModel::Groups groups = controller_->table_model()->GetGroups();
    if (row == 0) {
      return base::SysWideToNSString(groups[0].title);
    } else {
      return base::SysWideToNSString(groups[1].title);
    }
  }

  NSString* identifier = [tableColumn identifier];
  if ([identifier isEqualToString:@"name"]) {
    // The name column is an NSButtonCell so we can have text and image in the
    // same cell. As such, the "object value" for a button cell is either on
    // or off, so we always return off so we don't act like a button.
    return [NSNumber numberWithInt:NSOffState];
  }
  if ([identifier isEqualToString:@"keyword"]) {
    // The keyword object value is a normal string.
    int index = [self indexInModelForRow:row];
    int columnID = IDS_SEARCH_ENGINES_EDITOR_KEYWORD_COLUMN;
    std::wstring text = controller_->table_model()->GetText(index, columnID);
    return base::SysWideToNSString(text);
  }

  // And we shouldn't have any other columns...
  NOTREACHED();
  return nil;
}

// Table View Delegate --------------------------------------------------------

// When the selection in the table view changes, we need to adjust buttons.
- (void)tableViewSelectionDidChange:(NSNotification*)aNotification {
  [self adjustEditingButtons];
}

// Disallow selection of the group header rows.
- (BOOL)tableView:(NSTableView*)table shouldSelectRow:(NSInteger)row {
  return ![self tableView:table isGroupRow:row];
}

- (BOOL)tableView:(NSTableView*)table isGroupRow:(NSInteger)row {
  int otherGroupRow =
      controller_->table_model()->last_search_engine_index() + 1;
  return (row == 0 || row == otherGroupRow);
}

- (NSCell*)tableView:(NSTableView*)tableView
    dataCellForTableColumn:(NSTableColumn*)tableColumn
                       row:(NSInteger)row {
  static const CGFloat kCellFontSize = 12.0;

  // Check to see if we are a grouped row.
  if ([self tableView:tableView isGroupRow:row]) {
    DCHECK(!tableColumn);  // This would violate the group row contract.
    return groupCell_.get();
  }

  NSCell* cell = [tableColumn dataCellForRow:row];
  int offsetRow = [self indexInModelForRow:row];

  // Set the favicon and title for the search engine in the name column.
  if ([[tableColumn identifier] isEqualToString:@"name"]) {
    DCHECK([cell isKindOfClass:[NSButtonCell class]]);
    NSButtonCell* buttonCell = static_cast<NSButtonCell*>(cell);
    std::wstring title = controller_->table_model()->GetText(offsetRow,
        IDS_SEARCH_ENGINES_EDITOR_DESCRIPTION_COLUMN);
    [buttonCell setTitle:base::SysWideToNSString(title)];
    [buttonCell setImage:observer_->GetImageForRow(offsetRow)];
    [buttonCell setRefusesFirstResponder:YES];  // Don't push in like a button.
    [buttonCell setHighlightsBy:NSNoCellMask];
  }

  // The default search engine should be in bold font.
  const TemplateURL* defaultEngine =
      controller_->url_model()->GetDefaultSearchProvider();
  int rowIndex = controller_->table_model()->IndexOfTemplateURL(defaultEngine);
  if (rowIndex == offsetRow) {
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
  NSIndexSet* selection = [tableView_ selectedRowIndexes];
  BOOL canRemove = ([selection count] > 0);
  NSUInteger index = [selection firstIndex];

  // Delete button.
  while (canRemove && index != NSNotFound) {
    int modelIndex = [self indexInModelForRow:index];
    const TemplateURL& url =
        controller_->table_model()->GetTemplateURL(modelIndex);
    if (!controller_->CanRemove(&url))
      canRemove = NO;
    index = [selection indexGreaterThanIndex:index];
  }
  [removeButton_ setEnabled:canRemove];

  // Make default button.
  if ([selection count] != 1) {
    [makeDefaultButton_ setEnabled:NO];
  } else {
    int row = [self indexInModelForRow:[selection firstIndex]];
    const TemplateURL& url =
        controller_->table_model()->GetTemplateURL(row);
    [makeDefaultButton_ setEnabled:controller_->CanMakeDefault(&url)];
  }
}

// This converts a row index in our table view to an index in the model by
// computing the group offsets.
- (int)indexInModelForRow:(NSUInteger)row {
  DCHECK_GT(row, 0U);
  unsigned otherGroupId =
      controller_->table_model()->last_search_engine_index() + 1;
  DCHECK_NE(row, otherGroupId);
  if (row >= otherGroupId) {
    return row - 2;  // Other group.
  } else {
    return row - 1;  // Default group.
  }
}

@end
