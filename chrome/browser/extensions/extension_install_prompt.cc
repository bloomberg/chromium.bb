// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_install_prompt.h"

#include <map>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/bundle_installer.h"
#include "chrome/browser/extensions/extension_install_dialog.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/extension_resource.h"
#include "chrome/common/extensions/extension_switch_utils.h"
#include "chrome/common/extensions/permissions/permission_set.h"
#include "chrome/common/extensions/url_pattern.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/page_navigator.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

using extensions::BundleInstaller;
using extensions::Extension;
using extensions::PermissionSet;

static const int kTitleIds[ExtensionInstallPrompt::NUM_PROMPT_TYPES] = {
  0,  // The regular install prompt depends on what's being installed.
  IDS_EXTENSION_INLINE_INSTALL_PROMPT_TITLE,
  IDS_EXTENSION_INSTALL_PROMPT_TITLE,
  IDS_EXTENSION_RE_ENABLE_PROMPT_TITLE,
  IDS_EXTENSION_PERMISSIONS_PROMPT_TITLE
};
static const int kHeadingIds[ExtensionInstallPrompt::NUM_PROMPT_TYPES] = {
  IDS_EXTENSION_INSTALL_PROMPT_HEADING,
  0,  // Inline installs use the extension name.
  0,  // Heading for bundle installs depends on the bundle contents.
  IDS_EXTENSION_RE_ENABLE_PROMPT_HEADING,
  IDS_EXTENSION_PERMISSIONS_PROMPT_HEADING
};
static const int kAcceptButtonIds[ExtensionInstallPrompt::NUM_PROMPT_TYPES] = {
  IDS_EXTENSION_PROMPT_INSTALL_BUTTON,
  IDS_EXTENSION_PROMPT_INSTALL_BUTTON,
  IDS_EXTENSION_PROMPT_INSTALL_BUTTON,
  IDS_EXTENSION_PROMPT_RE_ENABLE_BUTTON,
  IDS_EXTENSION_PROMPT_PERMISSIONS_BUTTON
};
static const int kAbortButtonIds[ExtensionInstallPrompt::NUM_PROMPT_TYPES] = {
  0,  // These all use the platform's default cancel label.
  0,
  0,
  0,
  IDS_EXTENSION_PROMPT_PERMISSIONS_ABORT_BUTTON
};
static const int kPermissionsHeaderIds[
    ExtensionInstallPrompt::NUM_PROMPT_TYPES] = {
  IDS_EXTENSION_PROMPT_WILL_HAVE_ACCESS_TO,
  IDS_EXTENSION_PROMPT_WILL_HAVE_ACCESS_TO,
  IDS_EXTENSION_PROMPT_THESE_WILL_HAVE_ACCESS_TO,
  IDS_EXTENSION_PROMPT_WILL_NOW_HAVE_ACCESS_TO,
  IDS_EXTENSION_PROMPT_WANTS_ACCESS_TO,
};
static const int kOAuthHeaderIds[
    ExtensionInstallPrompt::NUM_PROMPT_TYPES] = {
  IDS_EXTENSION_PROMPT_OAUTH_HEADER,
  0,  // Inline installs don't show OAuth permissions.
  0,  // Bundle installs don't show OAuth permissions.
  IDS_EXTENSION_PROMPT_OAUTH_REENABLE_HEADER,
  IDS_EXTENSION_PROMPT_OAUTH_PERMISSIONS_HEADER,
};

namespace {

// Size of extension icon in top left of dialog.
const int kIconSize = 69;

// Returns pixel size under maximal scale factor for the icon whose device
// independent size is |size_in_dip|
int GetSizeForMaxScaleFactor(int size_in_dip) {
  std::vector<ui::ScaleFactor> supported_scale_factors =
      ui::GetSupportedScaleFactors();
  // Scale factors are in ascending order, so the last one is the one we need.
  ui::ScaleFactor max_scale_factor = supported_scale_factors.back();
  float max_scale_factor_scale = ui::GetScaleFactorScale(max_scale_factor);

  return static_cast<int>(size_in_dip * max_scale_factor_scale);
}

// Returns bitmap for the default icon with size equal to the default icon's
// pixel size under maximal supported scale factor.
SkBitmap GetDefaultIconBitmapForMaxScaleFactor(bool is_app) {
  std::vector<ui::ScaleFactor> supported_scale_factors =
      ui::GetSupportedScaleFactors();
  // Scale factors are in ascending order, so the last one is the one we need.
  ui::ScaleFactor max_scale_factor =
      supported_scale_factors[supported_scale_factors.size() - 1];

  return Extension::GetDefaultIcon(is_app).
      GetRepresentation(max_scale_factor).sk_bitmap();
}

}  // namespace

ExtensionInstallPrompt::Prompt::Prompt(Profile* profile, PromptType type)
    : type_(type),
      extension_(NULL),
      bundle_(NULL),
      average_rating_(0.0),
      rating_count_(0),
      profile_(profile) {
}

ExtensionInstallPrompt::Prompt::~Prompt() {
}

void ExtensionInstallPrompt::Prompt::SetPermissions(
    const std::vector<string16>& permissions) {
  permissions_ = permissions;
}

void ExtensionInstallPrompt::Prompt::SetOAuthIssueAdvice(
    const IssueAdviceInfo& issue_advice) {
  oauth_issue_advice_ = issue_advice;
}

void ExtensionInstallPrompt::Prompt::SetInlineInstallWebstoreData(
    const std::string& localized_user_count,
    double average_rating,
    int rating_count) {
  CHECK_EQ(INLINE_INSTALL_PROMPT, type_);
  localized_user_count_ = localized_user_count;
  average_rating_ = average_rating;
  rating_count_ = rating_count;
}

string16 ExtensionInstallPrompt::Prompt::GetDialogTitle() const {
  int resource_id = kTitleIds[type_];

  if (type_ == INSTALL_PROMPT) {
    if (extension_->is_app())
      resource_id = IDS_EXTENSION_INSTALL_APP_PROMPT_TITLE;
    else if (extension_->is_theme())
      resource_id = IDS_EXTENSION_INSTALL_THEME_PROMPT_TITLE;
    else
      resource_id = IDS_EXTENSION_INSTALL_EXTENSION_PROMPT_TITLE;
  }

  return l10n_util::GetStringUTF16(resource_id);
}

string16 ExtensionInstallPrompt::Prompt::GetHeading() const {
  if (type_ == INLINE_INSTALL_PROMPT) {
    return UTF8ToUTF16(extension_->name());
  } else if (type_ == BUNDLE_INSTALL_PROMPT) {
    return bundle_->GetHeadingTextFor(BundleInstaller::Item::STATE_PENDING);
  } else {
    return l10n_util::GetStringFUTF16(
        kHeadingIds[type_], UTF8ToUTF16(extension_->name()));
  }
}

string16 ExtensionInstallPrompt::Prompt::GetAcceptButtonLabel() const {
  return l10n_util::GetStringUTF16(kAcceptButtonIds[type_]);
}

bool ExtensionInstallPrompt::Prompt::HasAbortButtonLabel() const {
  return kAbortButtonIds[type_] > 0;
}

string16 ExtensionInstallPrompt::Prompt::GetAbortButtonLabel() const {
  CHECK(HasAbortButtonLabel());
  return l10n_util::GetStringUTF16(kAbortButtonIds[type_]);
}

string16 ExtensionInstallPrompt::Prompt::GetPermissionsHeading() const {
  return l10n_util::GetStringUTF16(kPermissionsHeaderIds[type_]);
}

string16 ExtensionInstallPrompt::Prompt::GetOAuthHeading() const {
  string16 username(ASCIIToUTF16("username@example.com"));
  // |profile_| can be NULL in unit tests.
  if (profile_) {
    username = UTF8ToUTF16(profile_->GetPrefs()->GetString(
        prefs::kGoogleServicesUsername));
  }
  int resource_id = kOAuthHeaderIds[type_];
  return l10n_util::GetStringFUTF16(resource_id, username);
}

void ExtensionInstallPrompt::Prompt::AppendRatingStars(
    StarAppender appender, void* data) const {
  CHECK(appender);
  CHECK_EQ(INLINE_INSTALL_PROMPT, type_);
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

string16 ExtensionInstallPrompt::Prompt::GetRatingCount() const {
  CHECK_EQ(INLINE_INSTALL_PROMPT, type_);
  return l10n_util::GetStringFUTF16(
        IDS_EXTENSION_RATING_COUNT,
        UTF8ToUTF16(base::IntToString(rating_count_)));
}

string16 ExtensionInstallPrompt::Prompt::GetUserCount() const {
  CHECK_EQ(INLINE_INSTALL_PROMPT, type_);
  return l10n_util::GetStringFUTF16(
      IDS_EXTENSION_USER_COUNT,
      UTF8ToUTF16(localized_user_count_));
}

size_t ExtensionInstallPrompt::Prompt::GetPermissionCount() const {
  return permissions_.size();
}

string16 ExtensionInstallPrompt::Prompt::GetPermission(size_t index) const {
  CHECK_LT(index, permissions_.size());
  return permissions_[index];
}

size_t ExtensionInstallPrompt::Prompt::GetOAuthIssueCount() const {
  return oauth_issue_advice_.size();
}

const IssueAdviceInfoEntry& ExtensionInstallPrompt::Prompt::GetOAuthIssue(
    size_t index) const {
  CHECK_LT(index, oauth_issue_advice_.size());
  return oauth_issue_advice_[index];
}

// static
scoped_refptr<Extension>
    ExtensionInstallPrompt::GetLocalizedExtensionForDisplay(
    const DictionaryValue* manifest,
    int flags,
    const std::string& id,
    const std::string& localized_name,
    const std::string& localized_description,
    std::string* error) {
  scoped_ptr<DictionaryValue> localized_manifest;
  if (!localized_name.empty() || !localized_description.empty()) {
    localized_manifest.reset(manifest->DeepCopy());
    if (!localized_name.empty()) {
      localized_manifest->SetString(extension_manifest_keys::kName,
                                    localized_name);
    }
    if (!localized_description.empty()) {
      localized_manifest->SetString(extension_manifest_keys::kDescription,
                                    localized_description);
    }
  }

  return Extension::Create(
      FilePath(),
      Extension::INTERNAL,
      localized_manifest.get() ? *localized_manifest.get() : *manifest,
      flags,
      id,
      error);
}

ExtensionInstallPrompt::ExtensionInstallPrompt(
    gfx::NativeWindow parent,
    content::PageNavigator* navigator,
    Profile* profile)
    : record_oauth2_grant_(false),
      parent_(parent),
      navigator_(navigator),
      ui_loop_(MessageLoop::current()),
      extension_(NULL),
      install_ui_(ExtensionInstallUI::Create(profile)),
      delegate_(NULL),
      prompt_(profile, UNSET_PROMPT_TYPE),
      prompt_type_(UNSET_PROMPT_TYPE),
      ALLOW_THIS_IN_INITIALIZER_LIST(tracker_(this)) {
}

ExtensionInstallPrompt::~ExtensionInstallPrompt() {
}

void ExtensionInstallPrompt::ConfirmBundleInstall(
    extensions::BundleInstaller* bundle,
    const PermissionSet* permissions) {
  DCHECK(ui_loop_ == MessageLoop::current());
  bundle_ = bundle;
  permissions_ = permissions;
  delegate_ = bundle;
  prompt_type_ = BUNDLE_INSTALL_PROMPT;

  FetchOAuthIssueAdviceIfNeeded();
}

void ExtensionInstallPrompt::ConfirmInlineInstall(
    Delegate* delegate,
    const Extension* extension,
    SkBitmap* icon,
    const ExtensionInstallPrompt::Prompt& prompt) {
  DCHECK(ui_loop_ == MessageLoop::current());
  extension_ = extension;
  permissions_ = extension->GetActivePermissions();
  delegate_ = delegate;
  prompt_ = prompt;
  prompt_type_ = INLINE_INSTALL_PROMPT;

  SetIcon(icon);
  FetchOAuthIssueAdviceIfNeeded();
}

void ExtensionInstallPrompt::ConfirmWebstoreInstall(Delegate* delegate,
                                                    const Extension* extension,
                                                    const SkBitmap* icon) {
  // SetIcon requires |extension_| to be set. ConfirmInstall will setup the
  // remaining fields.
  extension_ = extension;
  SetIcon(icon);
  ConfirmInstall(delegate, extension);
}

void ExtensionInstallPrompt::ConfirmInstall(Delegate* delegate,
                                            const Extension* extension) {
  DCHECK(ui_loop_ == MessageLoop::current());
  extension_ = extension;
  permissions_ = extension->GetActivePermissions();
  delegate_ = delegate;
  prompt_type_ = INSTALL_PROMPT;

  // We special-case themes to not show any confirm UI. Instead they are
  // immediately installed, and then we show an infobar (see OnInstallSuccess)
  // to allow the user to revert if they don't like it.
  //
  // We don't do this in the case where off-store extension installs are
  // disabled because in that case, we don't show the dangerous download UI, so
  // we need the UI confirmation.
  if (extension->is_theme()) {
    if (extension->from_webstore() ||
        extensions::switch_utils::IsEasyOffStoreInstallEnabled()) {
      delegate->InstallUIProceed();
      return;
    }
  }

  LoadImageIfNeeded();
}

void ExtensionInstallPrompt::ConfirmReEnable(Delegate* delegate,
                                             const Extension* extension) {
  DCHECK(ui_loop_ == MessageLoop::current());
  extension_ = extension;
  permissions_ = extension->GetActivePermissions();
  delegate_ = delegate;
  prompt_type_ = RE_ENABLE_PROMPT;

  LoadImageIfNeeded();
}

void ExtensionInstallPrompt::ConfirmPermissions(
    Delegate* delegate,
    const Extension* extension,
    const PermissionSet* permissions) {
  DCHECK(ui_loop_ == MessageLoop::current());
  extension_ = extension;
  permissions_ = permissions;
  delegate_ = delegate;
  prompt_type_ = PERMISSIONS_PROMPT;

  LoadImageIfNeeded();
}

void ExtensionInstallPrompt::ConfirmIssueAdvice(
    Delegate* delegate,
    const Extension* extension,
    const IssueAdviceInfo& issue_advice) {
  DCHECK(ui_loop_ == MessageLoop::current());
  extension_ = extension;
  delegate_ = delegate;
  prompt_type_ = PERMISSIONS_PROMPT;

  record_oauth2_grant_ = true;
  prompt_.SetOAuthIssueAdvice(issue_advice);

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

void ExtensionInstallPrompt::OnImageLoaded(const gfx::Image& image,
                                           const std::string& extension_id,
                                           int index) {
  SetIcon(image.IsEmpty() ? NULL : image.ToSkBitmap());
  FetchOAuthIssueAdviceIfNeeded();
}

void ExtensionInstallPrompt::LoadImageIfNeeded() {
  // Bundle install prompts do not have an icon.
  if (!icon_.empty()) {
    FetchOAuthIssueAdviceIfNeeded();
    return;
  }

  // Load the image asynchronously. For the response, check OnImageLoaded.
  ExtensionResource image =
      extension_->GetIconResource(extension_misc::EXTENSION_ICON_LARGE,
                                  ExtensionIconSet::MATCH_BIGGER);
  // Load the icon whose pixel size is large enough to be displayed under
  // maximal supported scale factor. UI code will scale the icon down if needed.
  // TODO(tbarzic): We should use IconImage here and load the required bitmap
  //     lazily.
  int pixel_size = GetSizeForMaxScaleFactor(kIconSize);
  tracker_.LoadImage(extension_, image,
                     gfx::Size(pixel_size, pixel_size),
                     ImageLoadingTracker::DONT_CACHE);
}

void ExtensionInstallPrompt::FetchOAuthIssueAdviceIfNeeded() {
  // |extension_| may be NULL, e.g. in the bundle install case.
  if (!extension_ ||
      prompt_type_ == BUNDLE_INSTALL_PROMPT ||
      prompt_type_ == INLINE_INSTALL_PROMPT ||
      prompt_.GetOAuthIssueCount() != 0U) {
    ShowConfirmation();
    return;
  }

  const Extension::OAuth2Info& oauth2_info = extension_->oauth2_info();
  if (oauth2_info.client_id.empty() ||
      oauth2_info.scopes.empty()) {
    ShowConfirmation();
    return;
  }

  Profile* profile = install_ui_->profile();
  TokenService* token_service = TokenServiceFactory::GetForProfile(profile);

  token_flow_.reset(new OAuth2MintTokenFlow(
      profile->GetRequestContext(),
      this,
      OAuth2MintTokenFlow::Parameters(
          token_service->GetOAuth2LoginRefreshToken(),
          extension_->id(),
          oauth2_info.client_id,
          oauth2_info.scopes,
          OAuth2MintTokenFlow::MODE_ISSUE_ADVICE)));
  token_flow_->Start();
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
  prompt_.set_type(prompt_type_);

  if (permissions_) {
    Extension::Type extension_type = prompt_type_ == BUNDLE_INSTALL_PROMPT ?
        Extension::TYPE_UNKNOWN : extension_->GetType();
    prompt_.SetPermissions(permissions_->GetWarningMessages(extension_type));
  }

  switch (prompt_type_) {
    case PERMISSIONS_PROMPT:
    case RE_ENABLE_PROMPT:
    case INLINE_INSTALL_PROMPT:
    case INSTALL_PROMPT: {
      prompt_.set_extension(extension_);
      prompt_.set_icon(gfx::Image(icon_));
      ShowExtensionInstallDialog(parent_, navigator_, delegate_, prompt_);
      break;
    }
    case BUNDLE_INSTALL_PROMPT: {
      prompt_.set_bundle(bundle_);
      ShowExtensionInstallDialog(parent_, navigator_, delegate_, prompt_);
      break;
    }
    default:
      NOTREACHED() << "Unknown message";
      break;
  }
}

namespace chrome {

ExtensionInstallPrompt* CreateExtensionInstallPromptWithBrowser(
    Browser* browser) {
  // |browser| can be NULL in unit tests.
  if (!browser)
    return new ExtensionInstallPrompt(NULL, NULL, NULL);
  gfx::NativeWindow parent =
      browser->window() ? browser->window()->GetNativeWindow() : NULL;
  return new ExtensionInstallPrompt(parent, browser, browser->profile());
}

}  // namespace chrome
