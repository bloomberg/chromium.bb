// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_BUNDLE_INSTALLER_H_
#define CHROME_BROWSER_EXTENSIONS_BUNDLE_INSTALLER_H_

#include <string>
#include <vector>

#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/webstore_install_helper.h"
#include "chrome/browser/extensions/webstore_installer.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/host_desktop.h"
#include "extensions/common/extension.h"

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
                        public ExtensionInstallPrompt::Delegate,
                        public WebstoreInstaller::Delegate,
                        public chrome::BrowserListObserver,
                        public base::RefCountedThreadSafe<BundleInstaller> {
 public:
  // Auto approve or cancel the permission prompt.
  static void SetAutoApproveForTesting(bool approve);

  class Delegate {
   public:
    virtual void OnBundleInstallApproved() {}
    virtual void OnBundleInstallCanceled(bool user_initiated) {}
    virtual void OnBundleInstallCompleted() {}

   protected:
    virtual ~Delegate() {}
  };

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

    // Gets the localized name, formatted for display in the prompt or bubble.
    base::string16 GetNameForDisplay();

    std::string id;
    std::string manifest;
    std::string localized_name;
    State state;
  };

  typedef std::vector<Item> ItemList;

  BundleInstaller(Browser* browser, const ItemList& items);

  // Returns true if the user has approved the bundle's permissions.
  bool approved() const { return approved_; }

  // Gets the items in the given state.
  ItemList GetItemsWithState(Item::State state) const;

  // Parses the extension manifests and then prompts the user to approve their
  // permissions. One of OnBundleInstallApproved or OnBundleInstallCanceled
  // will be called when complete if |delegate| is not NULL.
  // Note: the |delegate| must stay alive until receiving the callback.
  void PromptForApproval(Delegate* delegate);

  // If the bundle has been approved, this downloads and installs the member
  // extensions. OnBundleInstallComplete will be called when the process is
  // complete and |delegate| is not NULL. The download process uses the
  // NavigationController of the specified |web_contents|. When complete, we
  // show a confirmation bubble in the specified |browser|.
  // Note: the |delegate| must stay alive until receiving the callback.
  void CompleteInstall(content::WebContents* web_contents, Delegate* delegate);

  // We change the headings in the install prompt and installed bubble depending
  // on whether the bundle contains apps, extensions or both. This method gets
  // the correct heading for the items in the specified |state|, or an empty
  // string if no items are in the |state|.
  //   STATE_PENDING   - install prompt
  //   STATE_INSTALLED - installed bubble successful installs list
  //   STATE_FAILED    - installed bubble failed installs list
  base::string16 GetHeadingTextFor(Item::State state) const;

 private:
  friend class base::RefCountedThreadSafe<BundleInstaller>;

  typedef std::map<std::string, Item> ItemMap;
  typedef std::map<std::string, linked_ptr<base::DictionaryValue> > ManifestMap;

  virtual ~BundleInstaller();

  // Displays the install bubble for |bundle| on |browser|.
  // Note: this is a platform specific implementation.
  static void ShowInstalledBubble(const BundleInstaller* bundle,
                                  Browser* browser);

  // Parses the manifests using WebstoreInstallHelper.
  void ParseManifests();

  // Notifies the delegate that the installation has been approved.
  void ReportApproved();

  // Notifies the delegate that the installation was canceled.
  void ReportCanceled(bool user_initiated);

  // Notifies the delegate that the installation is complete.
  void ReportComplete();

  // Prompts the user to install the bundle once we have dummy extensions for
  // all the pending items.
  void ShowPromptIfDoneParsing();

  // Prompts the user to install the bundle.
  void ShowPrompt();

  // Displays the installed bubble once all items have installed or failed.
  void ShowInstalledBubbleIfDone();

  // WebstoreInstallHelper::Delegate implementation:
  virtual void OnWebstoreParseSuccess(
      const std::string& id,
      const SkBitmap& icon,
      base::DictionaryValue* parsed_manifest) OVERRIDE;
  virtual void OnWebstoreParseFailure(
      const std::string& id,
      InstallHelperResultCode result_code,
      const std::string& error_message) OVERRIDE;

  // ExtensionInstallPrompt::Delegate implementation:
  virtual void InstallUIProceed() OVERRIDE;
  virtual void InstallUIAbort(bool user_initiated) OVERRIDE;

  // WebstoreInstaller::Delegate implementation:
  virtual void OnExtensionInstallSuccess(const std::string& id) OVERRIDE;
  virtual void OnExtensionInstallFailure(
      const std::string& id,
      const std::string& error,
      WebstoreInstaller::FailureReason reason) OVERRIDE;

  // chrome::BrowserListObserver implementation:
  virtual void OnBrowserAdded(Browser* browser) OVERRIDE;
  virtual void OnBrowserRemoved(Browser* browser) OVERRIDE;
  virtual void OnBrowserSetLastActive(Browser* browser) OVERRIDE;

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

  // The desktop type of the browser.
  chrome::HostDesktopType host_desktop_type_;

  // The profile that the bundle should be installed in.
  Profile* profile_;

  // The UI that shows the confirmation prompt.
  scoped_ptr<ExtensionInstallPrompt> install_ui_;

  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(BundleInstaller);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_BUNDLE_INSTALLER_H_
