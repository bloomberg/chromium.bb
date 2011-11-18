// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/bookmark_all_tabs_dialog.h"

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/views/extensions/extension_dialog.h"
#include "chrome/browser/ui/views/extensions/extension_dialog_observer.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

const char kBookmarkAllTabsURL[] =
    "chrome-extension://eemcgdkfndhakfknompkggombfjjjeno/"
    "bookmark_all_tabs.html";

const int kDialogWidth = 640;
const int kDialogHeight = 480;

////////////////////////////////////////////////////////////////////////////////
// BookmarkAllTabsDialogBuilder class
////////////////////////////////////////////////////////////////////////////////

class BookmarkAllTabsDialogBuilder : public ExtensionDialogObserver {
 public:
  explicit BookmarkAllTabsDialogBuilder(Profile* profile);
  virtual ~BookmarkAllTabsDialogBuilder();

  // ExtensionDialog::Observer implementation.
  virtual void ExtensionDialogIsClosing(ExtensionDialog* dialog) OVERRIDE;

  void Show();

 private:
  Profile* profile_;

  // Host for the extension that implements this dialog.
  scoped_refptr<ExtensionDialog> extension_dialog_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkAllTabsDialogBuilder);
};

BookmarkAllTabsDialogBuilder::BookmarkAllTabsDialogBuilder(Profile* profile)
  : profile_(profile) {
}

BookmarkAllTabsDialogBuilder::~BookmarkAllTabsDialogBuilder() {
  if (extension_dialog_)
    extension_dialog_->ObserverDestroyed();
}

void BookmarkAllTabsDialogBuilder::ExtensionDialogIsClosing(
    ExtensionDialog* dialog) {
  extension_dialog_ = NULL;
  delete this;
}

void BookmarkAllTabsDialogBuilder::Show() {
  // Extension background pages may not supply an owner_window.
  Browser* browser = BrowserList::GetLastActiveWithProfile(profile_);
  if (!browser) {
    LOG(ERROR) << "Can't find owning browser";
    return;
  }
  GURL url(kBookmarkAllTabsURL);
  TabContentsWrapper* tab = browser->GetSelectedTabContentsWrapper();

  string16 title =
      l10n_util::GetStringUTF16(IDS_BOOKMARK_MANAGER_BOOKMARK_ALL_TABS);

  ExtensionDialog* dialog = ExtensionDialog::Show(
      url,
      browser, tab->tab_contents(),
      kDialogWidth, kDialogHeight,
      title,
      this );
  if (!dialog) {
    LOG(ERROR) << "Unable to create extension dialog";
    return;
  }
  extension_dialog_ = dialog;
}

namespace browser {

void ShowBookmarkAllTabsDialog(Profile* profile) {
  BookmarkAllTabsDialogBuilder *dialog
      = new BookmarkAllTabsDialogBuilder(profile);
  dialog->Show();
}

}  // namespace browser
