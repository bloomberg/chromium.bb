// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_BUNDLE_INSTALLER_H_
#define CHROME_BROWSER_EXTENSIONS_BUNDLE_INSTALLER_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/webstore_install_helper.h"
#include "chrome/browser/extensions/webstore_installer.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "extensions/common/extension.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

namespace base {
class DictionaryValue;
}  // namespace base

namespace content {
class WebContents;
}  // namespace content

class Browser;
class Profile;

namespace extensions {

// Manages the installation life cycle for extension bundles.
//
// We install bundles in two steps:
//  1) PromptForApproval: parse manifests and prompt the user
//  2) CompleteInstall: install the CRXs and show confirmation bubble
//
class BundleInstaller : public WebstoreInstallHelper::Delegate,
                        public WebstoreInstaller::Delegate,
                        public chrome::BrowserListObserver {
 public:
  // Auto approve or cancel the permission prompt.
  static void SetAutoApproveForTesting(bool approve);

  // Represents an individual member of the bundle.
  struct Item {
    // Items are in the PENDING state until they've been installed, or the
    // install has failed or been canceled.
    enum State {
      STATE_PENDING,
      STATE_INSTALLED,
      STATE_FAILED
    };

    Item();
    ~Item();

    // Gets the localized name, formatted for display in the bubble.
    base::string16 GetNameForDisplay() const;

    std::string id;
    std::string manifest;
    std::string localized_name;
    GURL icon_url;
    SkBitmap icon;
    State state;
  };

  enum ApprovalState {
    APPROVED,
    USER_CANCELED,
    APPROVAL_ERROR
  };

  typedef base::Callback<void(ApprovalState)> ApprovalCallback;

  typedef std::vector<Item> ItemList;

  BundleInstaller(Browser* browser,
                  const std::string& localized_name,
                  const SkBitmap& icon,
                  const std::string& authuser,
                  const std::string& delegated_username,
                  const ItemList& items);
  ~BundleInstaller() override;

  // Returns true if the user has approved the bundle's permissions.
  bool approved() const { return approved_; }

  // Returns the browser window associated with the bundle's installation.
  // Can return null if the browser is closed during the installation.
  Browser* browser() { return browser_; }

  // Gets the items in the given |state|.
  ItemList GetItemsWithState(Item::State state) const;

  // Returns whether there is at least one item with the given |state|.
  bool HasItemWithState(Item::State state) const;

  // Returns the number of items with the given |state|.
  size_t CountItemsWithState(Item::State state) const;

  // Parses the extension manifests and then prompts the user to approve their
  // permissions.
  void PromptForApproval(const ApprovalCallback& callback);

  // If the bundle has been approved, this downloads and installs the member
  // extensions. The download process uses the NavigationController of the
  // specified |web_contents|. When complete, we show a confirmation bubble.
  void CompleteInstall(content::WebContents* web_contents,
                       const base::Closure& callback);

  // We change the headings in the install prompt and installed bubble depending
  // on whether the bundle contains apps, extensions or both. This method gets
  // the correct heading for the items in the specified |state|, or an empty
  // string if no items are in the |state|.
  //   STATE_PENDING   - install prompt
  //   STATE_INSTALLED - installed bubble successful installs list
  //   STATE_FAILED    - installed bubble failed installs list
  base::string16 GetHeadingTextFor(Item::State state) const;

 private:
  typedef std::map<std::string, Item> ItemMap;
  typedef std::map<std::string, linked_ptr<base::DictionaryValue> > ManifestMap;

  // Displays the install bubble for |bundle| on |browser|.
  // Note: this is a platform specific implementation.
  static void ShowInstalledBubble(const BundleInstaller* bundle,
                                  Browser* browser);

  // Parses the manifests using WebstoreInstallHelper.
  void ParseManifests();

  // Prompts the user to install the bundle once we have dummy extensions for
  // all the pending items.
  void ShowPromptIfDoneParsing();

  // Prompts the user to install the bundle.
  void ShowPrompt();

  // Displays the installed bubble once all items have installed or failed.
  void ShowInstalledBubbleIfDone();

  // WebstoreInstallHelper::Delegate implementation:
  void OnWebstoreParseSuccess(const std::string& id,
                              const SkBitmap& icon,
                              base::DictionaryValue* parsed_manifest) override;
  void OnWebstoreParseFailure(const std::string& id,
                              InstallHelperResultCode result_code,
                              const std::string& error_message) override;

  // WebstoreInstaller::Delegate implementation:
  void OnExtensionInstallSuccess(const std::string& id) override;
  void OnExtensionInstallFailure(
      const std::string& id,
      const std::string& error,
      WebstoreInstaller::FailureReason reason) override;

  // chrome::BrowserListObserver implementation:
  void OnBrowserRemoved(Browser* browser) override;

  void OnInstallPromptDone(ExtensionInstallPrompt::Result result);

  // Holds the Extensions used to generate the permission warnings.
  ExtensionList dummy_extensions_;

  // Holds the parsed manifests, indexed by the extension ids.
  ManifestMap parsed_manifests_;

  // True if the user has approved the bundle.
  bool approved_;

  // Holds the bundle's Items, indexed by their ids.
  ItemMap items_;

  // The browser to show the confirmation bubble for.
  Browser* browser_;

  // The bundle's display name.
  std::string name_;

  // The bundle's icon.
  SkBitmap icon_;

  // The authuser query parameter value which should be used with CRX download
  // requests. May be empty.
  std::string authuser_;

  // The display name of the user for which this install happens, in the case
  // of delegated installs. Empty for regular installs.
  std::string delegated_username_;

  // The profile that the bundle should be installed in.
  Profile* profile_;

  // The UI that shows the confirmation prompt.
  scoped_ptr<ExtensionInstallPrompt> install_ui_;

  ApprovalCallback approval_callback_;
  base::Closure install_callback_;

  base::WeakPtrFactory<BundleInstaller> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(BundleInstaller);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_BUNDLE_INSTALLER_H_
