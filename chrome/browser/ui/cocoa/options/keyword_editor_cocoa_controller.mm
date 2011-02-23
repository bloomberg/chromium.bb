// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "chrome/browser/ui/cocoa/options/keyword_editor_cocoa_controller.h"

#import "base/mac/mac_util.h"
#include "base/lazy_instance.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_model.h"
#import "chrome/browser/ui/cocoa/options/edit_search_engine_cocoa_controller.h"
#import "chrome/browser/ui/cocoa/window_size_autosaver.h"
#include "chrome/browser/ui/search_engines/template_url_table_model.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"
#include "skia/ext/skia_utils_mac.h"
#include "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace {

const CGFloat kButtonBarHeight = 35.0;

}  // namespace

@interface KeywordEditorCocoaController (Private)
- (void)adjustEditingButtons;
- (void)editKeyword:(id)sender;
- (int)indexInModelForRow:(NSUInteger)row;
@end

// KeywordEditorModelObserver -------------------------------------------------

KeywordEditorModelObserver::KeywordEditorModelObserver(
    KeywordEditorCocoaController* controller)
    : controller_(controller),
      icon_cache_(this) {
}

KeywordEditorModelObserver::~KeywordEditorModelObserver() {
}

// Notification that the template url model has changed in some way.
void KeywordEditorModelObserver::OnTemplateURLModelChanged() {
  [controller_ modelChanged];
}

void KeywordEditorModelObserver::OnEditedKeyword(
    const TemplateURL* template_url,
    const string16& title,
    const string16& keyword,
    const std::string& url) {
  KeywordEditorController* controller = [controller_ controller];
  if (template_url) {
    controller->ModifyTemplateURL(template_url, title, keyword, url);
  } else {
    controller->AddTemplateURL(title, keyword, url);
  }
}

void KeywordEditorModelObserver::OnModelChanged() {
  icon_cache_.OnModelChanged();
  [controller_ modelChanged];
}

void KeywordEditorModelObserver::OnItemsChanged(int start, int length) {
  icon_cache_.OnItemsChanged(start, length);
  [controller_ modelChanged];
}

void KeywordEditorModelObserver::OnItemsAdded(int start, int length) {
  icon_cache_.OnItemsAdded(start, length);
  [controller_ modelChanged];
}

void KeywordEditorModelObserver::OnItemsRemoved(int start, int length) {
  icon_cache_.OnItemsRemoved(start, length);
  [controller_ modelChanged];
}

int KeywordEditorModelObserver::RowCount() const {
  return [controller_ controller]->table_model()->RowCount();
}

SkBitmap KeywordEditorModelObserver::GetIcon(int row) const {
  return [controller_ controller]->table_model()->GetIcon(row);
}

NSImage* KeywordEditorModelObserver::GetImageForRow(int row) {
  return icon_cache_.GetImageForRow(row);
}

// KeywordEditorCocoaController -----------------------------------------------

namespace {

typedef std::map<Profile*,KeywordEditorCocoaController*> ProfileControllerMap;

static base::LazyInstance<ProfileControllerMap> g_profile_controller_map(
    base::LINKER_INITIALIZED);

}  // namespace

@implementation KeywordEditorCocoaController

+ (KeywordEditorCocoaController*)sharedInstanceForProfile:(Profile*)profile {
  ProfileControllerMap* map = g_profile_controller_map.Pointer();
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

  ProfileControllerMap* map = g_profile_controller_map.Pointer();
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
  NSString* nibpath = [base::mac::MainAppBundle()
                        pathForResource:@"KeywordEditor"
                                 ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibpath owner:self])) {
    profile_ = profile;
    controller_.reset(new KeywordEditorController(profile_));
    observer_.reset(new KeywordEditorModelObserver(self));
    controller_->table_model()->SetObserver(observer_.get());
    controller_->url_model()->AddObserver(observer_.get());
    groupCell_.reset([[NSTextFieldCell alloc] init]);

    if (g_browser_process && g_browser_process->local_state()) {
      sizeSaver_.reset([[WindowSizeAutosaver alloc]
          initWithWindow:[self window]
             prefService:g_browser_process->local_state()
                    path:prefs::kKeywordEditorWindowPlacement]);
    }
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

  [[self window] setAutorecalculatesContentBorderThickness:NO
                                                   forEdge:NSMinYEdge];
  [[self window] setContentBorderThickness:kButtonBarHeight
                                   forEdge:NSMinYEdge];

  [self adjustEditingButtons];
  [tableView_ setDoubleAction:@selector(editKeyword:)];
  [tableView_ setTarget:self];
}

// When the window closes, clean ourselves up.
- (void)windowWillClose:(NSNotification*)notif {
  [self autorelease];

  ProfileControllerMap* map = g_profile_controller_map.Pointer();
  ProfileControllerMap::iterator it = map->find(profile_);
  // It should not be possible for this to be missing.
  // TODO(shess): Except that the unit test reaches in directly.
  // Consider circling around and refactoring that.
  //DCHECK(it != map->end());
  if (it != map->end()) {
    map->erase(it);
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

// Called when the user hits the escape key. Closes the window.
- (void)cancel:(id)sender {
  [[self window] performClose:self];
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
    ui::TableModel::Groups groups = controller_->table_model()->GetGroups();
    if (row == 0) {
      return base::SysUTF16ToNSString(groups[0].title);
    } else {
      return base::SysUTF16ToNSString(groups[1].title);
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
    string16 text = controller_->table_model()->GetText(index, columnID);
    return base::SysUTF16ToNSString(text);
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
    string16 title = controller_->table_model()->GetText(offsetRow,
        IDS_SEARCH_ENGINES_EDITOR_DESCRIPTION_COLUMN);
    [buttonCell setTitle:base::SysUTF16ToNSString(title)];
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
