// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_UI_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_UI_H_
#pragma once

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/string16.h"
#include "chrome/browser/extensions/image_loading_tracker.h"
#include "chrome/common/extensions/url_pattern.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/native_widget_types.h"

class Browser;
class Extension;
class ExtensionPermissionSet;
class MessageLoop;
class Profile;
class InfoBarDelegate;
class TabContentsWrapper;

namespace base {
class DictionaryValue;
}  // namespace base

namespace extensions {
class BundleInstaller;
}  // namespace extensions

// Displays all the UI around extension installation.
class ExtensionInstallUI : public ImageLoadingTracker::Observer {
 public:
  enum PromptType {
    UNSET_PROMPT_TYPE = -1,
    INSTALL_PROMPT = 0,
    INLINE_INSTALL_PROMPT,
    BUNDLE_INSTALL_PROMPT,
    RE_ENABLE_PROMPT,
    PERMISSIONS_PROMPT,
    NUM_PROMPT_TYPES
  };

  // Extra information needed to display an installation or uninstallation
  // prompt. Gets populated with raw data and exposes getters for formatted
  // strings so that the GTK/views/Cocoa install dialogs don't have to repeat
  // that logic.
  class Prompt {
   public:
    explicit Prompt(PromptType type);
    ~Prompt();

    void SetPermissions(const std::vector<string16>& permissions);
    void SetInlineInstallWebstoreData(const std::string& localized_user_count,
                                      double average_rating,
                                      int rating_count);

    PromptType type() const { return type_; }
    void set_type(PromptType type) { type_ = type; }

    // Getters for UI element labels.
    string16 GetDialogTitle() const;
    string16 GetHeading() const;
    string16 GetAcceptButtonLabel() const;
    bool HasAbortButtonLabel() const;
    string16 GetAbortButtonLabel() const;
    string16 GetPermissionsHeading() const;

    // Getters for webstore metadata. Only populated when the type is
    // INLINE_INSTALL_PROMPT.

    // The star display logic replicates the one used by the webstore (from
    // components.ratingutils.setFractionalYellowStars). Callers pass in an
    // "appender", which will be repeatedly called back with the star images
    // that they append to the star display area.
    typedef void(*StarAppender)(const SkBitmap*, void*);
    void AppendRatingStars(StarAppender appender, void* data) const;
    string16 GetRatingCount() const;
    string16 GetUserCount() const;
    size_t GetPermissionCount() const;
    string16 GetPermission(size_t index) const;

    // Populated for BUNDLE_INSTALL_PROMPT.
    const extensions::BundleInstaller* bundle() const { return bundle_; }
    void set_bundle(const extensions::BundleInstaller* bundle) {
      bundle_ = bundle;
    }

    // Populated for all other types.
    const Extension* extension() const { return extension_; }
    void set_extension(const Extension* extension) { extension_ = extension; }

    const gfx::Image& icon() const { return icon_; }
    void set_icon(const gfx::Image& icon) { icon_ = icon; }

   private:
    PromptType type_;
    // Permissions that are being requested (may not be all of an extension's
    // permissions if only additional ones are being requested)
    std::vector<string16> permissions_;

    // The extension or bundle being installed.
    const Extension* extension_;
    const extensions::BundleInstaller* bundle_;

    // The icon to be displayed.
    gfx::Image icon_;

    // These fields are populated only when the prompt type is
    // INLINE_INSTALL_PROMPT
    // Already formatted to be locale-specific.
    std::string localized_user_count_;
    // Range is kMinExtensionRating to kMaxExtensionRating
    double average_rating_;
    int rating_count_;
  };

  static const int kMinExtensionRating = 0;
  static const int kMaxExtensionRating = 5;

  class Delegate {
   public:
    // We call this method to signal that the installation should continue.
    virtual void InstallUIProceed() = 0;

    // We call this method to signal that the installation should stop, with
    // |user_initiated| true if the installation was stopped by the user.
    virtual void InstallUIAbort(bool user_initiated) = 0;

   protected:
    virtual ~Delegate() {}
  };

  // Creates a dummy extension from the |manifest|, replacing the name and
  // description with the localizations if provided.
  static scoped_refptr<Extension> GetLocalizedExtensionForDisplay(
      const base::DictionaryValue* manifest,
      const std::string& id,
      const std::string& localized_name,
      const std::string& localized_description,
      std::string* error);

  explicit ExtensionInstallUI(Profile* profile);
  virtual ~ExtensionInstallUI();

  // TODO(asargent) Normally we navigate to the new tab page when an app is
  // installed, but we're experimenting with instead showing a bubble when
  // an app is installed which points to the new tab button. This may become
  // the default behavior in the future.
  void set_use_app_installed_bubble(bool use_bubble) {
    use_app_installed_bubble_ = use_bubble;
  }

  // Whether or not to show the default UI after completing the installation.
  void set_skip_post_install_ui(bool skip_ui) {
    skip_post_install_ui_ = skip_ui;
  }

  // This is called by the bundle installer to verify whether the bundle
  // should be installed.
  //
  // We *MUST* eventually call either Proceed() or Abort() on |delegate|.
  virtual void ConfirmBundleInstall(extensions::BundleInstaller* bundle,
                                    const ExtensionPermissionSet* permissions);

  // This is called by the inline installer to verify whether the inline
  // install from the webstore should proceed.
  //
  // We *MUST* eventually call either Proceed() or Abort() on |delegate|.
  virtual void ConfirmInlineInstall(Delegate* delegate,
                                    const Extension* extension,
                                    SkBitmap* icon,
                                    const Prompt& prompt);

  // This is called by the installer to verify whether the installation from
  // the webstore should proceed.
  //
  // We *MUST* eventually call either Proceed() or Abort() on |delegate|.
  virtual void ConfirmWebstoreInstall(Delegate* delegate,
                                      const Extension* extension,
                                      const SkBitmap* icon);

  // This is called by the installer to verify whether the installation should
  // proceed. This is declared virtual for testing.
  //
  // We *MUST* eventually call either Proceed() or Abort() on |delegate|.
  virtual void ConfirmInstall(Delegate* delegate, const Extension* extension);

  // This is called by the app handler launcher to verify whether the app
  // should be re-enabled. This is declared virtual for testing.
  //
  // We *MUST* eventually call either Proceed() or Abort() on |delegate|.
  virtual void ConfirmReEnable(Delegate* delegate, const Extension* extension);

  // This is called by the extension permissions API to verify whether an
  // extension may be granted additional permissions.
  //
  // We *MUST* eventually call either Proceed() or Abort() on |delegate|.
  virtual void ConfirmPermissions(Delegate* delegate,
                                  const Extension* extension,
                                  const ExtensionPermissionSet* permissions);

  // Installation was successful. This is declared virtual for testing.
  virtual void OnInstallSuccess(const Extension* extension, SkBitmap* icon);

  // Installation failed. This is declared virtual for testing.
  virtual void OnInstallFailure(const string16& error);

  // ImageLoadingTracker::Observer:
  virtual void OnImageLoaded(const gfx::Image& image,
                             const std::string& extension_id,
                             int index) OVERRIDE;

  // Opens apps UI and animates the app icon for the app with id |app_id|.
  static void OpenAppInstalledUI(Browser* browser, const std::string& app_id);

 protected:
  friend class ExtensionNoConfirmWebstorePrivateApiTest;
  friend class WebstoreInlineInstallUnpackFailureTest;

  // Disables showing UI (ErrorBox, etc.) for install failures. To be used only
  // in tests.
  static void DisableFailureUIForTests();

 private:
  friend class GalleryInstallApiTestObserver;

  // Show an infobar for a newly-installed theme.  previous_theme_id
  // should be empty if the previous theme was the system/default
  // theme.
  static void ShowThemeInfoBar(
      const std::string& previous_theme_id, bool previous_using_native_theme,
      const Extension* new_theme, Profile* profile);

  // Sets the icon that will be used in any UI. If |icon| is NULL, or contains
  // an empty bitmap, then a default icon will be used instead.
  void SetIcon(const SkBitmap* icon);

  // Starts the process of showing a confirmation UI, which is split into two.
  // 1) Set off a 'load icon' task.
  // 2) Handle the load icon response and show the UI (OnImageLoaded).
  void LoadImageIfNeeded();

  // Shows the actual UI (the icon should already be loaded).
  void ShowConfirmation();

  // Returns the delegate to control the browser's info bar. This is
  // within its own function due to its platform-specific nature.
  static InfoBarDelegate* GetNewThemeInstalledInfoBarDelegate(
      TabContentsWrapper* tab_contents,
      const Extension* new_theme,
      const std::string& previous_theme_id,
      bool previous_using_native_theme);

  Profile* profile_;
  MessageLoop* ui_loop_;

  // Used to undo theme installation.
  std::string previous_theme_id_;
  bool previous_using_native_theme_;

  // The extensions installation icon.
  SkBitmap icon_;

  // The extension we are showing the UI for.
  const Extension* extension_;

  // The bundle we are showing the UI for, if type BUNDLE_INSTALL_PROMPT.
  const extensions::BundleInstaller* bundle_;

  // The permissions being prompted for.
  scoped_refptr<const ExtensionPermissionSet> permissions_;

  // The delegate we will call Proceed/Abort on after confirmation UI.
  Delegate* delegate_;

  // A pre-filled prompt.
  Prompt prompt_;

  // The type of prompt we are going to show.
  PromptType prompt_type_;

  // Keeps track of extension images being loaded on the File thread for the
  // purpose of showing the install UI.
  ImageLoadingTracker tracker_;

  // Whether to show an installed bubble on app install, or use the default
  // action of opening a new tab page.
  bool use_app_installed_bubble_;

  // Whether or not to show the default UI after completing the installation.
  bool skip_post_install_ui_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_UI_H_
