// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/bundle_installer.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/i18n/rtl.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/permissions/permission_set.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using content::NavigationController;

namespace extensions {

namespace {

enum AutoApproveForTest {
  DO_NOT_SKIP = 0,
  PROCEED,
  ABORT
};

AutoApproveForTest g_auto_approve_for_test = DO_NOT_SKIP;

// Creates a dummy extension and sets the manifest's name to the item's
// localized name.
scoped_refptr<Extension> CreateDummyExtension(const BundleInstaller::Item& item,
                                              DictionaryValue* manifest) {
  // We require localized names so we can have nice error messages when we can't
  // parse an extension manifest.
  CHECK(!item.localized_name.empty());

  std::string error;
  return Extension::Create(FilePath(),
                           Extension::INTERNAL,
                           *manifest,
                           Extension::NO_FLAGS,
                           item.id,
                           &error);
}

bool IsAppPredicate(scoped_refptr<const Extension> extension) {
  return extension->is_app();
}

struct MatchIdFunctor {
  explicit MatchIdFunctor(const std::string& id) : id(id) {}
  bool operator()(scoped_refptr<const Extension> extension) {
    return extension->id() == id;
  }
  std::string id;
};

// Holds the message IDs for BundleInstaller::GetHeadingTextFor.
const int kHeadingIds[3][4] = {
  {
    0,
    IDS_EXTENSION_BUNDLE_INSTALL_PROMPT_HEADING_EXTENSIONS,
    IDS_EXTENSION_BUNDLE_INSTALL_PROMPT_HEADING_APPS,
    IDS_EXTENSION_BUNDLE_INSTALL_PROMPT_HEADING_EXTENSION_APPS
  },
  {
    0,
    IDS_EXTENSION_BUNDLE_INSTALLED_HEADING_EXTENSIONS,
    IDS_EXTENSION_BUNDLE_INSTALLED_HEADING_APPS,
    IDS_EXTENSION_BUNDLE_INSTALLED_HEADING_EXTENSION_APPS
  }
};

}  // namespace

// static
void BundleInstaller::SetAutoApproveForTesting(bool auto_approve) {
  CHECK(CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestType));
  g_auto_approve_for_test = auto_approve ? PROCEED : ABORT;
}

BundleInstaller::Item::Item() : state(STATE_PENDING) {}

string16 BundleInstaller::Item::GetNameForDisplay() {
  string16 name = UTF8ToUTF16(localized_name);
  base::i18n::AdjustStringForLocaleDirection(&name);
  return l10n_util::GetStringFUTF16(IDS_EXTENSION_PERMISSION_LINE, name);
}

BundleInstaller::BundleInstaller(Browser* browser,
                                 const BundleInstaller::ItemList& items)
    : approved_(false),
      browser_(browser),
      host_desktop_type_(browser->host_desktop_type()),
      profile_(browser->profile()),
      delegate_(NULL) {
  BrowserList::AddObserver(this);
  for (size_t i = 0; i < items.size(); ++i) {
    items_[items[i].id] = items[i];
    items_[items[i].id].state = Item::STATE_PENDING;
  }
}

BundleInstaller::ItemList BundleInstaller::GetItemsWithState(
    Item::State state) const {
  ItemList list;

  for (ItemMap::const_iterator i = items_.begin(); i != items_.end(); ++i) {
    if (i->second.state == state)
      list.push_back(i->second);
  }

  return list;
}

void BundleInstaller::PromptForApproval(Delegate* delegate) {
  delegate_ = delegate;

  AddRef();  // Balanced in ReportApproved() and ReportCanceled().

  ParseManifests();
}

void BundleInstaller::CompleteInstall(NavigationController* controller,
                                      Delegate* delegate) {
  CHECK(approved_);

  delegate_ = delegate;

  AddRef();  // Balanced in ReportComplete();

  if (GetItemsWithState(Item::STATE_PENDING).empty()) {
    ReportComplete();
    return;
  }

  // Start each WebstoreInstaller.
  for (ItemMap::iterator i = items_.begin(); i != items_.end(); ++i) {
    if (i->second.state != Item::STATE_PENDING)
      continue;

    // Since we've already confirmed the permissions, create an approval that
    // lets CrxInstaller bypass the prompt.
    scoped_ptr<WebstoreInstaller::Approval> approval(
        WebstoreInstaller::Approval::CreateWithNoInstallPrompt(
            profile_,
            i->first,
            scoped_ptr<base::DictionaryValue>(
                parsed_manifests_[i->first]->DeepCopy())));
    approval->use_app_installed_bubble = false;
    approval->skip_post_install_ui = true;

    scoped_refptr<WebstoreInstaller> installer = new WebstoreInstaller(
        profile_,
        this,
        controller,
        i->first,
        approval.Pass(),
        WebstoreInstaller::FLAG_NONE);
    installer->Start();
  }
}

string16 BundleInstaller::GetHeadingTextFor(Item::State state) const {
  // For STATE_FAILED, we can't tell if the items were apps or extensions
  // so we always show the same message.
  if (state == Item::STATE_FAILED) {
    if (GetItemsWithState(state).size())
      return l10n_util::GetStringUTF16(IDS_EXTENSION_BUNDLE_ERROR_HEADING);
    return string16();
  }

  size_t total = GetItemsWithState(state).size();
  size_t apps = std::count_if(
      dummy_extensions_.begin(), dummy_extensions_.end(), &IsAppPredicate);

  bool has_apps = apps > 0;
  bool has_extensions = apps < total;
  size_t index = (has_extensions << 0) + (has_apps << 1);

  CHECK_LT(static_cast<size_t>(state), arraysize(kHeadingIds));
  CHECK_LT(index, arraysize(kHeadingIds[state]));

  int msg_id = kHeadingIds[state][index];
  if (!msg_id)
    return string16();

  return l10n_util::GetStringUTF16(msg_id);
}

BundleInstaller::~BundleInstaller() {
  BrowserList::RemoveObserver(this);
}

void BundleInstaller::ParseManifests() {
  if (items_.empty()) {
    ReportCanceled(false);
    return;
  }

  for (ItemMap::iterator i = items_.begin(); i != items_.end(); ++i) {
    scoped_refptr<WebstoreInstallHelper> helper = new WebstoreInstallHelper(
        this, i->first, i->second.manifest, "", GURL(), NULL);
    helper->Start();
  }
}

void BundleInstaller::ReportApproved() {
  if (delegate_)
    delegate_->OnBundleInstallApproved();

  Release();  // Balanced in PromptForApproval().
}

void BundleInstaller::ReportCanceled(bool user_initiated) {
  if (delegate_)
    delegate_->OnBundleInstallCanceled(user_initiated);

  Release();  // Balanced in PromptForApproval().
}

void BundleInstaller::ReportComplete() {
  if (delegate_)
    delegate_->OnBundleInstallCompleted();

  Release();  // Balanced in CompleteInstall().
}

void BundleInstaller::ShowPromptIfDoneParsing() {
  // We don't prompt until all the manifests have been parsed.
  ItemList pending_items = GetItemsWithState(Item::STATE_PENDING);
  if (pending_items.size() != dummy_extensions_.size())
    return;

  ShowPrompt();
}

void BundleInstaller::ShowPrompt() {
  // Abort if we couldn't create any Extensions out of the manifests.
  if (dummy_extensions_.empty()) {
    ReportCanceled(false);
    return;
  }

  scoped_refptr<PermissionSet> permissions;
  for (size_t i = 0; i < dummy_extensions_.size(); ++i) {
    permissions = PermissionSet::CreateUnion(
          permissions, dummy_extensions_[i]->required_permission_set());
  }

  if (g_auto_approve_for_test == PROCEED) {
    InstallUIProceed();
  } else if (g_auto_approve_for_test == ABORT) {
    InstallUIAbort(true);
  } else {
    Browser* browser = browser_;
    if (!browser) {
      // The browser that we got initially could have gone away during our
      // thread hopping.
      browser = chrome::FindLastActiveWithProfile(profile_, host_desktop_type_);
    }
    content::WebContents* web_contents = NULL;
    if (browser)
      web_contents = browser->tab_strip_model()->GetActiveWebContents();
    install_ui_.reset(new ExtensionInstallPrompt(web_contents));
    install_ui_->ConfirmBundleInstall(this, permissions);
  }
}

void BundleInstaller::ShowInstalledBubbleIfDone() {
  // We're ready to show the installed bubble when no items are pending.
  if (!GetItemsWithState(Item::STATE_PENDING).empty())
    return;

  if (browser_)
    ShowInstalledBubble(this, browser_);

  ReportComplete();
}

void BundleInstaller::OnWebstoreParseSuccess(
    const std::string& id,
    const SkBitmap& icon,
    DictionaryValue* manifest) {
  dummy_extensions_.push_back(CreateDummyExtension(items_[id], manifest));
  parsed_manifests_[id] = linked_ptr<DictionaryValue>(manifest);

  ShowPromptIfDoneParsing();
}

void BundleInstaller::OnWebstoreParseFailure(
    const std::string& id,
    WebstoreInstallHelper::Delegate::InstallHelperResultCode result_code,
    const std::string& error_message) {
  items_[id].state = Item::STATE_FAILED;

  ShowPromptIfDoneParsing();
}

void BundleInstaller::InstallUIProceed() {
  approved_ = true;
  ReportApproved();
}

void BundleInstaller::InstallUIAbort(bool user_initiated) {
  for (ItemMap::iterator i = items_.begin(); i != items_.end(); ++i)
    i->second.state = Item::STATE_FAILED;

  ReportCanceled(user_initiated);
}

void BundleInstaller::OnExtensionInstallSuccess(const std::string& id) {
  items_[id].state = Item::STATE_INSTALLED;

  ShowInstalledBubbleIfDone();
}

void BundleInstaller::OnExtensionInstallFailure(
    const std::string& id,
    const std::string& error,
    WebstoreInstaller::FailureReason reason) {
  items_[id].state = Item::STATE_FAILED;

  ExtensionList::iterator i = std::find_if(
      dummy_extensions_.begin(), dummy_extensions_.end(), MatchIdFunctor(id));
  CHECK(dummy_extensions_.end() != i);
  dummy_extensions_.erase(i);

  ShowInstalledBubbleIfDone();
}

void BundleInstaller::OnBrowserAdded(Browser* browser) {}

void BundleInstaller::OnBrowserRemoved(Browser* browser) {
  if (browser_ == browser)
    browser_ = NULL;
}

void BundleInstaller::OnBrowserSetLastActive(Browser* browser) {}

}  // namespace extensions
