// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_install_prompt.h"

#include <map>

#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/bundle_installer.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/permissions_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/image_loader.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/permissions/permission_message_provider.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/url_pattern.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

using extensions::BundleInstaller;
using extensions::Extension;
using extensions::Manifest;
using extensions::PermissionSet;

namespace {

bool AllowWebstoreData(ExtensionInstallPrompt::PromptType type) {
  return type == ExtensionInstallPrompt::INLINE_INSTALL_PROMPT ||
         type == ExtensionInstallPrompt::EXTERNAL_INSTALL_PROMPT ||
         type == ExtensionInstallPrompt::REPAIR_PROMPT;
}

static const int kTitleIds[ExtensionInstallPrompt::NUM_PROMPT_TYPES] = {
    0,  // The regular install prompt depends on what's being installed.
    IDS_EXTENSION_INLINE_INSTALL_PROMPT_TITLE,
    IDS_EXTENSION_INSTALL_PROMPT_TITLE,
    IDS_EXTENSION_RE_ENABLE_PROMPT_TITLE,
    IDS_EXTENSION_PERMISSIONS_PROMPT_TITLE,
    IDS_EXTENSION_EXTERNAL_INSTALL_PROMPT_TITLE,
    IDS_EXTENSION_POST_INSTALL_PERMISSIONS_PROMPT_TITLE,
    IDS_EXTENSION_LAUNCH_APP_PROMPT_TITLE,
    0,  // The remote install prompt depends on what's being installed.
    0,  // The repair install prompt depends on what's being installed.
};
static const int kHeadingIds[ExtensionInstallPrompt::NUM_PROMPT_TYPES] = {
    IDS_EXTENSION_INSTALL_PROMPT_HEADING,
    0,  // Inline installs use the extension name.
    0,  // Heading for bundle installs depends on the bundle contents.
    IDS_EXTENSION_RE_ENABLE_PROMPT_HEADING,
    IDS_EXTENSION_PERMISSIONS_PROMPT_HEADING,
    0,  // External installs use different strings for extensions/apps.
    IDS_EXTENSION_POST_INSTALL_PERMISSIONS_PROMPT_HEADING,
    IDS_EXTENSION_LAUNCH_APP_PROMPT_HEADING,
    IDS_EXTENSION_REMOTE_INSTALL_PROMPT_HEADING,
    IDS_EXTENSION_REPAIR_PROMPT_HEADING
};
static const int kButtons[ExtensionInstallPrompt::NUM_PROMPT_TYPES] = {
    ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL,
    ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL,
    ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL,
    ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL,
    ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL,
    ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL,
    ui::DIALOG_BUTTON_CANCEL,
    ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL,
    ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL,
    ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL,
};
static const int kAcceptButtonIds[ExtensionInstallPrompt::NUM_PROMPT_TYPES] = {
    IDS_EXTENSION_PROMPT_INSTALL_BUTTON,
    IDS_EXTENSION_PROMPT_INSTALL_BUTTON,
    IDS_EXTENSION_PROMPT_INSTALL_BUTTON,
    IDS_EXTENSION_PROMPT_RE_ENABLE_BUTTON,
    IDS_EXTENSION_PROMPT_PERMISSIONS_BUTTON,
    0,  // External installs use different strings for extensions/apps.
    IDS_EXTENSION_PROMPT_PERMISSIONS_CLEAR_RETAINED_FILES_BUTTON,
    IDS_EXTENSION_PROMPT_LAUNCH_BUTTON,
    IDS_EXTENSION_PROMPT_REMOTE_INSTALL_BUTTON,
    IDS_EXTENSION_PROMPT_REPAIR_BUTTON,
};
static const int kAbortButtonIds[ExtensionInstallPrompt::NUM_PROMPT_TYPES] = {
    0,  // These all use the platform's default cancel label.
    0,
    0,
    0,
    IDS_EXTENSION_PROMPT_PERMISSIONS_ABORT_BUTTON,
    IDS_EXTENSION_EXTERNAL_INSTALL_PROMPT_ABORT_BUTTON,
    IDS_CLOSE,
    0,  // Platform dependent cancel button.
    0,
    0,
};
static const int
    kPermissionsHeaderIds[ExtensionInstallPrompt::NUM_PROMPT_TYPES] = {
        IDS_EXTENSION_PROMPT_WILL_HAVE_ACCESS_TO,
        IDS_EXTENSION_PROMPT_WILL_HAVE_ACCESS_TO,
        IDS_EXTENSION_PROMPT_THESE_WILL_HAVE_ACCESS_TO,
        IDS_EXTENSION_PROMPT_WILL_NOW_HAVE_ACCESS_TO,
        IDS_EXTENSION_PROMPT_WANTS_ACCESS_TO,
        IDS_EXTENSION_PROMPT_WILL_HAVE_ACCESS_TO,
        IDS_EXTENSION_PROMPT_CAN_ACCESS,
        IDS_EXTENSION_PROMPT_WILL_HAVE_ACCESS_TO,
        IDS_EXTENSION_PROMPT_WILL_HAVE_ACCESS_TO,
        IDS_EXTENSION_PROMPT_CAN_ACCESS,
};

// Returns bitmap for the default icon with size equal to the default icon's
// pixel size under maximal supported scale factor.
SkBitmap GetDefaultIconBitmapForMaxScaleFactor(bool is_app) {
  const gfx::ImageSkia& image = is_app ?
      extensions::util::GetDefaultAppIcon() :
      extensions::util::GetDefaultExtensionIcon();
  return image.GetRepresentation(
      gfx::ImageSkia::GetMaxSupportedScale()).sk_bitmap();
}

// If auto confirm is enabled then posts a task to proceed with or cancel the
// install and returns true. Otherwise returns false.
bool AutoConfirmPrompt(ExtensionInstallPrompt::Delegate* delegate) {
  switch (ExtensionInstallPrompt::g_auto_confirm_for_tests) {
    case ExtensionInstallPrompt::NONE:
      return false;
    // We use PostTask instead of calling the delegate directly here, because in
    // the real implementations it's highly likely the message loop will be
    // pumping a few times before the user clicks accept or cancel.
    case ExtensionInstallPrompt::ACCEPT:
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&ExtensionInstallPrompt::Delegate::InstallUIProceed,
                     base::Unretained(delegate)));
      return true;
    case ExtensionInstallPrompt::CANCEL:
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&ExtensionInstallPrompt::Delegate::InstallUIAbort,
                     base::Unretained(delegate),
                     true));
      return true;
  }

  NOTREACHED();
  return false;
}

Profile* ProfileForWebContents(content::WebContents* web_contents) {
  if (!web_contents)
    return NULL;
  return Profile::FromBrowserContext(web_contents->GetBrowserContext());
}

gfx::NativeWindow NativeWindowForWebContents(content::WebContents* contents) {
  if (!contents)
    return NULL;

  return contents->GetTopLevelNativeWindow();
}

}  // namespace

ExtensionInstallPrompt::Prompt::InstallPromptPermissions::
    InstallPromptPermissions() {
}
ExtensionInstallPrompt::Prompt::InstallPromptPermissions::
    ~InstallPromptPermissions() {
}

// static
ExtensionInstallPrompt::AutoConfirmForTests
ExtensionInstallPrompt::g_auto_confirm_for_tests = ExtensionInstallPrompt::NONE;

// This should match the PromptType enum.
std::string ExtensionInstallPrompt::PromptTypeToString(PromptType type) {
  switch (type) {
    case ExtensionInstallPrompt::INSTALL_PROMPT:
      return "INSTALL_PROMPT";
    case ExtensionInstallPrompt::INLINE_INSTALL_PROMPT:
      return "INLINE_INSTALL_PROMPT";
    case ExtensionInstallPrompt::BUNDLE_INSTALL_PROMPT:
      return "BUNDLE_INSTALL_PROMPT";
    case ExtensionInstallPrompt::RE_ENABLE_PROMPT:
      return "RE_ENABLE_PROMPT";
    case ExtensionInstallPrompt::PERMISSIONS_PROMPT:
      return "PERMISSIONS_PROMPT";
    case ExtensionInstallPrompt::EXTERNAL_INSTALL_PROMPT:
      return "EXTERNAL_INSTALL_PROMPT";
    case ExtensionInstallPrompt::POST_INSTALL_PERMISSIONS_PROMPT:
      return "POST_INSTALL_PERMISSIONS_PROMPT";
    case ExtensionInstallPrompt::LAUNCH_PROMPT:
      return "LAUNCH_PROMPT";
    case ExtensionInstallPrompt::REMOTE_INSTALL_PROMPT:
      return "REMOTE_INSTALL_PROMPT";
    case ExtensionInstallPrompt::REPAIR_PROMPT:
      return "REPAIR_PROMPT";
    case ExtensionInstallPrompt::UNSET_PROMPT_TYPE:
    case ExtensionInstallPrompt::NUM_PROMPT_TYPES:
      break;
  }
  return "OTHER";
}

ExtensionInstallPrompt::Prompt::Prompt(PromptType type)
    : type_(type),
      is_showing_details_for_retained_files_(false),
      extension_(NULL),
      bundle_(NULL),
      average_rating_(0.0),
      rating_count_(0),
      show_user_count_(false),
      has_webstore_data_(false) {
}

ExtensionInstallPrompt::Prompt::~Prompt() {
}

void ExtensionInstallPrompt::Prompt::SetPermissions(
    const std::vector<base::string16>& permissions,
    PermissionsType permissions_type) {
  GetPermissionsForType(permissions_type).permissions = permissions;
}

void ExtensionInstallPrompt::Prompt::SetPermissionsDetails(
    const std::vector<base::string16>& details,
    PermissionsType permissions_type) {
  InstallPromptPermissions& install_permissions =
      GetPermissionsForType(permissions_type);
  install_permissions.details = details;
  install_permissions.is_showing_details.clear();
  install_permissions.is_showing_details.insert(
      install_permissions.is_showing_details.begin(), details.size(), false);
}

void ExtensionInstallPrompt::Prompt::SetIsShowingDetails(
    DetailsType type,
    size_t index,
    bool is_showing_details) {
  switch (type) {
    case PERMISSIONS_DETAILS:
      prompt_permissions_.is_showing_details[index] = is_showing_details;
      break;
    case WITHHELD_PERMISSIONS_DETAILS:
      withheld_prompt_permissions_.is_showing_details[index] =
          is_showing_details;
      break;
    case RETAINED_FILES_DETAILS:
      is_showing_details_for_retained_files_ = is_showing_details;
      break;
  }
}

void ExtensionInstallPrompt::Prompt::SetWebstoreData(
    const std::string& localized_user_count,
    bool show_user_count,
    double average_rating,
    int rating_count) {
  CHECK(AllowWebstoreData(type_));
  localized_user_count_ = localized_user_count;
  show_user_count_ = show_user_count;
  average_rating_ = average_rating;
  rating_count_ = rating_count;
  has_webstore_data_ = true;
}

base::string16 ExtensionInstallPrompt::Prompt::GetDialogTitle() const {
  int resource_id = kTitleIds[type_];

  if (type_ == INSTALL_PROMPT) {
    if (extension_->is_app())
      resource_id = IDS_EXTENSION_INSTALL_APP_PROMPT_TITLE;
    else if (extension_->is_theme())
      resource_id = IDS_EXTENSION_INSTALL_THEME_PROMPT_TITLE;
    else
      resource_id = IDS_EXTENSION_INSTALL_EXTENSION_PROMPT_TITLE;
  } else if (type_ == EXTERNAL_INSTALL_PROMPT) {
    return l10n_util::GetStringFUTF16(
        resource_id, base::UTF8ToUTF16(extension_->name()));
  } else if (type_ == REMOTE_INSTALL_PROMPT) {
    if (extension_->is_app())
      resource_id = IDS_EXTENSION_REMOTE_INSTALL_APP_PROMPT_TITLE;
    else
      resource_id = IDS_EXTENSION_REMOTE_INSTALL_EXTENSION_PROMPT_TITLE;
  } else if (type_ == REPAIR_PROMPT) {
    if (extension_->is_app())
      resource_id = IDS_EXTENSION_REPAIR_APP_PROMPT_TITLE;
    else
      resource_id = IDS_EXTENSION_REPAIR_EXTENSION_PROMPT_TITLE;
  }

  return l10n_util::GetStringUTF16(resource_id);
}

base::string16 ExtensionInstallPrompt::Prompt::GetHeading() const {
  if (type_ == INLINE_INSTALL_PROMPT) {
    return base::UTF8ToUTF16(extension_->name());
  } else if (type_ == BUNDLE_INSTALL_PROMPT) {
    return bundle_->GetHeadingTextFor(BundleInstaller::Item::STATE_PENDING);
  } else if (type_ == EXTERNAL_INSTALL_PROMPT) {
    int resource_id = -1;
    if (extension_->is_app())
      resource_id = IDS_EXTENSION_EXTERNAL_INSTALL_PROMPT_HEADING_APP;
    else if (extension_->is_theme())
      resource_id = IDS_EXTENSION_EXTERNAL_INSTALL_PROMPT_HEADING_THEME;
    else
      resource_id = IDS_EXTENSION_EXTERNAL_INSTALL_PROMPT_HEADING_EXTENSION;
    return l10n_util::GetStringUTF16(resource_id);
  } else {
    return l10n_util::GetStringFUTF16(
        kHeadingIds[type_], base::UTF8ToUTF16(extension_->name()));
  }
}

int ExtensionInstallPrompt::Prompt::GetDialogButtons() const {
  if (type_ == POST_INSTALL_PERMISSIONS_PROMPT &&
      ShouldDisplayRevokeFilesButton()) {
    return kButtons[type_] | ui::DIALOG_BUTTON_OK;
  }

  return kButtons[type_];
}

bool ExtensionInstallPrompt::Prompt::ShouldShowExplanationText() const {
  return type_ == INSTALL_PROMPT && extension_->is_extension() &&
         experiment_.get() && experiment_->text_only();
}

bool ExtensionInstallPrompt::Prompt::HasAcceptButtonLabel() const {
  if (kAcceptButtonIds[type_] == 0)
    return false;

  if (type_ == POST_INSTALL_PERMISSIONS_PROMPT)
    return ShouldDisplayRevokeFilesButton();

  return true;
}

base::string16 ExtensionInstallPrompt::Prompt::GetAcceptButtonLabel() const {
  if (type_ == EXTERNAL_INSTALL_PROMPT) {
    int id = -1;
    if (extension_->is_app())
      id = IDS_EXTENSION_EXTERNAL_INSTALL_PROMPT_ACCEPT_BUTTON_APP;
    else if (extension_->is_theme())
      id = IDS_EXTENSION_EXTERNAL_INSTALL_PROMPT_ACCEPT_BUTTON_THEME;
    else
      id = IDS_EXTENSION_EXTERNAL_INSTALL_PROMPT_ACCEPT_BUTTON_EXTENSION;
    return l10n_util::GetStringUTF16(id);
  }
  if (ShouldShowExplanationText())
    return experiment_->GetOkButtonText();
  return l10n_util::GetStringUTF16(kAcceptButtonIds[type_]);
}

bool ExtensionInstallPrompt::Prompt::HasAbortButtonLabel() const {
  if (ShouldShowExplanationText())
    return true;
  return kAbortButtonIds[type_] > 0;
}

base::string16 ExtensionInstallPrompt::Prompt::GetAbortButtonLabel() const {
  CHECK(HasAbortButtonLabel());
  if (ShouldShowExplanationText())
    return experiment_->GetCancelButtonText();
  return l10n_util::GetStringUTF16(kAbortButtonIds[type_]);
}

base::string16 ExtensionInstallPrompt::Prompt::GetPermissionsHeading(
    PermissionsType permissions_type) const {
  switch (permissions_type) {
    case REGULAR_PERMISSIONS:
      return l10n_util::GetStringUTF16(kPermissionsHeaderIds[type_]);
    case WITHHELD_PERMISSIONS:
      return l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_WITHHELD);
    case ALL_PERMISSIONS:
    default:
      NOTREACHED();
      return base::string16();
  }
}

base::string16 ExtensionInstallPrompt::Prompt::GetRetainedFilesHeading() const {
  const int kRetainedFilesMessageIDs[6] = {
      IDS_EXTENSION_PROMPT_RETAINED_FILES_DEFAULT,
      IDS_EXTENSION_PROMPT_RETAINED_FILE_SINGULAR,
      IDS_EXTENSION_PROMPT_RETAINED_FILES_ZERO,
      IDS_EXTENSION_PROMPT_RETAINED_FILES_TWO,
      IDS_EXTENSION_PROMPT_RETAINED_FILES_FEW,
      IDS_EXTENSION_PROMPT_RETAINED_FILES_MANY,
  };
  std::vector<int> message_ids;
  for (size_t i = 0; i < arraysize(kRetainedFilesMessageIDs); i++) {
    message_ids.push_back(kRetainedFilesMessageIDs[i]);
  }
  return l10n_util::GetPluralStringFUTF16(message_ids, GetRetainedFileCount());
}

bool ExtensionInstallPrompt::Prompt::ShouldShowPermissions() const {
  return GetPermissionCount(ALL_PERMISSIONS) > 0 ||
         type_ == POST_INSTALL_PERMISSIONS_PROMPT;
}

void ExtensionInstallPrompt::Prompt::AppendRatingStars(
    StarAppender appender, void* data) const {
  CHECK(appender);
  CHECK(AllowWebstoreData(type_));
  int rating_integer = floor(average_rating_);
  double rating_fractional = average_rating_ - rating_integer;

  if (rating_fractional > 0.66) {
    rating_integer++;
  }

  if (rating_fractional < 0.33 || rating_fractional > 0.66) {
    rating_fractional = 0;
  }

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  int i;
  for (i = 0; i < rating_integer; i++) {
    appender(rb.GetImageSkiaNamed(IDR_EXTENSIONS_RATING_STAR_ON), data);
  }
  if (rating_fractional) {
    appender(rb.GetImageSkiaNamed(IDR_EXTENSIONS_RATING_STAR_HALF_LEFT), data);
    i++;
  }
  for (; i < kMaxExtensionRating; i++) {
    appender(rb.GetImageSkiaNamed(IDR_EXTENSIONS_RATING_STAR_OFF), data);
  }
}

base::string16 ExtensionInstallPrompt::Prompt::GetRatingCount() const {
  CHECK(AllowWebstoreData(type_));
  return l10n_util::GetStringFUTF16(IDS_EXTENSION_RATING_COUNT,
                                    base::IntToString16(rating_count_));
}

base::string16 ExtensionInstallPrompt::Prompt::GetUserCount() const {
  CHECK(AllowWebstoreData(type_));

  if (show_user_count_) {
    return l10n_util::GetStringFUTF16(IDS_EXTENSION_USER_COUNT,
                                      base::UTF8ToUTF16(localized_user_count_));
  }
  return base::string16();
}

size_t ExtensionInstallPrompt::Prompt::GetPermissionCount(
    PermissionsType permissions_type) const {
  switch (permissions_type) {
    case REGULAR_PERMISSIONS:
      return prompt_permissions_.permissions.size();
    case WITHHELD_PERMISSIONS:
      return withheld_prompt_permissions_.permissions.size();
    case ALL_PERMISSIONS:
      return prompt_permissions_.permissions.size() +
             withheld_prompt_permissions_.permissions.size();
    default:
      NOTREACHED();
      return 0u;
  }
}

size_t ExtensionInstallPrompt::Prompt::GetPermissionsDetailsCount(
    PermissionsType permissions_type) const {
  switch (permissions_type) {
    case REGULAR_PERMISSIONS:
      return prompt_permissions_.details.size();
    case WITHHELD_PERMISSIONS:
      return withheld_prompt_permissions_.details.size();
    case ALL_PERMISSIONS:
      return prompt_permissions_.details.size() +
             withheld_prompt_permissions_.details.size();
    default:
      NOTREACHED();
      return 0u;
  }
}

base::string16 ExtensionInstallPrompt::Prompt::GetPermission(
    size_t index,
    PermissionsType permissions_type) const {
  const InstallPromptPermissions& install_permissions =
      GetPermissionsForType(permissions_type);
  CHECK_LT(index, install_permissions.permissions.size());
  return install_permissions.permissions[index];
}

base::string16 ExtensionInstallPrompt::Prompt::GetPermissionsDetails(
    size_t index,
    PermissionsType permissions_type) const {
  const InstallPromptPermissions& install_permissions =
      GetPermissionsForType(permissions_type);
  CHECK_LT(index, install_permissions.details.size());
  return install_permissions.details[index];
}

bool ExtensionInstallPrompt::Prompt::GetIsShowingDetails(
    DetailsType type, size_t index) const {
  switch (type) {
    case PERMISSIONS_DETAILS:
      CHECK_LT(index, prompt_permissions_.is_showing_details.size());
      return prompt_permissions_.is_showing_details[index];
    case WITHHELD_PERMISSIONS_DETAILS:
      CHECK_LT(index, withheld_prompt_permissions_.is_showing_details.size());
      return withheld_prompt_permissions_.is_showing_details[index];
    case RETAINED_FILES_DETAILS:
      return is_showing_details_for_retained_files_;
  }
  return false;
}

size_t ExtensionInstallPrompt::Prompt::GetRetainedFileCount() const {
  return retained_files_.size();
}

base::string16 ExtensionInstallPrompt::Prompt::GetRetainedFile(size_t index)
    const {
  CHECK_LT(index, retained_files_.size());
  return retained_files_[index].AsUTF16Unsafe();
}

ExtensionInstallPrompt::Prompt::InstallPromptPermissions&
ExtensionInstallPrompt::Prompt::GetPermissionsForType(
    PermissionsType permissions_type) {
  DCHECK_NE(ALL_PERMISSIONS, permissions_type);
  return permissions_type == REGULAR_PERMISSIONS ? prompt_permissions_
                                                 : withheld_prompt_permissions_;
}

const ExtensionInstallPrompt::Prompt::InstallPromptPermissions&
ExtensionInstallPrompt::Prompt::GetPermissionsForType(
    PermissionsType permissions_type) const {
  DCHECK_NE(ALL_PERMISSIONS, permissions_type);
  return permissions_type == REGULAR_PERMISSIONS ? prompt_permissions_
                                                 : withheld_prompt_permissions_;
}

bool ExtensionInstallPrompt::Prompt::ShouldDisplayRevokeFilesButton() const {
  return !retained_files_.empty();
}

ExtensionInstallPrompt::ShowParams::ShowParams(content::WebContents* contents)
    : parent_web_contents(contents),
      parent_window(NativeWindowForWebContents(contents)),
      navigator(contents) {
}

ExtensionInstallPrompt::ShowParams::ShowParams(
    gfx::NativeWindow window,
    content::PageNavigator* navigator)
    : parent_web_contents(NULL),
      parent_window(window),
      navigator(navigator) {
}

// static
scoped_refptr<Extension>
    ExtensionInstallPrompt::GetLocalizedExtensionForDisplay(
    const base::DictionaryValue* manifest,
    int flags,
    const std::string& id,
    const std::string& localized_name,
    const std::string& localized_description,
    std::string* error) {
  scoped_ptr<base::DictionaryValue> localized_manifest;
  if (!localized_name.empty() || !localized_description.empty()) {
    localized_manifest.reset(manifest->DeepCopy());
    if (!localized_name.empty()) {
      localized_manifest->SetString(extensions::manifest_keys::kName,
                                    localized_name);
    }
    if (!localized_description.empty()) {
      localized_manifest->SetString(extensions::manifest_keys::kDescription,
                                    localized_description);
    }
  }

  return Extension::Create(
      base::FilePath(),
      Manifest::INTERNAL,
      localized_manifest.get() ? *localized_manifest.get() : *manifest,
      flags,
      id,
      error);
}

ExtensionInstallPrompt::ExtensionInstallPrompt(content::WebContents* contents)
    : ui_loop_(base::MessageLoop::current()),
      extension_(NULL),
      bundle_(NULL),
      install_ui_(ExtensionInstallUI::Create(ProfileForWebContents(contents))),
      show_params_(contents),
      delegate_(NULL) {
}

ExtensionInstallPrompt::ExtensionInstallPrompt(
    Profile* profile,
    gfx::NativeWindow native_window,
    content::PageNavigator* navigator)
    : ui_loop_(base::MessageLoop::current()),
      extension_(NULL),
      bundle_(NULL),
      install_ui_(ExtensionInstallUI::Create(profile)),
      show_params_(native_window, navigator),
      delegate_(NULL) {
}

ExtensionInstallPrompt::~ExtensionInstallPrompt() {
}

void ExtensionInstallPrompt::ConfirmBundleInstall(
    extensions::BundleInstaller* bundle,
    const PermissionSet* permissions) {
  DCHECK(ui_loop_ == base::MessageLoop::current());
  bundle_ = bundle;
  custom_permissions_ = permissions;
  delegate_ = bundle;
  prompt_ = new Prompt(BUNDLE_INSTALL_PROMPT);

  ShowConfirmation();
}

void ExtensionInstallPrompt::ConfirmStandaloneInstall(
    Delegate* delegate,
    const Extension* extension,
    SkBitmap* icon,
    scoped_refptr<Prompt> prompt) {
  DCHECK(ui_loop_ == base::MessageLoop::current());
  extension_ = extension;
  delegate_ = delegate;
  prompt_ = prompt;

  SetIcon(icon);
  ShowConfirmation();
}

void ExtensionInstallPrompt::ConfirmWebstoreInstall(
    Delegate* delegate,
    const Extension* extension,
    const SkBitmap* icon,
    const ShowDialogCallback& show_dialog_callback) {
  // SetIcon requires |extension_| to be set. ConfirmInstall will setup the
  // remaining fields.
  extension_ = extension;
  SetIcon(icon);
  ConfirmInstall(delegate, extension, show_dialog_callback);
}

void ExtensionInstallPrompt::ConfirmInstall(
    Delegate* delegate,
    const Extension* extension,
    const ShowDialogCallback& show_dialog_callback) {
  DCHECK(ui_loop_ == base::MessageLoop::current());
  extension_ = extension;
  delegate_ = delegate;
  prompt_ = new Prompt(INSTALL_PROMPT);
  show_dialog_callback_ = show_dialog_callback;

  // We special-case themes to not show any confirm UI. Instead they are
  // immediately installed, and then we show an infobar (see OnInstallSuccess)
  // to allow the user to revert if they don't like it.
  //
  // We don't do this in the case where off-store extension installs are
  // disabled because in that case, we don't show the dangerous download UI, so
  // we need the UI confirmation.
  if (extension->is_theme()) {
    if (extension->from_webstore() ||
        extensions::FeatureSwitch::easy_off_store_install()->IsEnabled()) {
      delegate->InstallUIProceed();
      return;
    }
  }

  LoadImageIfNeeded();
}

void ExtensionInstallPrompt::ConfirmReEnable(Delegate* delegate,
                                             const Extension* extension) {
  DCHECK(ui_loop_ == base::MessageLoop::current());
  extension_ = extension;
  delegate_ = delegate;
  bool is_remote_install =
      install_ui_->profile() &&
      extensions::ExtensionPrefs::Get(install_ui_->profile())->HasDisableReason(
          extension->id(), extensions::Extension::DISABLE_REMOTE_INSTALL);
  bool is_ephemeral =
      extensions::util::IsEphemeralApp(extension->id(), install_ui_->profile());

  PromptType type = UNSET_PROMPT_TYPE;
  if (is_ephemeral)
    type = LAUNCH_PROMPT;
  else if (is_remote_install)
    type = REMOTE_INSTALL_PROMPT;
  else
    type = RE_ENABLE_PROMPT;
  prompt_ = new Prompt(type);

  LoadImageIfNeeded();
}

void ExtensionInstallPrompt::ConfirmExternalInstall(
    Delegate* delegate,
    const Extension* extension,
    const ShowDialogCallback& show_dialog_callback,
    scoped_refptr<Prompt> prompt) {
  DCHECK(ui_loop_ == base::MessageLoop::current());
  extension_ = extension;
  delegate_ = delegate;
  prompt_ = prompt;
  show_dialog_callback_ = show_dialog_callback;

  LoadImageIfNeeded();
}

void ExtensionInstallPrompt::ConfirmPermissions(
    Delegate* delegate,
    const Extension* extension,
    const PermissionSet* permissions) {
  DCHECK(ui_loop_ == base::MessageLoop::current());
  extension_ = extension;
  custom_permissions_ = permissions;
  delegate_ = delegate;
  prompt_ = new Prompt(PERMISSIONS_PROMPT);

  LoadImageIfNeeded();
}

void ExtensionInstallPrompt::ReviewPermissions(
    Delegate* delegate,
    const Extension* extension,
    const std::vector<base::FilePath>& retained_file_paths) {
  DCHECK(ui_loop_ == base::MessageLoop::current());
  extension_ = extension;
  prompt_ = new Prompt(POST_INSTALL_PERMISSIONS_PROMPT);
  prompt_->set_retained_files(retained_file_paths);
  delegate_ = delegate;

  LoadImageIfNeeded();
}

void ExtensionInstallPrompt::OnInstallSuccess(const Extension* extension,
                                              SkBitmap* icon) {
  extension_ = extension;
  SetIcon(icon);

  install_ui_->OnInstallSuccess(extension, &icon_);
}

void ExtensionInstallPrompt::OnInstallFailure(
    const extensions::CrxInstallerError& error) {
  install_ui_->OnInstallFailure(error);
}

void ExtensionInstallPrompt::SetIcon(const SkBitmap* image) {
  if (image)
    icon_ = *image;
  else
    icon_ = SkBitmap();
  if (icon_.empty()) {
    // Let's set default icon bitmap whose size is equal to the default icon's
    // pixel size under maximal supported scale factor. If the bitmap is larger
    // than the one we need, it will be scaled down by the ui code.
    icon_ = GetDefaultIconBitmapForMaxScaleFactor(extension_->is_app());
  }
}

void ExtensionInstallPrompt::OnImageLoaded(const gfx::Image& image) {
  SetIcon(image.IsEmpty() ? NULL : image.ToSkBitmap());
  ShowConfirmation();
}

void ExtensionInstallPrompt::LoadImageIfNeeded() {
  // Bundle install prompts do not have an icon.
  // Also |install_ui_.profile()| can be NULL in unit tests.
  if (!icon_.empty() || !install_ui_->profile()) {
    ShowConfirmation();
    return;
  }

  extensions::ExtensionResource image = extensions::IconsInfo::GetIconResource(
      extension_,
      extension_misc::EXTENSION_ICON_LARGE,
      ExtensionIconSet::MATCH_BIGGER);

  // Load the image asynchronously. The response will be sent to OnImageLoaded.
  extensions::ImageLoader* loader =
      extensions::ImageLoader::Get(install_ui_->profile());

  std::vector<extensions::ImageLoader::ImageRepresentation> images_list;
  images_list.push_back(extensions::ImageLoader::ImageRepresentation(
      image,
      extensions::ImageLoader::ImageRepresentation::NEVER_RESIZE,
      gfx::Size(),
      ui::SCALE_FACTOR_100P));
  loader->LoadImagesAsync(
      extension_,
      images_list,
      base::Bind(&ExtensionInstallPrompt::OnImageLoaded, AsWeakPtr()));
}

void ExtensionInstallPrompt::ShowConfirmation() {
  if (prompt_->type() == INSTALL_PROMPT)
    prompt_->set_experiment(ExtensionInstallPromptExperiment::Find());
  else
    prompt_->set_experiment(ExtensionInstallPromptExperiment::ControlGroup());

  scoped_refptr<const PermissionSet> permissions_to_display;
  if (custom_permissions_.get()) {
    permissions_to_display = custom_permissions_;
  } else if (extension_) {
    // Initialize permissions if they have not already been set so that
    // withheld permissions are displayed properly in the install prompt.
    extensions::PermissionsUpdater(
        install_ui_->profile(),
        extensions::PermissionsUpdater::INIT_FLAG_TRANSIENT)
        .InitializePermissions(extension_);
    permissions_to_display =
        extension_->permissions_data()->active_permissions();
  }

  if (permissions_to_display.get() &&
      (!extension_ ||
       !extensions::PermissionsData::ShouldSkipPermissionWarnings(
           extension_->id()))) {
    Manifest::Type type =
        extension_ ? extension_->GetType() : Manifest::TYPE_UNKNOWN;
    const extensions::PermissionMessageProvider* message_provider =
        extensions::PermissionMessageProvider::Get();
    prompt_->SetPermissions(message_provider->GetWarningMessages(
                                permissions_to_display.get(), type),
                            REGULAR_PERMISSIONS);
    prompt_->SetPermissionsDetails(message_provider->GetWarningMessagesDetails(
                                       permissions_to_display.get(), type),
                                   REGULAR_PERMISSIONS);

    scoped_refptr<const extensions::PermissionSet> withheld =
        extension_->permissions_data()->withheld_permissions();
    if (!withheld->IsEmpty()) {
      prompt_->SetPermissions(
          message_provider->GetWarningMessages(withheld.get(), type),
          PermissionsType::WITHHELD_PERMISSIONS);
      prompt_->SetPermissionsDetails(
          message_provider->GetWarningMessagesDetails(withheld.get(), type),
          PermissionsType::WITHHELD_PERMISSIONS);
    }
  }

  switch (prompt_->type()) {
    case PERMISSIONS_PROMPT:
    case RE_ENABLE_PROMPT:
    case INLINE_INSTALL_PROMPT:
    case EXTERNAL_INSTALL_PROMPT:
    case INSTALL_PROMPT:
    case LAUNCH_PROMPT:
    case POST_INSTALL_PERMISSIONS_PROMPT:
    case REMOTE_INSTALL_PROMPT:
    case REPAIR_PROMPT: {
      prompt_->set_extension(extension_);
      prompt_->set_icon(gfx::Image::CreateFrom1xBitmap(icon_));
      break;
    }
    case BUNDLE_INSTALL_PROMPT: {
      prompt_->set_bundle(bundle_);
      break;
    }
    default:
      NOTREACHED() << "Unknown message";
      return;
  }

  if (AutoConfirmPrompt(delegate_))
    return;

  if (show_dialog_callback_.is_null())
    GetDefaultShowDialogCallback().Run(show_params_, delegate_, prompt_);
  else
    show_dialog_callback_.Run(show_params_, delegate_, prompt_);
}
