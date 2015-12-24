// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_PROMPT_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_PROMPT_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "extensions/common/permissions/permission_message.h"
#include "extensions/common/url_pattern.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/native_widget_types.h"

class ExtensionInstallPromptShowParams;
class Profile;

namespace base {
class DictionaryValue;
class MessageLoop;
}  // namespace base

namespace content {
class BrowserContext;
class WebContents;
}

namespace extensions {
class BundleInstaller;
class CrxInstallError;
class Extension;
class ExtensionInstallUI;
class ExtensionWebstorePrivateApiTest;
class MockGetAuthTokenFunction;
class PermissionSet;
}  // namespace extensions

namespace infobars {
class InfoBarDelegate;
}

// Displays all the UI around extension installation.
class ExtensionInstallPrompt
    : public base::SupportsWeakPtr<ExtensionInstallPrompt> {
 public:
  // This enum is associated with Extensions.InstallPrompt_Type UMA histogram.
  // Do not modify existing values and add new values only to the end.
  enum PromptType {
    UNSET_PROMPT_TYPE = -1,
    INSTALL_PROMPT = 0,
    INLINE_INSTALL_PROMPT,
    BUNDLE_INSTALL_PROMPT,
    RE_ENABLE_PROMPT,
    PERMISSIONS_PROMPT,
    EXTERNAL_INSTALL_PROMPT,
    POST_INSTALL_PERMISSIONS_PROMPT,
    LAUNCH_PROMPT_DEPRECATED,
    REMOTE_INSTALL_PROMPT,
    REPAIR_PROMPT,
    DELEGATED_PERMISSIONS_PROMPT,
    DELEGATED_BUNDLE_PERMISSIONS_PROMPT,
    NUM_PROMPT_TYPES
  };

  // The last prompt type to display; only used for testing.
  static PromptType g_last_prompt_type_for_tests;

  // Enumeration for permissions and retained files details.
  enum DetailsType {
    PERMISSIONS_DETAILS = 0,
    WITHHELD_PERMISSIONS_DETAILS,
    RETAINED_FILES_DETAILS,
    RETAINED_DEVICES_DETAILS,
  };

  // This enum is used to differentiate regular and withheld permissions for
  // segregation in the install prompt.
  enum PermissionsType {
    REGULAR_PERMISSIONS = 0,
    WITHHELD_PERMISSIONS,
    ALL_PERMISSIONS,
  };

  static std::string PromptTypeToString(PromptType type);

  // Extra information needed to display an installation or uninstallation
  // prompt. Gets populated with raw data and exposes getters for formatted
  // strings so that the GTK/views/Cocoa install dialogs don't have to repeat
  // that logic.
  class Prompt {
   public:
    explicit Prompt(PromptType type);
    ~Prompt();

    void SetPermissions(const extensions::PermissionMessages& permissions,
                        PermissionsType permissions_type);
    void SetIsShowingDetails(DetailsType type,
                             size_t index,
                             bool is_showing_details);
    void SetWebstoreData(const std::string& localized_user_count,
                         bool show_user_count,
                         double average_rating,
                         int rating_count);

    PromptType type() const { return type_; }
    void set_type(PromptType type) { type_ = type; }

    // Getters for UI element labels.
    base::string16 GetDialogTitle() const;
    int GetDialogButtons() const;
    // Returns the empty string when there should be no "accept" button.
    base::string16 GetAcceptButtonLabel() const;
    base::string16 GetAbortButtonLabel() const;
    base::string16 GetPermissionsHeading(
        PermissionsType permissions_type) const;
    base::string16 GetRetainedFilesHeading() const;
    base::string16 GetRetainedDevicesHeading() const;

    bool ShouldShowPermissions() const;

    bool ShouldUseTabModalDialog() const;

    // Getters for webstore metadata. Only populated when the type is
    // INLINE_INSTALL_PROMPT, EXTERNAL_INSTALL_PROMPT, or REPAIR_PROMPT.

    // The star display logic replicates the one used by the webstore (from
    // components.ratingutils.setFractionalYellowStars). Callers pass in an
    // "appender", which will be repeatedly called back with the star images
    // that they append to the star display area.
    typedef void(*StarAppender)(const gfx::ImageSkia*, void*);
    void AppendRatingStars(StarAppender appender, void* data) const;
    base::string16 GetRatingCount() const;
    base::string16 GetUserCount() const;
    size_t GetPermissionCount(PermissionsType permissions_type) const;
    size_t GetPermissionsDetailsCount(PermissionsType permissions_type) const;
    base::string16 GetPermission(size_t index,
                                 PermissionsType permissions_type) const;
    base::string16 GetPermissionsDetails(
        size_t index,
        PermissionsType permissions_type) const;
    bool GetIsShowingDetails(DetailsType type, size_t index) const;
    size_t GetRetainedFileCount() const;
    base::string16 GetRetainedFile(size_t index) const;
    size_t GetRetainedDeviceCount() const;
    base::string16 GetRetainedDeviceMessageString(size_t index) const;

    // Populated for BUNDLE_INSTALL_PROMPT and
    // DELEGATED_BUNDLE_PERMISSIONS_PROMPT.
    const extensions::BundleInstaller* bundle() const { return bundle_; }
    void set_bundle(const extensions::BundleInstaller* bundle) {
      bundle_ = bundle;
    }

    // Populated for all other types.
    const extensions::Extension* extension() const { return extension_; }
    void set_extension(const extensions::Extension* extension) {
      extension_ = extension;
    }

    // May be populated for POST_INSTALL_PERMISSIONS_PROMPT.
    void set_retained_files(const std::vector<base::FilePath>& retained_files) {
      retained_files_ = retained_files;
    }
    void set_retained_device_messages(
        const std::vector<base::string16>& retained_device_messages) {
      retained_device_messages_ = retained_device_messages;
    }

    const std::string& delegated_username() const {
      return delegated_username_;
    }
    void set_delegated_username(const std::string& delegated_username) {
      delegated_username_ = delegated_username;
    }

    const gfx::Image& icon() const { return icon_; }
    void set_icon(const gfx::Image& icon) { icon_ = icon; }

    bool has_webstore_data() const { return has_webstore_data_; }

   private:
    friend class base::RefCountedThreadSafe<Prompt>;

    struct InstallPromptPermissions {
      InstallPromptPermissions();
      ~InstallPromptPermissions();

      std::vector<base::string16> permissions;
      std::vector<base::string16> details;
      std::vector<bool> is_showing_details;
    };

    bool ShouldDisplayRevokeButton() const;

    // Returns the InstallPromptPermissions corresponding to
    // |permissions_type|.
    InstallPromptPermissions& GetPermissionsForType(
        PermissionsType permissions_type);
    const InstallPromptPermissions& GetPermissionsForType(
        PermissionsType permissions_type) const;

    bool ShouldDisplayRevokeFilesButton() const;

    PromptType type_;

    // Permissions that are being requested (may not be all of an extension's
    // permissions if only additional ones are being requested)
    InstallPromptPermissions prompt_permissions_;
    // Permissions that will be withheld upon install.
    InstallPromptPermissions withheld_prompt_permissions_;

    bool is_showing_details_for_retained_files_;
    bool is_showing_details_for_retained_devices_;

    // The extension or bundle being installed.
    const extensions::Extension* extension_;
    const extensions::BundleInstaller* bundle_;

    std::string delegated_username_;

    // The icon to be displayed.
    gfx::Image icon_;

    // These fields are populated only when the prompt type is
    // INLINE_INSTALL_PROMPT
    // Already formatted to be locale-specific.
    std::string localized_user_count_;
    // Range is kMinExtensionRating to kMaxExtensionRating
    double average_rating_;
    int rating_count_;

    // Whether we should display the user count (we anticipate this will be
    // false if localized_user_count_ represents the number zero).
    bool show_user_count_;

    // Whether or not this prompt has been populated with data from the
    // webstore.
    bool has_webstore_data_;

    std::vector<base::FilePath> retained_files_;
    std::vector<base::string16> retained_device_messages_;

    DISALLOW_COPY_AND_ASSIGN(Prompt);
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

  typedef base::Callback<void(ExtensionInstallPromptShowParams*,
                              ExtensionInstallPrompt::Delegate*,
                              scoped_ptr<ExtensionInstallPrompt::Prompt>)>
      ShowDialogCallback;

  // Callback to show the default extension install dialog.
  // The implementations of this function are platform-specific.
  static ShowDialogCallback GetDefaultShowDialogCallback();

  // Returns the appropriate prompt type for the given |extension|.
  // TODO(devlin): This method is yucky - callers probably only care about one
  // prompt type. We just need to comb through and figure out what it is.
  static PromptType GetReEnablePromptTypeForExtension(
      content::BrowserContext* context,
      const extensions::Extension* extension);

  // Creates a dummy extension from the |manifest|, replacing the name and
  // description with the localizations if provided.
  static scoped_refptr<extensions::Extension> GetLocalizedExtensionForDisplay(
      const base::DictionaryValue* manifest,
      int flags,  // Extension::InitFromValueFlags
      const std::string& id,
      const std::string& localized_name,
      const std::string& localized_description,
      std::string* error);

  // Creates a prompt with a parent web content.
  explicit ExtensionInstallPrompt(content::WebContents* contents);

  // Creates a prompt with a profile and a native window. The most recently
  // active browser window (or a new browser window if there are no browser
  // windows) is used if a new tab needs to be opened.
  ExtensionInstallPrompt(Profile* profile, gfx::NativeWindow native_window);

  virtual ~ExtensionInstallPrompt();

  extensions::ExtensionInstallUI* install_ui() const {
    return install_ui_.get();
  }

  // Starts the process to show the install dialog. Loads the icon (if |icon| is
  // null), sets up the Prompt, and calls |show_dialog_callback| when ready to
  // show.
  // |extension| can be null in the case of a bndle install.
  // If |icon| is null, this will attempt to load the extension's icon.
  // |prompt| is used to pass in a prompt with additional data (like retained
  // device permissions) or a different type. If not provided, |prompt| will
  // be created as an INSTALL_PROMPT.
  // |custom_permissions| will be used if provided; otherwise, the extensions
  // current permissions are used.
  //
  // We *MUST* eventually call either Proceed() or Abort() on |delegate|.
  void ShowDialog(Delegate* delegate,
                  const extensions::Extension* extension,
                  const SkBitmap* icon,
                  const ShowDialogCallback& show_dialog_callback);
  void ShowDialog(Delegate* delegate,
                  const extensions::Extension* extension,
                  const SkBitmap* icon,
                  scoped_ptr<Prompt> prompt,
                  const ShowDialogCallback& show_dialog_callback);
  // Declared virtual for testing purposes.
  // Note: if all you want to do is automatically confirm or cancel, prefer
  // ScopedTestDialogAutoConfirm from extension_dialog_auto_confirm.h
  virtual void ShowDialog(
      Delegate* delegate,
      const extensions::Extension* extension,
      const SkBitmap* icon,
      scoped_ptr<Prompt> prompt,
      scoped_ptr<const extensions::PermissionSet> custom_permissions,
      const ShowDialogCallback& show_dialog_callback);

  // Installation was successful. This is declared virtual for testing.
  virtual void OnInstallSuccess(const extensions::Extension* extension,
                                SkBitmap* icon);

  // Installation failed. This is declared virtual for testing.
  virtual void OnInstallFailure(const extensions::CrxInstallError& error);

  bool did_call_show_dialog() const { return did_call_show_dialog_; }

 protected:
  friend class extensions::ExtensionWebstorePrivateApiTest;
  friend class WebstoreStartupInstallUnpackFailureTest;

 private:
  friend class GalleryInstallApiTestObserver;

  // Sets the icon that will be used in any UI. If |icon| is NULL, or contains
  // an empty bitmap, then a default icon will be used instead.
  void SetIcon(const SkBitmap* icon);

  // ImageLoader callback.
  void OnImageLoaded(const gfx::Image& image);

  // Starts the process of showing a confirmation UI, which is split into two.
  // 1) Set off a 'load icon' task.
  // 2) Handle the load icon response and show the UI (OnImageLoaded).
  void LoadImageIfNeeded();

  // Shows the actual UI (the icon should already be loaded).
  void ShowConfirmation();

  Profile* profile_;

  base::MessageLoop* ui_loop_;

  // The extensions installation icon.
  SkBitmap icon_;

  // The extension we are showing the UI for, if type is not
  // BUNDLE_INSTALL_PROMPT or DELEGATED_BUNDLE_PERMISSIONS_PROMPT.
  const extensions::Extension* extension_;

  // A custom set of permissions to show in the install prompt instead of the
  // extension's active permissions.
  scoped_ptr<const extensions::PermissionSet> custom_permissions_;

  // The object responsible for doing the UI specific actions.
  scoped_ptr<extensions::ExtensionInstallUI> install_ui_;

  // Parameters to show the confirmation UI.
  scoped_ptr<ExtensionInstallPromptShowParams> show_params_;

  // The delegate we will call Proceed/Abort on after confirmation UI.
  Delegate* delegate_;

  // A pre-filled prompt.
  scoped_ptr<Prompt> prompt_;

  // Used to show the confirm dialog.
  ShowDialogCallback show_dialog_callback_;

  // Whether or not the |show_dialog_callback_| was called.
  bool did_call_show_dialog_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInstallPrompt);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_PROMPT_H_
