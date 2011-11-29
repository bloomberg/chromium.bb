// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CRX_INSTALLER_H_
#define CHROME_BROWSER_EXTENSIONS_CRX_INSTALLER_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/version.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/browser/extensions/sandboxed_extension_unpacker.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/web_apps.h"

class ExtensionService;
class SkBitmap;

// This class installs a crx file into a profile.
//
// Installing a CRX is a multi-step process, including unpacking the crx,
// validating it, prompting the user, and installing. Since many of these
// steps must occur on the file thread, this class contains a copy of all data
// necessary to do its job. (This also minimizes external dependencies for
// easier testing).
//
// Lifetime management:
//
// This class is ref-counted by each call it makes to itself on another thread,
// and by UtilityProcessHost.
//
// Additionally, we hold a reference to our own client so that it lives at least
// long enough to receive the result of unpacking.
//
// IMPORTANT: Callers should keep a reference to a CrxInstaller while they are
// working with it, eg:
//
// scoped_refptr<CrxInstaller> installer(new CrxInstaller(...));
// installer->set_foo();
// installer->set_bar();
// installer->InstallCrx(...);
class CrxInstaller
    : public SandboxedExtensionUnpackerClient,
      public ExtensionInstallUI::Delegate {
 public:
  // Extensions will be installed into frontend->install_directory(),
  // then registered with |frontend|. Any install UI will be displayed
  // using |client|. Pass NULL for |client| for silent install.
  static scoped_refptr<CrxInstaller> Create(
      ExtensionService* frontend,
      ExtensionInstallUI* client);

  // This is pretty lame, but given the difficulty of connecting a particular
  // ExtensionFunction to a resulting download in the download manager, it's
  // currently necessary. This is the |id| of an extension to be installed
  // *by the web store only* which should not get the permissions install
  // prompt. This should only be called on the UI thread.
  // crbug.com/54916
  // TODO(asargent): This should be removed now that SetWhitelistEntry exists
  // http://crbug.com/100584
  static void SetWhitelistedInstallId(const std::string& id);

  struct WhitelistEntry {
    WhitelistEntry();
    ~WhitelistEntry();

    scoped_ptr<base::DictionaryValue> parsed_manifest;
    std::string localized_name;

    // Whether to use a bubble notification when an app is installed, instead of
    // the default behavior of transitioning to the new tab page.
    bool use_app_installed_bubble;

    // Whether to skip the post install UI like the extension installed bubble.
    bool skip_post_install_ui;
  };

  // Exempt the next extension install with |id| from displaying a confirmation
  // prompt, since the user already agreed to the install via
  // beginInstallWithManifest3. We require that the extension manifest matches
  // the manifest in |entry|, which is what was used to prompt with. Ownership
  // of |entry| is transferred here.
  static void SetWhitelistEntry(const std::string& id, WhitelistEntry* entry);

  // Returns the previously stored manifest from a call to SetWhitelistEntry.
  static const CrxInstaller::WhitelistEntry* GetWhitelistEntry(
      const std::string& id);

  // Removes any whitelist data for |id| and returns it. The caller owns
  // the return value and is responsible for deleting it.
  static WhitelistEntry* RemoveWhitelistEntry(const std::string& id);

  // Returns whether |id| is whitelisted - only call this on the UI thread.
  static bool IsIdWhitelisted(const std::string& id);

  // Returns whether |id| was found and removed (was whitelisted). This should
  // only be called on the UI thread.
  static bool ClearWhitelistedInstallId(const std::string& id);

  // Install the crx in |source_file|.
  void InstallCrx(const FilePath& source_file);

  // Convert the specified user script into an extension and install it.
  void InstallUserScript(const FilePath& source_file,
                         const GURL& download_url);

  // Convert the specified web app into an extension and install it.
  void InstallWebApp(const WebApplicationInfo& web_app);

  // Overridden from ExtensionInstallUI::Delegate:
  virtual void InstallUIProceed() OVERRIDE;
  virtual void InstallUIAbort(bool user_initiated) OVERRIDE;

  int creation_flags() const { return creation_flags_; }
  void set_creation_flags(int val) { creation_flags_ = val; }

  const GURL& download_url() const { return download_url_; }
  void set_download_url(const GURL& val) { download_url_ = val; }

  Extension::Location install_source() const { return install_source_; }
  void set_install_source(Extension::Location source) {
    install_source_ = source;
  }

  const std::string& expected_id() const { return expected_id_; }
  void set_expected_id(const std::string& val) { expected_id_ = val; }

  void set_expected_version(const Version& val) {
    expected_version_.reset(val.Clone());
  }

  bool delete_source() const { return delete_source_; }
  void set_delete_source(bool val) { delete_source_ = val; }

  bool allow_silent_install() const { return allow_silent_install_; }
  void set_allow_silent_install(bool val) { allow_silent_install_ = val; }

  bool is_gallery_install() const {
    return (creation_flags_ & Extension::FROM_WEBSTORE) > 0;
  }
  void set_is_gallery_install(bool val) {
    if (val)
      creation_flags_ |= Extension::FROM_WEBSTORE;
    else
      creation_flags_ &= ~Extension::FROM_WEBSTORE;
  }

  // The original download URL should be set when the WebstoreInstaller is
  // tracking the installation. The WebstoreInstaller uses this URL to match
  // failure notifications to the extension.
  const GURL& original_download_url() const { return original_download_url_; }
  void set_original_download_url(const GURL& url) {
    original_download_url_ = url;
  }

  // If |apps_require_extension_mime_type_| is set to true, be sure to set
  // |original_mime_type_| as well.
  void set_apps_require_extension_mime_type(
      bool apps_require_extension_mime_type) {
    apps_require_extension_mime_type_ = apps_require_extension_mime_type;
  }

  void set_original_mime_type(const std::string& original_mime_type) {
    original_mime_type_ = original_mime_type;
  }

  extension_misc::CrxInstallCause install_cause() const {
    return install_cause_;
  }

  void set_install_cause(extension_misc::CrxInstallCause install_cause) {
    install_cause_ = install_cause;
  }

  void set_page_index(int page_index) {
    page_index_ = page_index;
  }

  Profile* profile() { return profile_; }

 private:
  friend class ExtensionUpdaterTest;

  CrxInstaller(base::WeakPtr<ExtensionService> frontend_weak,
               ExtensionInstallUI* client);
  virtual ~CrxInstaller();

  // Converts the source user script to an extension.
  void ConvertUserScriptOnFileThread();

  // Converts the source web app to an extension.
  void ConvertWebAppOnFileThread(const WebApplicationInfo& web_app);

  // Called after OnUnpackSuccess as a last check to see whether the install
  // should complete.
  bool AllowInstall(const Extension* extension, std::string* error);

  // SandboxedExtensionUnpackerClient
  virtual void OnUnpackFailure(const std::string& error_message) OVERRIDE;
  virtual void OnUnpackSuccess(const FilePath& temp_dir,
                               const FilePath& extension_dir,
                               const base::DictionaryValue* original_manifest,
                               const Extension* extension) OVERRIDE;

  // Returns true if we can skip confirmation because the install was
  // whitelisted.
  bool CanSkipConfirmation();

  // Runs on the UI thread. Confirms with the user (via ExtensionInstallUI) that
  // it is OK to install this extension.
  void ConfirmInstall();

  // Runs on File thread. Install the unpacked extension into the profile and
  // notify the frontend.
  void CompleteInstall();

  // Result reporting.
  void ReportFailureFromFileThread(const std::string& error);
  void ReportFailureFromUIThread(const std::string& error);
  void ReportSuccessFromFileThread();
  void ReportSuccessFromUIThread();
  void NotifyCrxInstallComplete(const Extension* extension);

  // The file we're installing.
  FilePath source_file_;

  // The URL the file was downloaded from.
  GURL download_url_;

  // The directory extensions are installed to.
  FilePath install_directory_;

  // The location the installation came from (bundled with Chromium, registry,
  // manual install, etc). This metadata is saved with the installation if
  // successful. Defaults to INTERNAL.
  Extension::Location install_source_;

  // For updates and external installs we have an ID we're expecting the
  // extension to contain.
  std::string expected_id_;

  // If non-NULL, contains the expected version of the extension we're
  // installing.  Important for external sources, where claiming the wrong
  // version could cause unnessisary unpacking of an extension at every
  // restart.
  scoped_ptr<Version> expected_version_;

  // Whether manual extension installation is enabled. We can't just check this
  // before trying to install because themes are special-cased to always be
  // allowed.
  bool extensions_enabled_;

  // Whether we're supposed to delete the source file on destruction. Defaults
  // to false.
  bool delete_source_;

  // The download URL, before redirects, if this is a gallery install.
  GURL original_download_url_;

  // Whether to create an app shortcut after successful installation. This is
  // set based on the user's selection in the UI and can only ever be true for
  // apps.
  bool create_app_shortcut_;

  // The extension we're installing. We own this and either pass it off to
  // ExtensionService on success, or delete it on failure.
  scoped_refptr<const Extension> extension_;

  // The index of the NTP apps page |extension_| will be shown on.
  int page_index_;

  // A parsed copy of the unmodified original manifest, before any
  // transformations like localization have taken place.
  scoped_ptr<base::DictionaryValue> original_manifest_;

  // If non-empty, contains the current version of the extension we're
  // installing (for upgrades).
  std::string current_version_;

  // The icon we will display in the installation UI, if any.
  scoped_ptr<SkBitmap> install_icon_;

  // The temp directory extension resources were unpacked to. We own this and
  // must delete it when we are done with it.
  FilePath temp_dir_;

  // The frontend we will report results back to.
  base::WeakPtr<ExtensionService> frontend_weak_;

  // The Profile where the extension is being installed in.
  Profile* profile_;

  // The client we will work with to do the installation. This can be NULL, in
  // which case the install is silent.
  // NOTE: we may be deleted on the file thread. To ensure the UI is deleted on
  // the main thread we don't use a scoped_ptr here.
  ExtensionInstallUI* client_;

  // The root of the unpacked extension directory. This is a subdirectory of
  // temp_dir_, so we don't have to delete it explicitly.
  FilePath unpacked_extension_root_;

  // True when the CRX being installed was just downloaded.
  // Used to trigger extra checks before installing.
  bool apps_require_extension_mime_type_;

  // Allows for the possibility of a normal install (one in which a |client|
  // is provided in the ctor) to procede without showing the permissions prompt
  // dialog. Note that this will only take place if |allow_silent_install_|
  // is true AND the unpacked id of the extension is whitelisted with
  // SetWhitelistedInstallId().
  bool allow_silent_install_;

  // The value of the content type header sent with the CRX.
  // Ignorred unless |require_extension_mime_type_| is true.
  std::string original_mime_type_;

  // What caused this install?  Used only for histograms that report
  // on failure rates, broken down by the cause of the install.
  extension_misc::CrxInstallCause install_cause_;

  // Creation flags to use for the extension.  These flags will be used
  // when calling Extenion::Create() by the crx installer.
  int creation_flags_;

  DISALLOW_COPY_AND_ASSIGN(CrxInstaller);
};

#endif  // CHROME_BROWSER_EXTENSIONS_CRX_INSTALLER_H_
