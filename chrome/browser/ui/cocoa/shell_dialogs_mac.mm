// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/shell_dialogs.h"

#import <Cocoa/Cocoa.h>
#include <CoreServices/CoreServices.h>

#include <map>
#include <set>
#include <vector>

#include "base/file_util.h"
#include "base/logging.h"
#import "base/mac/cocoa_protocols.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#import "base/scoped_nsobject.h"
#include "base/sys_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util_mac.h"

static const int kFileTypePopupTag = 1234;

class SelectFileDialogImpl;

// A bridge class to act as the modal delegate to the save/open sheet and send
// the results to the C++ class.
@interface SelectFileDialogBridge : NSObject<NSOpenSavePanelDelegate> {
 @private
  SelectFileDialogImpl* selectFileDialogImpl_;  // WEAK; owns us
}

- (id)initWithSelectFileDialogImpl:(SelectFileDialogImpl*)s;
- (void)endedPanel:(NSSavePanel*)panel
        withReturn:(int)returnCode
           context:(void*)context;

// NSSavePanel delegate method
- (BOOL)panel:(id)sender shouldShowFilename:(NSString *)filename;

@end

// Implementation of SelectFileDialog that shows Cocoa dialogs for choosing a
// file or folder.
class SelectFileDialogImpl : public SelectFileDialog {
 public:
  explicit SelectFileDialogImpl(Listener* listener);
  virtual ~SelectFileDialogImpl();

  // BaseShellDialog implementation.
  virtual bool IsRunning(gfx::NativeWindow parent_window) const;
  virtual void ListenerDestroyed();

  // SelectFileDialog implementation.
  // |params| is user data we pass back via the Listener interface.
  virtual void SelectFile(Type type,
                          const string16& title,
                          const FilePath& default_path,
                          const FileTypeInfo* file_types,
                          int file_type_index,
                          const FilePath::StringType& default_extension,
                          gfx::NativeWindow owning_window,
                          void* params);

  // Callback from ObjC bridge.
  void FileWasSelected(NSSavePanel* dialog,
                       NSWindow* parent_window,
                       bool was_cancelled,
                       bool is_multi,
                       const std::vector<FilePath>& files,
                       int index);

  bool ShouldEnableFilename(NSSavePanel* dialog, NSString* filename);

  struct SheetContext {
    Type type;
    NSWindow* owning_window;
  };

 private:
  // Gets the accessory view for the save dialog.
  NSView* GetAccessoryView(const FileTypeInfo* file_types,
                           int file_type_index);

  // The listener to be notified of selection completion.
  Listener* listener_;

  // The bridge for results from Cocoa to return to us.
  scoped_nsobject<SelectFileDialogBridge> bridge_;

  // A map from file dialogs to the |params| user data associated with them.
  std::map<NSSavePanel*, void*> params_map_;

  // The set of all parent windows for which we are currently running dialogs.
  std::set<NSWindow*> parents_;

  // A map from file dialogs to their types.
  std::map<NSSavePanel*, Type> type_map_;

  DISALLOW_COPY_AND_ASSIGN(SelectFileDialogImpl);
};

// static
SelectFileDialog* SelectFileDialog::Create(Listener* listener) {
  return new SelectFileDialogImpl(listener);
}

SelectFileDialogImpl::SelectFileDialogImpl(Listener* listener)
    : listener_(listener),
      bridge_([[SelectFileDialogBridge alloc]
               initWithSelectFileDialogImpl:this]) {
}

SelectFileDialogImpl::~SelectFileDialogImpl() {
  // Walk through the open dialogs and close them all.  Use a temporary vector
  // to hold the pointers, since we can't delete from the map as we're iterating
  // through it.
  std::vector<NSSavePanel*> panels;
  for (std::map<NSSavePanel*, void*>::iterator it = params_map_.begin();
       it != params_map_.end(); ++it) {
    panels.push_back(it->first);
  }

  for (std::vector<NSSavePanel*>::iterator it = panels.begin();
       it != panels.end(); ++it) {
    [*it cancel:*it];
  }
}

bool SelectFileDialogImpl::IsRunning(gfx::NativeWindow parent_window) const {
  return parents_.find(parent_window) != parents_.end();
}

void SelectFileDialogImpl::ListenerDestroyed() {
  listener_ = NULL;
}

void SelectFileDialogImpl::SelectFile(
    Type type,
    const string16& title,
    const FilePath& default_path,
    const FileTypeInfo* file_types,
    int file_type_index,
    const FilePath::StringType& default_extension,
    gfx::NativeWindow owning_window,
    void* params) {
  DCHECK(type == SELECT_FOLDER ||
         type == SELECT_OPEN_FILE ||
         type == SELECT_OPEN_MULTI_FILE ||
         type == SELECT_SAVEAS_FILE);
  parents_.insert(owning_window);

  // Note: we need to retain the dialog as owning_window can be null.
  // (see http://crbug.com/29213)
  NSSavePanel* dialog;
  if (type == SELECT_SAVEAS_FILE)
    dialog = [[NSSavePanel savePanel] retain];
  else
    dialog = [[NSOpenPanel openPanel] retain];

  if (!title.empty())
    [dialog setTitle:base::SysUTF16ToNSString(title)];

  NSString* default_dir = nil;
  NSString* default_filename = nil;
  if (!default_path.empty()) {
    // The file dialog is going to do a ton of stats anyway. Not much
    // point in eliminating this one.
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    if (file_util::DirectoryExists(default_path)) {
      default_dir = base::SysUTF8ToNSString(default_path.value());
    } else {
      default_dir = base::SysUTF8ToNSString(default_path.DirName().value());
      default_filename =
          base::SysUTF8ToNSString(default_path.BaseName().value());
    }
  }

  NSMutableArray* allowed_file_types = nil;
  if (file_types) {
    if (!file_types->extensions.empty()) {
      allowed_file_types = [NSMutableArray array];
      for (size_t i=0; i < file_types->extensions.size(); ++i) {
        const std::vector<FilePath::StringType>& ext_list =
            file_types->extensions[i];
        for (size_t j=0; j < ext_list.size(); ++j) {
          [allowed_file_types addObject:base::SysUTF8ToNSString(ext_list[j])];
        }
      }
    }
    if (type == SELECT_SAVEAS_FILE)
      [dialog setAllowedFileTypes:allowed_file_types];
    // else we'll pass it in when we run the open panel

    if (file_types->include_all_files)
      [dialog setAllowsOtherFileTypes:YES];

    if (!file_types->extension_description_overrides.empty()) {
      NSView* accessory_view = GetAccessoryView(file_types, file_type_index);
      [dialog setAccessoryView:accessory_view];
    }
  } else {
    // If no type info is specified, anything goes.
    [dialog setAllowsOtherFileTypes:YES];
  }

  if (!default_extension.empty())
    [dialog setRequiredFileType:base::SysUTF8ToNSString(default_extension)];

  params_map_[dialog] = params;
  type_map_[dialog] = type;

  SheetContext* context = new SheetContext;

  // |context| should never be NULL, but we are seeing indications otherwise.
  // This CHECK is here to confirm if we are actually getting NULL
  // |context|s. http://crbug.com/58959
  CHECK(context);
  context->type = type;
  context->owning_window = owning_window;

  if (type == SELECT_SAVEAS_FILE) {
    [dialog beginSheetForDirectory:default_dir
                              file:default_filename
                    modalForWindow:owning_window
                     modalDelegate:bridge_.get()
                    didEndSelector:@selector(endedPanel:withReturn:context:)
                       contextInfo:context];
  } else {
    NSOpenPanel* open_dialog = (NSOpenPanel*)dialog;

    if (type == SELECT_OPEN_MULTI_FILE)
      [open_dialog setAllowsMultipleSelection:YES];
    else
      [open_dialog setAllowsMultipleSelection:NO];

    if (type == SELECT_FOLDER) {
      [open_dialog setCanChooseFiles:NO];
      [open_dialog setCanChooseDirectories:YES];
      [open_dialog setCanCreateDirectories:YES];
      NSString *prompt = l10n_util::GetNSString(IDS_SELECT_FOLDER_BUTTON_TITLE);
      [open_dialog setPrompt:prompt];
    } else {
      [open_dialog setCanChooseFiles:YES];
      [open_dialog setCanChooseDirectories:NO];
    }

    [open_dialog setDelegate:bridge_.get()];
    [open_dialog beginSheetForDirectory:default_dir
                                   file:default_filename
                                  types:allowed_file_types
                         modalForWindow:owning_window
                          modalDelegate:bridge_.get()
                        didEndSelector:@selector(endedPanel:withReturn:context:)
                            contextInfo:context];
  }
}

void SelectFileDialogImpl::FileWasSelected(NSSavePanel* dialog,
                                           NSWindow* parent_window,
                                           bool was_cancelled,
                                           bool is_multi,
                                           const std::vector<FilePath>& files,
                                           int index) {
  void* params = params_map_[dialog];
  params_map_.erase(dialog);
  parents_.erase(parent_window);
  type_map_.erase(dialog);

  if (!listener_)
    return;

  if (was_cancelled) {
    listener_->FileSelectionCanceled(params);
  } else {
    if (is_multi) {
      listener_->MultiFilesSelected(files, params);
    } else {
      listener_->FileSelected(files[0], index, params);
    }
  }
}

NSView* SelectFileDialogImpl::GetAccessoryView(const FileTypeInfo* file_types,
                                               int file_type_index) {
  DCHECK(file_types);
  scoped_nsobject<NSNib> nib (
      [[NSNib alloc] initWithNibNamed:@"SaveAccessoryView"
                               bundle:base::mac::MainAppBundle()]);
  if (!nib)
    return nil;

  NSArray* objects;
  BOOL success = [nib instantiateNibWithOwner:nil
                              topLevelObjects:&objects];
  if (!success)
    return nil;
  [objects makeObjectsPerformSelector:@selector(release)];

  // This is a one-object nib, but IB insists on creating a second object, the
  // NSApplication. I don't know why.
  size_t view_index = 0;
  while (view_index < [objects count] &&
      ![[objects objectAtIndex:view_index] isKindOfClass:[NSView class]])
    ++view_index;
  DCHECK(view_index < [objects count]);
  NSView* accessory_view = [objects objectAtIndex:view_index];

  NSPopUpButton* popup = [accessory_view viewWithTag:kFileTypePopupTag];
  DCHECK(popup);

  size_t type_count = file_types->extensions.size();
  for (size_t type = 0; type<type_count; ++type) {
    NSString* type_description;
    if (type < file_types->extension_description_overrides.size()) {
      type_description = base::SysUTF16ToNSString(
          file_types->extension_description_overrides[type]);
    } else {
      const std::vector<FilePath::StringType>& ext_list =
          file_types->extensions[type];
      DCHECK(!ext_list.empty());
      NSString* type_extension = base::SysUTF8ToNSString(ext_list[0]);
      base::mac::ScopedCFTypeRef<CFStringRef> uti(
          UTTypeCreatePreferredIdentifierForTag(kUTTagClassFilenameExtension,
                                                (CFStringRef)type_extension,
                                                NULL));
      base::mac::ScopedCFTypeRef<CFStringRef> description(
          UTTypeCopyDescription(uti.get()));

      type_description =
          [NSString stringWithString:(NSString*)description.get()];
    }
    [popup addItemWithTitle:type_description];
  }

  [popup selectItemAtIndex:file_type_index-1];  // 1-based
  return accessory_view;
}

bool SelectFileDialogImpl::ShouldEnableFilename(NSSavePanel* dialog,
                                                NSString* filename) {
  // If this is a single open file dialog, disable selecting packages.
  if (type_map_[dialog] != SELECT_OPEN_FILE)
    return true;

  return ![[NSWorkspace sharedWorkspace] isFilePackageAtPath:filename];
}

@implementation SelectFileDialogBridge

- (id)initWithSelectFileDialogImpl:(SelectFileDialogImpl*)s {
  self = [super init];
  if (self != nil) {
    selectFileDialogImpl_ = s;
  }
  return self;
}

- (void)endedPanel:(NSSavePanel*)panel
        withReturn:(int)returnCode
           context:(void*)context {
  // |context| should never be NULL, but we are seeing indications otherwise.
  // |This CHECK is here to confirm if we are actually getting NULL
  // ||context|s. http://crbug.com/58959
  CHECK(context);

  int index = 0;
  SelectFileDialogImpl::SheetContext* context_struct =
      (SelectFileDialogImpl::SheetContext*)context;

  SelectFileDialog::Type type = context_struct->type;
  NSWindow* parentWindow = context_struct->owning_window;
  delete context_struct;

  bool isMulti = type == SelectFileDialog::SELECT_OPEN_MULTI_FILE;

  std::vector<FilePath> paths;
  bool did_cancel = returnCode == NSCancelButton;
  if (!did_cancel) {
    if (type == SelectFileDialog::SELECT_SAVEAS_FILE) {
      paths.push_back(FilePath(base::SysNSStringToUTF8([panel filename])));

      NSView* accessoryView = [panel accessoryView];
      if (accessoryView) {
        NSPopUpButton* popup = [accessoryView viewWithTag:kFileTypePopupTag];
        if (popup) {
          // File type indexes are 1-based.
          index = [popup indexOfSelectedItem] + 1;
        }
      } else {
        index = 1;
      }
    } else {
      CHECK([panel isKindOfClass:[NSOpenPanel class]]);
      NSArray* filenames = [static_cast<NSOpenPanel*>(panel) filenames];
      for (NSString* filename in filenames)
        paths.push_back(FilePath(base::SysNSStringToUTF8(filename)));
    }
  }

  selectFileDialogImpl_->FileWasSelected(panel,
                                         parentWindow,
                                         did_cancel,
                                         isMulti,
                                         paths,
                                         index);
  [panel release];
}

- (BOOL)panel:(id)sender shouldShowFilename:(NSString *)filename {
  return selectFileDialogImpl_->ShouldEnableFilename(sender, filename);
}

@end
