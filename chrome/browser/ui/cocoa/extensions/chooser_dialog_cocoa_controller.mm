// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/extensions/chooser_dialog_cocoa_controller.h"

#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "chrome/browser/ui/cocoa/chooser_content_view.h"
#import "chrome/browser/ui/cocoa/extensions/chooser_dialog_cocoa.h"
#include "chrome/grit/generated_resources.h"
#include "components/chooser_controller/chooser_controller.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#import "ui/base/l10n/l10n_util_mac.h"
#include "url/gurl.h"
#include "url/origin.h"

@implementation ChooserDialogCocoaController

- (instancetype)
initWithChooserDialogCocoa:(ChooserDialogCocoa*)chooserDialogCocoa
         chooserController:(ChooserController*)chooserController {
  DCHECK(chooserDialogCocoa);
  DCHECK(chooserController);
  if ((self = [super init])) {
    chooserDialogCocoa_ = chooserDialogCocoa;
    chooserController_ = chooserController;
  }

  base::string16 chooserTitle;
  url::Origin origin = chooserController_->GetOrigin();
  content::WebContents* web_contents = chooserDialogCocoa_->web_contents();
  content::BrowserContext* browser_context = web_contents->GetBrowserContext();
  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(browser_context);
  if (extension_registry) {
    const extensions::Extension* extension =
        extension_registry->enabled_extensions().GetExtensionOrAppByURL(
            GURL(origin.Serialize()));
    if (extension)
      chooserTitle = base::UTF8ToUTF16(extension->name());
  }

  if (chooserTitle.empty()) {
    chooserTitle = url_formatter::FormatOriginForSecurityDisplay(
        origin, url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);
  }

  chooserContentView_.reset([[ChooserContentView alloc]
      initWithChooserTitle:l10n_util::GetNSStringF(IDS_CHOOSER_BUBBLE_PROMPT,
                                                   chooserTitle)]);

  tableView_ = [chooserContentView_ tableView];
  connectButton_ = [chooserContentView_ connectButton];
  cancelButton_ = [chooserContentView_ cancelButton];
  helpButton_ = [chooserContentView_ helpButton];

  [connectButton_ setTarget:self];
  [connectButton_ setAction:@selector(onConnect:)];
  [cancelButton_ setTarget:self];
  [cancelButton_ setAction:@selector(onCancel:)];
  [tableView_ setDelegate:self];
  [tableView_ setDataSource:self];
  [helpButton_ setTarget:self];
  [helpButton_ setAction:@selector(onHelpPressed:)];
  self.view = chooserContentView_;
  [self updateTableView];

  return self;
}

- (NSInteger)numberOfRowsInTableView:(NSTableView*)tableView {
  // When there are no devices, the table contains a message saying there are
  // no devices, so the number of rows is always at least 1.
  return std::max(static_cast<NSInteger>(chooserController_->NumOptions()),
                  static_cast<NSInteger>(1));
}

- (id)tableView:(NSTableView*)tableView
    objectValueForTableColumn:(NSTableColumn*)tableColumn
                          row:(NSInteger)rowIndex {
  NSInteger num_options =
      static_cast<NSInteger>(chooserController_->NumOptions());
  if (num_options == 0) {
    DCHECK_EQ(0, rowIndex);
    return l10n_util::GetNSString(IDS_CHOOSER_BUBBLE_NO_DEVICES_FOUND_PROMPT);
  }

  DCHECK_GE(rowIndex, 0);
  DCHECK_LT(rowIndex, num_options);
  return base::SysUTF16ToNSString(
      chooserController_->GetOption(static_cast<size_t>(rowIndex)));
}

- (BOOL)tableView:(NSTableView*)aTableView
    shouldEditTableColumn:(NSTableColumn*)aTableColumn
                      row:(NSInteger)rowIndex {
  return NO;
}

- (void)onOptionsInitialized {
  [self updateTableView];
}

- (void)onOptionAdded:(NSInteger)index {
  [self updateTableView];
}

- (void)onOptionRemoved:(NSInteger)index {
  // |tableView_| will automatically selects the next item if the current
  // item is removed, so here it tracks if the removed item is the item
  // that was previously selected, if so, deselect it.
  if ([tableView_ selectedRow] == index)
    [tableView_ deselectRow:index];

  [self updateTableView];
}

- (void)updateTableView {
  [tableView_ setEnabled:chooserController_->NumOptions() > 0];
  [tableView_ reloadData];
}

- (void)tableViewSelectionDidChange:(NSNotification*)aNotification {
  [connectButton_ setEnabled:[tableView_ numberOfSelectedRows] > 0];
}

- (void)onConnect:(id)sender {
  NSInteger row = [tableView_ selectedRow];
  chooserDialogCocoa_->Dismissed();
  chooserController_->Select(row);
}

- (void)onCancel:(id)sender {
  chooserDialogCocoa_->Dismissed();
  chooserController_->Cancel();
}

- (void)onHelpPressed:(id)sender {
  chooserController_->OpenHelpCenterUrl();
}

- (ChooserContentView*)chooserContentView {
  return chooserContentView_.get();
}

@end
