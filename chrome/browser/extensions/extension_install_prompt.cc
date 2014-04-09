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
#include "chrome/browser/extensions/image_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/identity/oauth2_manifest_handler.h"
#include "chrome/common/pref_names.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
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
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

using extensions::BundleInstaller;
using extensions::Extension;
using extensions::Manifest;
using extensions::PermissionSet;

namespace {

static const int kTitleIds[ExtensionInstallPrompt::NUM_PROMPT_TYPES] = {
  0,  // The regular install prompt depends on what's being installed.
  IDS_EXTENSION_INLINE_INSTALL_PROMPT_TITLE,
  IDS_EXTENSION_INSTALL_PROMPT_TITLE,
  IDS_EXTENSION_RE_ENABLE_PROMPT_TITLE,
  IDS_EXTENSION_PERMISSIONS_PROMPT_TITLE,
  IDS_EXTENSION_EXTERNAL_INSTALL_PROMPT_TITLE,
  IDS_EXTENSION_POST_INSTALL_PERMISSIONS_PROMPT_TITLE,
  IDS_EXTENSION_LAUNCH_APP_PROMPT_TITLE,
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
};
static const int kPermissionsHeaderIds[
    ExtensionInstallPrompt::NUM_PROMPT_TYPES] = {
  IDS_EXTENSION_PROMPT_WILL_HAVE_ACCESS_TO,
  IDS_EXTENSION_PROMPT_WILL_HAVE_ACCESS_TO,
  IDS_EXTENSION_PROMPT_THESE_WILL_HAVE_ACCESS_TO,
  IDS_EXTENSION_PROMPT_WILL_NOW_HAVE_ACCESS_TO,
  IDS_EXTENSION_PROMPT_WANTS_ACCESS_TO,
  IDS_EXTENSION_PROMPT_WILL_HAVE_ACCESS_TO,
  IDS_EXTENSION_PROMPT_CAN_ACCESS,
  IDS_EXTENSION_PROMPT_WILL_HAVE_ACCESS_TO,
};
static const int kOAuthHeaderIds[ExtensionInstallPrompt::NUM_PROMPT_TYPES] = {
  IDS_EXTENSION_PROMPT_OAUTH_HEADER,
  0,  // Inline installs don't show OAuth permissions.
  0,  // Bundle installs don't show OAuth permissions.
  IDS_EXTENSION_PROMPT_OAUTH_REENABLE_HEADER,
  IDS_EXTENSION_PROMPT_OAUTH_PERMISSIONS_HEADER,
  0,
  0,
  IDS_EXTENSION_PROMPT_OAUTH_HEADER,
};

// Size of extension icon in top left of dialog.
const int kIconSize = 69;

// Returns pixel size under maximal scale factor for the icon whose device
// independent size is |size_in_dip|
int GetSizeForMaxScaleFactor(int size_in_dip) {
  return static_cast<int>(size_in_dip * gfx::ImageSkia::GetMaxSupportedScale());
}

// Returns bitmap for the default icon with size equal to the default icon's
// pixel size under maximal supported scale factor.
SkBitmap GetDefaultIconBitmapForMaxScaleFactor(bool is_app) {
  const gfx::ImageSkia& image = is_app ?
      extensions::IconsInfo::GetDefaultAppIcon() :
      extensions::IconsInfo::GetDefaultExtensionIcon();
  return image.GetRepresentation(
      gfx::ImageSkia::GetMaxSupportedScale()).sk_bitmap();
}

// If auto confirm is enabled then posts a task to proceed with or cancel the
// install and returns true. Otherwise returns false.
bool AutoConfirmPrompt(ExtensionInstallPrompt::Delegate* delegate) {
  const CommandLine* cmdline = CommandLine::ForCurrentProcess();
  if (!cmdline->HasSwitch(switches::kAppsGalleryInstallAutoConfirmForTests))
    return false;
  std::string value = cmdline->GetSwitchValueASCII(
      switches::kAppsGalleryInstallAutoConfirmForTests);

  // We use PostTask instead of calling the delegate directly here, because in
  // the real implementations it's highly likely the message loop will be
  // pumping a few times before the user clicks accept or cancel.
  if (value == "accept") {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&ExtensionInstallPrompt::Delegate::InstallUIProceed,
                   base::Unretained(delegate)));
    return true;
  }

  if (value == "cancel") {
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

  return contents->GetView()->GetTopLevelNativeWindow();
}

}  // namespace

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
    const std::vector<base::string16>& permissions) {
  permissions_ = permissions;
}

void ExtensionInstallPrompt::Prompt::SetPermissionsDetails(
    const std::vector<base::string16>& details) {
  details_ = details;
  is_showing_details_for_permissions_.clear();
  for (size_t i = 0; i < details.size(); ++i)
    is_showing_details_for_permissions_.push_back(false);
}

void ExtensionInstallPrompt::Prompt::SetIsShowingDetails(
    DetailsType type,
    size_t index,
    bool is_showing_details) {
  switch (type) {
    case PERMISSIONS_DETAILS:
      is_showing_details_for_permissions_[index] = is_showing_details;
      break;
    case OAUTH_DETAILS:
      is_showing_details_for_oauth_[index] = is_showing_details;
      break;
    case RETAINED_FILES_DETAILS:
      is_showing_details_for_retained_files_ = is_showing_details;
      break;
  }
}

void ExtensionInstallPrompt::Prompt::SetOAuthIssueAdvice(
    const IssueAdviceInfo& issue_advice) {
  is_showing_details_for_oauth_.clear();
  for (size_t i = 0; i < issue_advice.size(); ++i)
    is_showing_details_for_oauth_.push_back(false);

  oauth_issue_advice_ = issue_advice;
}

void ExtensionInstallPrompt::Prompt::SetUserNameFromProfile(Profile* profile) {
  // |profile| can be NULL in unit tests.
  if (profile) {
    oauth_user_name_ = base::UTF8ToUTF16(profile->GetPrefs()->GetString(
        prefs::kGoogleServicesUsername));
  } else {
    oauth_user_name_.clear();
  }
}

void ExtensionInstallPrompt::Prompt::SetWebstoreData(
    const std::string& localized_user_count,
    bool show_user_count,
    double average_rating,
    int rating_count) {
  CHECK(type_ == INLINE_INSTALL_PROMPT || type_ == EXTERNAL_INSTALL_PROMPT);
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
   return type_ == INSTALL_PROMPT &&
       extension_->is_extension() && experiment_ && experiment_->text_only();
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

base::string16 ExtensionInstallPrompt::Prompt::GetPermissionsHeading() const {
  return l10n_util::GetStringUTF16(kPermissionsHeaderIds[type_]);
}

base::string16 ExtensionInstallPrompt::Prompt::GetOAuthHeading() const {
  return l10n_util::GetStringFUTF16(kOAuthHeaderIds[type_], oauth_user_name_);
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
  return GetPermissionCount() > 0 || type_ == POST_INSTALL_PERMISSIONS_PROMPT;
}

void ExtensionInstallPrompt::Prompt::AppendRatingStars(
    StarAppender appender, void* data) const {
  CHECK(appender);
  CHECK(type_ == INLINE_INSTALL_PROMPT || type_ == EXTERNAL_INSTALL_PROMPT);
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
  CHECK(type_ == INLINE_INSTALL_PROMPT || type_ == EXTERNAL_INSTALL_PROMPT);
  return l10n_util::GetStringFUTF16(IDS_EXTENSION_RATING_COUNT,
                                    base::IntToString16(rating_count_));
}

base::string16 ExtensionInstallPrompt::Prompt::GetUserCount() const {
  CHECK(type_ == INLINE_INSTALL_PROMPT || type_ == EXTERNAL_INSTALL_PROMPT);

  if (show_user_count_) {
    return l10n_util::GetStringFUTF16(IDS_EXTENSION_USER_COUNT,
                                      base::UTF8ToUTF16(localized_user_count_));
  }
  return base::string16();
}

size_t ExtensionInstallPrompt::Prompt::GetPermissionCount() const {
  return permissions_.size();
}

size_t ExtensionInstallPrompt::Prompt::GetPermissionsDetailsCount() const {
  return details_.size();
}

base::string16 ExtensionInstallPrompt::Prompt::GetPermission(size_t index)
    const {
  CHECK_LT(index, permissions_.size());
  return permissions_[index];
}

base::string16 ExtensionInstallPrompt::Prompt::GetPermissionsDetails(
    size_t index) const {
  CHECK_LT(index, details_.size());
  return details_[index];
}

bool ExtensionInstallPrompt::Prompt::GetIsShowingDetails(
    DetailsType type, size_t index) const {
  switch (type) {
    case PERMISSIONS_DETAILS:
      CHECK_LT(index, is_showing_details_for_permissions_.size());
      return is_showing_details_for_permissions_[index];
    case OAUTH_DETAILS:
      CHECK_LT(index, is_showing_details_for_oauth_.size());
      return is_showing_details_for_oauth_[index];
    case RETAINED_FILES_DETAILS:
      return is_showing_details_for_retained_files_;
  }
  return false;
}

size_t ExtensionInstallPrompt::Prompt::GetOAuthIssueCount() const {
  return oauth_issue_advice_.size();
}

const IssueAdviceInfoEntry& ExtensionInstallPrompt::Prompt::GetOAuthIssue(
    size_t index) const {
  CHECK_LT(index, oauth_issue_advice_.size());
  return oauth_issue_advice_[index];
}

size_t ExtensionInstallPrompt::Prompt::GetRetainedFileCount() const {
  return retained_files_.size();
}

base::string16 ExtensionInstallPrompt::Prompt::GetRetainedFile(size_t index)
    const {
  CHECK_LT(index, retained_files_.size());
  return retained_files_[index].AsUTF16Unsafe();
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
    : OAuth2TokenService::Consumer("extensions_install"),
      record_oauth2_grant_(false),
      ui_loop_(base::MessageLoop::current()),
      extension_(NULL),
      bundle_(NULL),
      install_ui_(ExtensionInstallUI::Create(ProfileForWebContents(contents))),
      show_params_(contents),
      delegate_(NULL),
      prompt_(UNSET_PROMPT_TYPE) {
  prompt_.SetUserNameFromProfile(install_ui_->profile());
}

ExtensionInstallPrompt::ExtensionInstallPrompt(
    Profile* profile,
    gfx::NativeWindow native_window,
    content::PageNavigator* navigator)
    : OAuth2TokenService::Consumer("extensions_install"),
      record_oauth2_grant_(false),
      ui_loop_(base::MessageLoop::current()),
      extension_(NULL),
      bundle_(NULL),
      install_ui_(ExtensionInstallUI::Create(profile)),
      show_params_(native_window, navigator),
      delegate_(NULL),
      prompt_(UNSET_PROMPT_TYPE) {
  prompt_.SetUserNameFromProfile(install_ui_->profile());
}

ExtensionInstallPrompt::~ExtensionInstallPrompt() {
}

void ExtensionInstallPrompt::ConfirmBundleInstall(
    extensions::BundleInstaller* bundle,
    const PermissionSet* permissions) {
  DCHECK(ui_loop_ == base::MessageLoop::current());
  bundle_ = bundle;
  permissions_ = permissions;
  delegate_ = bundle;
  prompt_.set_type(BUNDLE_INSTALL_PROMPT);

  ShowConfirmation();
}

void ExtensionInstallPrompt::ConfirmStandaloneInstall(
    Delegate* delegate,
    const Extension* extension,
    SkBitmap* icon,
    const ExtensionInstallPrompt::Prompt& prompt) {
  DCHECK(ui_loop_ == base::MessageLoop::current());
  extension_ = extension;
  permissions_ = extension->GetActivePermissions();
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
  permissions_ = extension->GetActivePermissions();
  delegate_ = delegate;
  prompt_.set_type(INSTALL_PROMPT);
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
  permissions_ = extension->GetActivePermissions();
  delegate_ = delegate;
  prompt_.set_type(extension->is_ephemeral() ? LAUNCH_PROMPT :
                                               RE_ENABLE_PROMPT);

  LoadImageIfNeeded();
}

void ExtensionInstallPrompt::ConfirmExternalInstall(
    Delegate* delegate,
    const Extension* extension,
    const ShowDialogCallback& show_dialog_callback,
    const Prompt& prompt) {
  DCHECK(ui_loop_ == base::MessageLoop::current());
  extension_ = extension;
  permissions_ = extension->GetActivePermissions();
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
  permissions_ = permissions;
  delegate_ = delegate;
  prompt_.set_type(PERMISSIONS_PROMPT);

  LoadImageIfNeeded();
}

void ExtensionInstallPrompt::ConfirmIssueAdvice(
    Delegate* delegate,
    const Extension* extension,
    const IssueAdviceInfo& issue_advice) {
  DCHECK(ui_loop_ == base::MessageLoop::current());
  extension_ = extension;
  delegate_ = delegate;
  prompt_.set_type(PERMISSIONS_PROMPT);

  record_oauth2_grant_ = true;
  prompt_.SetOAuthIssueAdvice(issue_advice);

  LoadImageIfNeeded();
}

void ExtensionInstallPrompt::ReviewPermissions(
    Delegate* delegate,
    const Extension* extension,
    const std::vector<base::FilePath>& retained_file_paths) {
  DCHECK(ui_loop_ == base::MessageLoop::current());
  extension_ = extension;
  permissions_ = extension->GetActivePermissions();
  prompt_.set_retained_files(retained_file_paths);
  delegate_ = delegate;
  prompt_.set_type(POST_INSTALL_PERMISSIONS_PROMPT);

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

  // Load the image asynchronously. For the response, check OnImageLoaded.
  extensions::ExtensionResource image = extensions::IconsInfo::GetIconResource(
      extension_,
      extension_misc::EXTENSION_ICON_LARGE,
      ExtensionIconSet::MATCH_BIGGER);
  // Load the icon whose pixel size is large enough to be displayed under
  // maximal supported scale factor. UI code will scale the icon down if needed.
  // TODO(tbarzic): We should use IconImage here and load the required bitmap
  //     lazily.
  int pixel_size = GetSizeForMaxScaleFactor(kIconSize);
  extensions::ImageLoader::Get(install_ui_->profile())->LoadImageAsync(
      extension_, image, gfx::Size(pixel_size, pixel_size),
      base::Bind(&ExtensionInstallPrompt::OnImageLoaded, AsWeakPtr()));
}

void ExtensionInstallPrompt::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const std::string& access_token,
    const base::Time& expiration_time) {
  DCHECK_EQ(login_token_request_.get(), request);
  login_token_request_.reset();

  const extensions::OAuth2Info& oauth2_info =
      extensions::OAuth2Info::GetOAuth2Info(extension_);

  token_flow_.reset(new OAuth2MintTokenFlow(
      install_ui_->profile()->GetRequestContext(),
      this,
      OAuth2MintTokenFlow::Parameters(
          access_token,
          extension_->id(),
          oauth2_info.client_id,
          oauth2_info.scopes,
          OAuth2MintTokenFlow::MODE_ISSUE_ADVICE)));
  token_flow_->Start();
}

void ExtensionInstallPrompt::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  DCHECK_EQ(login_token_request_.get(), request);
  login_token_request_.reset();
  ShowConfirmation();
}

void ExtensionInstallPrompt::OnIssueAdviceSuccess(
    const IssueAdviceInfo& advice_info) {
  prompt_.SetOAuthIssueAdvice(advice_info);
  record_oauth2_grant_ = true;
  ShowConfirmation();
}

void ExtensionInstallPrompt::OnMintTokenFailure(
    const GoogleServiceAuthError& error) {
  ShowConfirmation();
}

void ExtensionInstallPrompt::ShowConfirmation() {
  if (prompt_.type() == INSTALL_PROMPT)
    prompt_.set_experiment(ExtensionInstallPromptExperiment::Find());
  else
    prompt_.set_experiment(ExtensionInstallPromptExperiment::ControlGroup());

  if (permissions_.get() &&
      (!extension_ ||
       !extensions::PermissionsData::ShouldSkipPermissionWarnings(
           extension_))) {
    Manifest::Type extension_type = extension_ ?
        extension_->GetType() : Manifest::TYPE_UNKNOWN;
    prompt_.SetPermissions(
        extensions::PermissionMessageProvider::Get()->
            GetWarningMessages(permissions_, extension_type));
    prompt_.SetPermissionsDetails(
        extensions::PermissionMessageProvider::Get()->
            GetWarningMessagesDetails(permissions_, extension_type));
  }

  switch (prompt_.type()) {
    case PERMISSIONS_PROMPT:
    case RE_ENABLE_PROMPT:
    case INLINE_INSTALL_PROMPT:
    case EXTERNAL_INSTALL_PROMPT:
    case INSTALL_PROMPT:
    case LAUNCH_PROMPT:
    case POST_INSTALL_PERMISSIONS_PROMPT: {
      prompt_.set_extension(extension_);
      prompt_.set_icon(gfx::Image::CreateFrom1xBitmap(icon_));
      break;
    }
    case BUNDLE_INSTALL_PROMPT: {
      prompt_.set_bundle(bundle_);
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
