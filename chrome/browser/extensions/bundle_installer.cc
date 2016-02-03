// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/bundle_installer.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/i18n/rtl.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/permissions_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_skia.h"

namespace extensions {

namespace {

enum AutoApproveForTest {
  DO_NOT_SKIP = 0,
  PROCEED,
  ABORT
};

AutoApproveForTest g_auto_approve_for_test = DO_NOT_SKIP;

scoped_refptr<Extension> CreateDummyExtension(
    const BundleInstaller::Item& item,
    const base::DictionaryValue& manifest,
    content::BrowserContext* browser_context) {
  // We require localized names so we can have nice error messages when we can't
  // parse an extension manifest.
  CHECK(!item.localized_name.empty());

  std::string error;
  scoped_refptr<Extension> extension = Extension::Create(base::FilePath(),
                                                         Manifest::INTERNAL,
                                                         manifest,
                                                         Extension::NO_FLAGS,
                                                         item.id,
                                                         &error);
  // Initialize permissions so that withheld permissions are displayed properly
  // in the install prompt.
  PermissionsUpdater(browser_context, PermissionsUpdater::INIT_FLAG_TRANSIENT)
      .InitializePermissions(extension.get());
  return extension;
}

const int kHeadingIdsInstallPrompt[] = {
  IDS_EXTENSION_BUNDLE_INSTALL_PROMPT_TITLE_EXTENSIONS,
  IDS_EXTENSION_BUNDLE_INSTALL_PROMPT_TITLE_APPS,
  IDS_EXTENSION_BUNDLE_INSTALL_PROMPT_TITLE_EXTENSION_APPS
};

const int kHeadingIdsDelegatedInstallPrompt[] = {
  IDS_EXTENSION_BUNDLE_DELEGATED_INSTALL_PROMPT_TITLE_EXTENSIONS,
  IDS_EXTENSION_BUNDLE_DELEGATED_INSTALL_PROMPT_TITLE_APPS,
  IDS_EXTENSION_BUNDLE_DELEGATED_INSTALL_PROMPT_TITLE_EXTENSION_APPS
};

const int kHeadingIdsInstalledBubble[] = {
  IDS_EXTENSION_BUNDLE_INSTALLED_HEADING_EXTENSIONS,
  IDS_EXTENSION_BUNDLE_INSTALLED_HEADING_APPS,
  IDS_EXTENSION_BUNDLE_INSTALLED_HEADING_EXTENSION_APPS
};

}  // namespace

// static
void BundleInstaller::SetAutoApproveForTesting(bool auto_approve) {
  CHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestType));
  g_auto_approve_for_test = auto_approve ? PROCEED : ABORT;
}

BundleInstaller::Item::Item() : state(STATE_PENDING) {}

BundleInstaller::Item::~Item() {}

base::string16 BundleInstaller::Item::GetNameForDisplay() const {
  base::string16 name = base::UTF8ToUTF16(localized_name);
  base::i18n::AdjustStringForLocaleDirection(&name);
  return name;
}

BundleInstaller::BundleInstaller(Browser* browser,
                                 const std::string& name,
                                 const SkBitmap& icon,
                                 const std::string& authuser,
                                 const std::string& delegated_username,
                                 const BundleInstaller::ItemList& items)
    : approved_(false),
      browser_(browser),
      name_(name),
      icon_(icon),
      authuser_(authuser),
      delegated_username_(delegated_username),
      profile_(browser->profile()),
      weak_factory_(this) {
  BrowserList::AddObserver(this);
  for (size_t i = 0; i < items.size(); ++i) {
    items_[items[i].id] = items[i];
    items_[items[i].id].state = Item::STATE_PENDING;
  }
}

BundleInstaller::~BundleInstaller() {
  BrowserList::RemoveObserver(this);
}

BundleInstaller::ItemList BundleInstaller::GetItemsWithState(
    Item::State state) const {
  ItemList list;

  for (const std::pair<std::string, Item>& entry : items_) {
    if (entry.second.state == state)
      list.push_back(entry.second);
  }

  return list;
}

bool BundleInstaller::HasItemWithState(Item::State state) const {
  return CountItemsWithState(state) > 0;
}

size_t BundleInstaller::CountItemsWithState(Item::State state) const {
  return std::count_if(items_.begin(), items_.end(),
                       [state] (const std::pair<std::string, Item>& entry) {
                         return entry.second.state == state;
                       });
}

void BundleInstaller::PromptForApproval(const ApprovalCallback& callback) {
  approval_callback_ = callback;

  ParseManifests();
}

void BundleInstaller::CompleteInstall(content::WebContents* web_contents,
                                      const base::Closure& callback) {
  DCHECK(web_contents);
  CHECK(approved_);

  install_callback_ = callback;

  if (!HasItemWithState(Item::STATE_PENDING)) {
    install_callback_.Run();
    return;
  }

  // Start each WebstoreInstaller.
  for (const std::pair<std::string, Item>& entry : items_) {
    if (entry.second.state != Item::STATE_PENDING)
      continue;

    // Since we've already confirmed the permissions, create an approval that
    // lets CrxInstaller bypass the prompt.
    scoped_ptr<WebstoreInstaller::Approval> approval(
        WebstoreInstaller::Approval::CreateWithNoInstallPrompt(
            profile_,
            entry.first,
            make_scoped_ptr(parsed_manifests_[entry.first]->DeepCopy()),
            true));
    approval->use_app_installed_bubble = false;
    approval->skip_post_install_ui = true;
    approval->authuser = authuser_;
    approval->installing_icon =
        gfx::ImageSkia::CreateFrom1xBitmap(entry.second.icon);

    scoped_refptr<WebstoreInstaller> installer = new WebstoreInstaller(
        profile_, this, web_contents, entry.first, std::move(approval),
        WebstoreInstaller::INSTALL_SOURCE_OTHER);
    installer->Start();
  }
}

base::string16 BundleInstaller::GetHeadingTextFor(Item::State state) const {
  // For STATE_FAILED, we can't tell if the items were apps or extensions
  // so we always show the same message.
  if (state == Item::STATE_FAILED) {
    if (HasItemWithState(state))
      return l10n_util::GetStringUTF16(IDS_EXTENSION_BUNDLE_ERROR_HEADING);
    return base::string16();
  }

  size_t total = CountItemsWithState(state);
  if (total == 0)
    return base::string16();

  size_t apps = std::count_if(
      dummy_extensions_.begin(), dummy_extensions_.end(),
      [] (const scoped_refptr<const Extension>& ext) { return ext->is_app(); });

  bool has_apps = apps > 0;
  bool has_extensions = apps < total;
  size_t index = (has_extensions << 0) + (has_apps << 1) - 1;

  if (state == Item::STATE_PENDING) {
    if (!delegated_username_.empty()) {
      return l10n_util::GetStringFUTF16(
          kHeadingIdsDelegatedInstallPrompt[index], base::UTF8ToUTF16(name_),
          base::UTF8ToUTF16(delegated_username_));
    } else {
      return l10n_util::GetStringFUTF16(kHeadingIdsInstallPrompt[index],
                                        base::UTF8ToUTF16(name_));
    }
  } else {
    return l10n_util::GetStringUTF16(kHeadingIdsInstalledBubble[index]);
  }
}

void BundleInstaller::ParseManifests() {
  if (items_.empty()) {
    approval_callback_.Run(APPROVAL_ERROR);
    return;
  }

  net::URLRequestContextGetter* context_getter =
      browser_ ? browser_->profile()->GetRequestContext() : nullptr;

  for (const std::pair<std::string, Item>& entry : items_) {
    scoped_refptr<WebstoreInstallHelper> helper = new WebstoreInstallHelper(
        this, entry.first, entry.second.manifest, entry.second.icon_url,
        context_getter);
    helper->Start();
  }
}

void BundleInstaller::ShowPromptIfDoneParsing() {
  // We don't prompt until all the manifests have been parsed.
  if (CountItemsWithState(Item::STATE_PENDING) != dummy_extensions_.size())
    return;

  ShowPrompt();
}

void BundleInstaller::ShowPrompt() {
  // Abort if we couldn't create any Extensions out of the manifests.
  if (dummy_extensions_.empty()) {
    approval_callback_.Run(APPROVAL_ERROR);
    return;
  }

  scoped_ptr<const PermissionSet> permissions;
  PermissionSet empty;
  for (size_t i = 0; i < dummy_extensions_.size(); ++i) {
    // Using "permissions ? *permissions : PermissionSet()" tries to do a copy,
    // and doesn't compile. Use a more verbose, but compilable, workaround.
    permissions = PermissionSet::CreateUnion(
        permissions ? *permissions : empty,
        dummy_extensions_[i]->permissions_data()->active_permissions());
  }

  if (g_auto_approve_for_test == PROCEED) {
    OnInstallPromptDone(ExtensionInstallPrompt::Result::ACCEPTED);
  } else if (g_auto_approve_for_test == ABORT) {
    OnInstallPromptDone(ExtensionInstallPrompt::Result::USER_CANCELED);
  } else {
    Browser* browser = browser_;
    if (!browser) {
      // The browser that we got initially could have gone away during our
      // thread hopping.
      browser = chrome::FindLastActiveWithProfile(profile_);
    }
    content::WebContents* web_contents = NULL;
    if (browser)
      web_contents = browser->tab_strip_model()->GetActiveWebContents();
    install_ui_.reset(new ExtensionInstallPrompt(web_contents));
    scoped_ptr<ExtensionInstallPrompt::Prompt> prompt;
    if (delegated_username_.empty()) {
      prompt.reset(new ExtensionInstallPrompt::Prompt(
          ExtensionInstallPrompt::BUNDLE_INSTALL_PROMPT));
    } else {
      prompt.reset(new ExtensionInstallPrompt::Prompt(
          ExtensionInstallPrompt::DELEGATED_BUNDLE_PERMISSIONS_PROMPT));
      prompt->set_delegated_username(delegated_username_);
    }
    prompt->set_bundle(this);
    install_ui_->ShowDialog(
        base::Bind(&BundleInstaller::OnInstallPromptDone,
                   weak_factory_.GetWeakPtr()),
        nullptr, &icon_, std::move(prompt), std::move(permissions),
        ExtensionInstallPrompt::GetDefaultShowDialogCallback());
  }
}

void BundleInstaller::ShowInstalledBubbleIfDone() {
  // We're ready to show the installed bubble when no items are pending.
  if (HasItemWithState(Item::STATE_PENDING))
    return;

  if (browser_)
    ShowInstalledBubble(this, browser_);

  install_callback_.Run();
}

void BundleInstaller::OnWebstoreParseSuccess(
    const std::string& id,
    const SkBitmap& icon,
    base::DictionaryValue* manifest) {
  items_[id].icon = icon;
  dummy_extensions_.push_back(
      CreateDummyExtension(items_[id], *manifest, profile_));
  parsed_manifests_[id] = linked_ptr<base::DictionaryValue>(manifest);

  ShowPromptIfDoneParsing();
}

void BundleInstaller::OnWebstoreParseFailure(
    const std::string& id,
    WebstoreInstallHelper::Delegate::InstallHelperResultCode result_code,
    const std::string& error_message) {
  items_[id].state = Item::STATE_FAILED;

  ShowPromptIfDoneParsing();
}

void BundleInstaller::OnInstallPromptDone(
    ExtensionInstallPrompt::Result result) {
  ApprovalState state = APPROVED;
  if (result == ExtensionInstallPrompt::Result::ACCEPTED) {
    approved_ = true;
    state = APPROVED;
  } else {
    for (std::pair<const std::string, Item>& entry : items_)
      entry.second.state = Item::STATE_FAILED;

    state = result == ExtensionInstallPrompt::Result::USER_CANCELED
                ? USER_CANCELED
                : APPROVAL_ERROR;
  }
  approval_callback_.Run(state);
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
      dummy_extensions_.begin(), dummy_extensions_.end(),
      [&id] (const scoped_refptr<const Extension>& ext) {
        return ext->id() == id;
      });
  CHECK(dummy_extensions_.end() != i);
  dummy_extensions_.erase(i);

  ShowInstalledBubbleIfDone();
}

void BundleInstaller::OnBrowserRemoved(Browser* browser) {
  if (browser_ == browser)
    browser_ = nullptr;
}

}  // namespace extensions
