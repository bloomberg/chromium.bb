// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_install_prompt.h"

#include <map>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/bundle_installer.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/browser/extensions/image_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/icons/icons_handler.h"
#include "chrome/common/extensions/api/identity/oauth2_manifest_handler.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/feature_switch.h"
#include "chrome/common/extensions/manifest.h"
#include "chrome/common/extensions/permissions/permission_set.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "extensions/common/extension_resource.h"
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
};
static const int kHeadingIds[ExtensionInstallPrompt::NUM_PROMPT_TYPES] = {
  IDS_EXTENSION_INSTALL_PROMPT_HEADING,
  0,  // Inline installs use the extension name.
  0,  // Heading for bundle installs depends on the bundle contents.
  IDS_EXTENSION_RE_ENABLE_PROMPT_HEADING,
  IDS_EXTENSION_PERMISSIONS_PROMPT_HEADING,
  0,  // External installs use different strings for extensions/apps.
  IDS_EXTENSION_POST_INSTALL_PERMISSIONS_PROMPT_HEADING,
};
static const int kButtons[ExtensionInstallPrompt::NUM_PROMPT_TYPES] = {
  ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL,
  ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL,
  ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL,
  ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL,
  ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL,
  ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL,
  ui::DIALOG_BUTTON_CANCEL,
};
static const int kAcceptButtonIds[ExtensionInstallPrompt::NUM_PROMPT_TYPES] = {
  IDS_EXTENSION_PROMPT_INSTALL_BUTTON,
  IDS_EXTENSION_PROMPT_INSTALL_BUTTON,
  IDS_EXTENSION_PROMPT_INSTALL_BUTTON,
  IDS_EXTENSION_PROMPT_RE_ENABLE_BUTTON,
  IDS_EXTENSION_PROMPT_PERMISSIONS_BUTTON,
  0,  // External installs use different strings for extensions/apps.
  0,
};
static const int kAbortButtonIds[ExtensionInstallPrompt::NUM_PROMPT_TYPES] = {
  0,  // These all use the platform's default cancel label.
  0,
  0,
  0,
  IDS_EXTENSION_PROMPT_PERMISSIONS_ABORT_BUTTON,
  IDS_EXTENSION_EXTERNAL_INSTALL_PROMPT_ABORT_BUTTON,
  IDS_CLOSE,
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
};
static const int kOAuthHeaderIds[ExtensionInstallPrompt::NUM_PROMPT_TYPES] = {
  IDS_EXTENSION_PROMPT_OAUTH_HEADER,
  0,  // Inline installs don't show OAuth permissions.
  0,  // Bundle installs don't show OAuth permissions.
  IDS_EXTENSION_PROMPT_OAUTH_REENABLE_HEADER,
  IDS_EXTENSION_PROMPT_OAUTH_PERMISSIONS_HEADER,
  // TODO(mpcomplete): Do we need this for external install UI? If we do,
  // we need to update FetchOAuthIssueAdviceIfNeeded.
  0,
  0,
};

// Size of extension icon in top left of dialog.
const int kIconSize = 69;

// Returns pixel size under maximal scale factor for the icon whose device
// independent size is |size_in_dip|
int GetSizeForMaxScaleFactor(int size_in_dip) {
  float max_scale_factor_scale =
      ui::GetScaleFactorScale(ui::GetMaxScaleFactor());
  return static_cast<int>(size_in_dip * max_scale_factor_scale);
}

// Returns bitmap for the default icon with size equal to the default icon's
// pixel size under maximal supported scale factor.
SkBitmap GetDefaultIconBitmapForMaxScaleFactor(bool is_app) {
  const gfx::ImageSkia& image = is_app ?
      extensions::IconsInfo::GetDefaultAppIcon() :
      extensions::IconsInfo::GetDefaultExtensionIcon();
  return image.GetRepresentation(ui::GetMaxScaleFactor()).sk_bitmap();
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
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&ExtensionInstallPrompt::Delegate::InstallUIProceed,
                   base::Unretained(delegate)));
    return true;
  }

  if (value == "cancel") {
    MessageLoop::current()->PostTask(
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
      extension_(NULL),
      bundle_(NULL),
      average_rating_(0.0),
      rating_count_(0) {
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

void ExtensionInstallPrompt::Prompt::SetUserNameFromProfile(Profile* profile) {
  // |profile| can be NULL in unit tests.
  if (profile) {
    oauth_user_name_ = UTF8ToUTF16(profile->GetPrefs()->GetString(
        prefs::kGoogleServicesUsername));
  } else {
    oauth_user_name_.clear();
  }
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
  } else if (type_ == EXTERNAL_INSTALL_PROMPT) {
    return l10n_util::GetStringFUTF16(
        resource_id, UTF8ToUTF16(extension_->name()));
  }

  return l10n_util::GetStringUTF16(resource_id);
}

string16 ExtensionInstallPrompt::Prompt::GetHeading() const {
  if (type_ == INLINE_INSTALL_PROMPT) {
    return UTF8ToUTF16(extension_->name());
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
        kHeadingIds[type_], UTF8ToUTF16(extension_->name()));
  }
}

int ExtensionInstallPrompt::Prompt::GetDialogButtons() const {
  return kButtons[type_];
}

bool ExtensionInstallPrompt::Prompt::HasAcceptButtonLabel() const {
  return kAcceptButtonIds[type_] > 0;
}

string16 ExtensionInstallPrompt::Prompt::GetAcceptButtonLabel() const {
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
  return l10n_util::GetStringFUTF16(kOAuthHeaderIds[type_], oauth_user_name_);
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
      base::FilePath(),
      Manifest::INTERNAL,
      localized_manifest.get() ? *localized_manifest.get() : *manifest,
      flags,
      id,
      error);
}

ExtensionInstallPrompt::ExtensionInstallPrompt(
    content::WebContents* contents)
    : record_oauth2_grant_(false),
      ui_loop_(MessageLoop::current()),
      extension_(NULL),
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
    : record_oauth2_grant_(false),
      ui_loop_(MessageLoop::current()),
      extension_(NULL),
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
  DCHECK(ui_loop_ == MessageLoop::current());
  bundle_ = bundle;
  permissions_ = permissions;
  delegate_ = bundle;
  prompt_.set_type(BUNDLE_INSTALL_PROMPT);

  FetchOAuthIssueAdviceIfNeeded();
}

void ExtensionInstallPrompt::ConfirmStandaloneInstall(
    Delegate* delegate,
    const Extension* extension,
    SkBitmap* icon,
    const ExtensionInstallPrompt::Prompt& prompt) {
  DCHECK(ui_loop_ == MessageLoop::current());
  extension_ = extension;
  permissions_ = extension->GetActivePermissions();
  delegate_ = delegate;
  prompt_ = prompt;

  SetIcon(icon);
  FetchOAuthIssueAdviceIfNeeded();
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
  DCHECK(ui_loop_ == MessageLoop::current());
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
  DCHECK(ui_loop_ == MessageLoop::current());
  extension_ = extension;
  permissions_ = extension->GetActivePermissions();
  delegate_ = delegate;
  prompt_.set_type(RE_ENABLE_PROMPT);

  LoadImageIfNeeded();
}

void ExtensionInstallPrompt::ConfirmExternalInstall(
    Delegate* delegate, const Extension* extension) {
  DCHECK(ui_loop_ == MessageLoop::current());
  extension_ = extension;
  permissions_ = extension->GetActivePermissions();
  delegate_ = delegate;
  prompt_.set_type(EXTERNAL_INSTALL_PROMPT);

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
  prompt_.set_type(PERMISSIONS_PROMPT);

  LoadImageIfNeeded();
}

void ExtensionInstallPrompt::ConfirmIssueAdvice(
    Delegate* delegate,
    const Extension* extension,
    const IssueAdviceInfo& issue_advice) {
  DCHECK(ui_loop_ == MessageLoop::current());
  extension_ = extension;
  delegate_ = delegate;
  prompt_.set_type(PERMISSIONS_PROMPT);

  record_oauth2_grant_ = true;
  prompt_.SetOAuthIssueAdvice(issue_advice);

  LoadImageIfNeeded();
}

void ExtensionInstallPrompt::ReviewPermissions(Delegate* delegate,
                                               const Extension* extension) {
  DCHECK(ui_loop_ == MessageLoop::current());
  extension_ = extension;
  permissions_ = extension->GetActivePermissions();
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
  FetchOAuthIssueAdviceIfNeeded();
}

void ExtensionInstallPrompt::LoadImageIfNeeded() {
  // Bundle install prompts do not have an icon.
  // Also |install_ui_.profile()| can be NULL in unit tests.
  if (!icon_.empty() || !install_ui_->profile()) {
    FetchOAuthIssueAdviceIfNeeded();
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

void ExtensionInstallPrompt::FetchOAuthIssueAdviceIfNeeded() {
  // |extension_| may be NULL, e.g. in the bundle install case.
  if (!extension_ ||
      prompt_.type() == BUNDLE_INSTALL_PROMPT ||
      prompt_.type() == INLINE_INSTALL_PROMPT ||
      prompt_.type() == EXTERNAL_INSTALL_PROMPT ||
      prompt_.GetOAuthIssueCount() != 0U) {
    ShowConfirmation();
    return;
  }

  const extensions::OAuth2Info& oauth2_info =
      extensions::OAuth2Info::GetOAuth2Info(extension_);
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
  if (permissions_ &&
      (!extension_ || !extension_->ShouldSkipPermissionWarnings())) {
    Manifest::Type extension_type = extension_ ?
        extension_->GetType() : Manifest::TYPE_UNKNOWN;
    prompt_.SetPermissions(permissions_->GetWarningMessages(extension_type));
  }

  switch (prompt_.type()) {
    case PERMISSIONS_PROMPT:
    case RE_ENABLE_PROMPT:
    case INLINE_INSTALL_PROMPT:
    case EXTERNAL_INSTALL_PROMPT:
    case INSTALL_PROMPT:
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
