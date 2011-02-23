// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "base/mac/cocoa_protocols.h"
#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "chrome/browser/search_engines/template_url_model_observer.h"
#include "chrome/browser/ui/cocoa/table_row_nsimage_cache.h"
#include "chrome/browser/ui/search_engines/edit_search_engine_controller.h"
#include "chrome/browser/ui/search_engines/keyword_editor_controller.h"
#include "ui/base/models/table_model_observer.h"

class EditSearchEngineControllerDelegate;
@class KeywordEditorCocoaController;
class Profile;
@class WindowSizeAutosaver;

// Very thin bridge that simply pushes notifications from C++ to ObjC.
class KeywordEditorModelObserver : public TemplateURLModelObserver,
                                   public EditSearchEngineControllerDelegate,
                                   public ui::TableModelObserver,
                                   public TableRowNSImageCache::Table {
 public:
  explicit KeywordEditorModelObserver(KeywordEditorCocoaController* controller);
  virtual ~KeywordEditorModelObserver();

  // Notification that the template url model has changed in some way.
  virtual void OnTemplateURLModelChanged();

  // Invoked from the EditSearchEngineController when the user accepts the
  // edits. NOTE: |template_url| is the value supplied to
  // EditSearchEngineController's constructor, and may be NULL. A NULL value
  // indicates a new TemplateURL should be created rather than modifying an
  // existing TemplateURL.
  virtual void OnEditedKeyword(const TemplateURL* template_url,
                               const string16& title,
                               const string16& keyword,
                               const std::string& url);

  // ui::TableModelObserver overrides. Invalidate icon cache.
  virtual void OnModelChanged();
  virtual void OnItemsChanged(int start, int length);
  virtual void OnItemsAdded(int start, int length);
  virtual void OnItemsRemoved(int start, int length);

  // TableRowNSImageCache::Table
  virtual int RowCount() const;
  virtual SkBitmap GetIcon(int row) const;

  // Lazily converts the image at the given row and caches it in |icon_cache_|.
  NSImage* GetImageForRow(int row);

 private:
  KeywordEditorCocoaController* controller_;

  TableRowNSImageCache icon_cache_;

  DISALLOW_COPY_AND_ASSIGN(KeywordEditorModelObserver);
};

// This controller manages a window with a table view of search engines. It
// acts as |tableView_|'s data source and delegate, feeding it data from the
// KeywordEditorController's |table_model()|.

@interface KeywordEditorCocoaController : NSWindowController
                                          <NSWindowDelegate,
                                           NSTableViewDataSource,
                                           NSTableViewDelegate> {
  IBOutlet NSTableView* tableView_;
  IBOutlet NSButton* addButton_;
  IBOutlet NSButton* removeButton_;
  IBOutlet NSButton* makeDefaultButton_;

  scoped_nsobject<NSTextFieldCell> groupCell_;

  Profile* profile_;  // weak
  scoped_ptr<KeywordEditorController> controller_;
  scoped_ptr<KeywordEditorModelObserver> observer_;

  scoped_nsobject<WindowSizeAutosaver> sizeSaver_;
}
@property(nonatomic, readonly) KeywordEditorController* controller;

// Show the keyword editor associated with the given profile (or the
// original profile if this is an incognito profile).  If no keyword
// editor exists for this profile, create one and show it.  Any
// resulting editor releases itself when closed.
+ (void)showKeywordEditor:(Profile*)profile;

- (KeywordEditorController*)controller;

// Message forwarded by KeywordEditorModelObserver.
- (void)modelChanged;

- (IBAction)addKeyword:(id)sender;
- (IBAction)deleteKeyword:(id)sender;
- (IBAction)makeDefault:(id)sender;

@end

@interface KeywordEditorCocoaController (TestingAPI)

// Instances of this class are managed, use +showKeywordEditor:.
- (id)initWithProfile:(Profile*)profile;

// Returns a reference to the shared instance for the given profile,
// or nil if there is none.
+ (KeywordEditorCocoaController*)sharedInstanceForProfile:(Profile*)profile;

// Converts a row index in our table view (which has group header rows) into
// one in the |controller_|'s model, which does not have them.
- (int)indexInModelForRow:(NSUInteger)row;

@end
