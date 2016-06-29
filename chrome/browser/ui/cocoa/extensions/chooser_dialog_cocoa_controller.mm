// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/extensions/chooser_dialog_cocoa_controller.h"

#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "chrome/browser/ui/cocoa/chooser_content_view_cocoa.h"
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
         chooserController:
             (std::unique_ptr<ChooserController>)chooserController {
  DCHECK(chooserDialogCocoa);
  DCHECK(chooserController);
  if ((self = [super init]))
    chooserDialogCocoa_ = chooserDialogCocoa;

  base::string16 chooserTitle;
  url::Origin origin = chooserController->GetOrigin();
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

  chooserContentView_.reset([[ChooserContentViewCocoa alloc]
      initWithChooserTitle:l10n_util::GetNSStringF(IDS_DEVICE_CHOOSER_PROMPT,
                                                   chooserTitle)
         chooserController:std::move(chooserController)]);

  tableView_ = [chooserContentView_ tableView];
  connectButton_ = [chooserContentView_ connectButton];
  cancelButton_ = [chooserContentView_ cancelButton];

  [connectButton_ setTarget:self];
  [connectButton_ setAction:@selector(onConnect:)];
  [cancelButton_ setTarget:self];
  [cancelButton_ setAction:@selector(onCancel:)];
  [tableView_ setDelegate:self];
  [tableView_ setDataSource:self];
  self.view = chooserContentView_;
  [chooserContentView_ updateTableView];

  return self;
}

- (NSInteger)numberOfRowsInTableView:(NSTableView*)tableView {
  return [chooserContentView_ numberOfOptions];
}

- (id)tableView:(NSTableView*)tableView
    objectValueForTableColumn:(NSTableColumn*)tableColumn
                          row:(NSInteger)rowIndex {
  return [chooserContentView_ optionAtIndex:rowIndex];
}

- (BOOL)tableView:(NSTableView*)aTableView
    shouldEditTableColumn:(NSTableColumn*)aTableColumn
                      row:(NSInteger)rowIndex {
  return NO;
}

- (void)tableViewSelectionDidChange:(NSNotification*)aNotification {
  [connectButton_ setEnabled:[tableView_ numberOfSelectedRows] > 0];
}

- (void)onConnect:(id)sender {
  [chooserContentView_ accept];
  chooserDialogCocoa_->Dismissed();
}

- (void)onCancel:(id)sender {
  [chooserContentView_ cancel];
  chooserDialogCocoa_->Dismissed();
}

- (ChooserContentViewCocoa*)chooserContentView {
  return chooserContentView_.get();
}

@end
