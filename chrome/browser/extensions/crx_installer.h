// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CRX_INSTALLER_H_
#define CHROME_BROWSER_EXTENSIONS_CRX_INSTALLER_H_
#pragma once

#include <string>

#include "base/file_path.h"
#include "base/memory/ref_counted.h"
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

  // This is pretty lame, but given the difficulty of connecting a particular
  // ExtensionFunction to a resulting download in the download manager, it's
  // currently necessary. This is the |id| of an extension to be installed
  // *by the web store only* which should not get the permissions install
  // prompt. This should only be called on the UI thread.
  // crbug.com/54916
  static void SetWhitelistedInstallId(const std::string& id);

  // Exempt the next extension install with |id| from displaying a confirmation
  // prompt, since the user already agreed to the install via
  // beginInstallWithManifest. We require that the extension manifest matches
  // |parsed_manifest| which is what was used to prompt with. Ownership of
  // |parsed_manifest| is transferred here.
  static void SetWhitelistedManifest(const std::string& id,
                                     DictionaryValue* parsed_manifest);

  // Returns the previously stored manifest from a call to
  // SetWhitelistedManifest.
  static const DictionaryValue* GetWhitelistedManifest(const std::string& id);

  // Removes any whitelisted manifest for |id| and returns it. The caller owns
  // the return value and is responsible for deleting it.
  static DictionaryValue* RemoveWhitelistedManifest(const std::string& id);

  // Returns whether |id| is whitelisted - only call this on the UI thread.
  static bool IsIdWhitelisted(const std::string& id);

  // Returns whether |id| was found and removed (was whitelisted). This should
  // only be called on the UI thread.
  static bool ClearWhitelistedInstallId(const std::string& id);

  // Constructor.  Extensions will be installed into
  // frontend->install_directory() then registered with |frontend|. Any install
  // UI will be displayed using |client|. Pass NULL for |client| for silent
  // install.
  CrxInstaller(ExtensionService* frontend,
               ExtensionInstallUI* client);

  // Install the crx in |source_file|.
  void InstallCrx(const FilePath& source_file);

  // Convert the specified user script into an extension and install it.
  void InstallUserScript(const FilePath& source_file,
                         const GURL& original_url);

  // Convert the specified web app into an extension and install it.
  void InstallWebApp(const WebApplicationInfo& web_app);

  // Overridden from ExtensionInstallUI::Delegate:
  virtual void InstallUIProceed();
  virtual void InstallUIAbort();

  const GURL& original_url() const { return original_url_; }
  void set_original_url(const GURL& val) { original_url_ = val; }

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

  bool is_gallery_install() const { return is_gallery_install_; }
  void set_is_gallery_install(bool val) { is_gallery_install_ = val; }

  // If |apps_require_extension_mime_type_| is set to true, be sure to set
  // |original_mime_type_| as well.
  void set_apps_require_extension_mime_type(
      bool apps_require_extension_mime_type) {
    apps_require_extension_mime_type_ = apps_require_extension_mime_type;
  }

  void set_original_mime_type(const std::string& original_mime_type) {
    original_mime_type_ = original_mime_type;
  }

 private:
  ~CrxInstaller();

  // Converts the source user script to an extension.
  void ConvertUserScriptOnFileThread();

  // Converts the source web app to an extension.
  void ConvertWebAppOnFileThread(const WebApplicationInfo& web_app);

  // Called after OnUnpackSuccess as a last check to see whether the install
  // should complete.
  bool AllowInstall(const Extension* extension, std::string* error);

  // SandboxedExtensionUnpackerClient
  virtual void OnUnpackFailure(const std::string& error_message);
  virtual void OnUnpackSuccess(const FilePath& temp_dir,
                               const FilePath& extension_dir,
                               const Extension* extension);

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

  // The file we're installing.
  FilePath source_file_;

  // The URL the file was downloaded from.
  GURL original_url_;

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

  // Whether the install originated from the gallery.
  bool is_gallery_install_;

  // Whether to create an app shortcut after successful installation. This is
  // set based on the user's selection in the UI and can only ever be true for
  // apps.
  bool create_app_shortcut_;

  // The extension we're installing. We own this and either pass it off to
  // ExtensionService on success, or delete it on failure.
  scoped_refptr<const Extension> extension_;

  // If non-empty, contains the current version of the extension we're
  // installing (for upgrades).
  std::string current_version_;

  // The icon we will display in the installation UI, if any.
  scoped_ptr<SkBitmap> install_icon_;

  // The temp directory extension resources were unpacked to. We own this and
  // must delete it when we are done with it.
  FilePath temp_dir_;

  // The frontend we will report results back to.
  scoped_refptr<ExtensionService> frontend_;

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

  DISALLOW_COPY_AND_ASSIGN(CrxInstaller);
};

#endif  // CHROME_BROWSER_EXTENSIONS_CRX_INSTALLER_H_
