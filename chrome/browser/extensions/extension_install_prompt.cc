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
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/extension_resource.h"
#include "chrome/common/extensions/extension_switch_utils.h"
#include "chrome/common/extensions/url_pattern.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

#if defined(USE_ASH)
#include "ash/shell.h"
#endif

using extensions::BundleInstaller;
using extensions::Extension;

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

namespace {

// Size of extension icon in top left of dialog.
const int kIconSize = 69;

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
  return l10n_util::GetStringFUTF16(
      IDS_EXTENSION_PERMISSION_LINE, permissions_[index]);
}

// static
scoped_refptr<Extension>
    ExtensionInstallPrompt::GetLocalizedExtensionForDisplay(
    const DictionaryValue* manifest,
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
      Extension::NO_FLAGS,
      id,
      error);
}

ExtensionInstallPrompt::ExtensionInstallPrompt(Profile* profile)
    : profile_(profile),
      ui_loop_(MessageLoop::current()),
      extension_(NULL),
      install_ui_(ExtensionInstallUI::Create(profile)),
      delegate_(NULL),
      prompt_(UNSET_PROMPT_TYPE),
      prompt_type_(UNSET_PROMPT_TYPE),
      ALLOW_THIS_IN_INITIALIZER_LIST(tracker_(this)) {
}

ExtensionInstallPrompt::~ExtensionInstallPrompt() {
}

void ExtensionInstallPrompt::ConfirmBundleInstall(
    extensions::BundleInstaller* bundle,
    const ExtensionPermissionSet* permissions) {
  DCHECK(ui_loop_ == MessageLoop::current());
  bundle_ = bundle;
  permissions_ = permissions;
  delegate_ = bundle;
  prompt_type_ = BUNDLE_INSTALL_PROMPT;

  ShowConfirmation();
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
  ShowConfirmation();
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
    const ExtensionPermissionSet* permissions) {
  DCHECK(ui_loop_ == MessageLoop::current());
  extension_ = extension;
  permissions_ = permissions;
  delegate_ = delegate;
  prompt_type_ = PERMISSIONS_PROMPT;

  LoadImageIfNeeded();
}

void ExtensionInstallPrompt::OnInstallSuccess(const Extension* extension,
                                              SkBitmap* icon) {
  extension_ = extension;
  SetIcon(icon);

  install_ui_->OnInstallSuccess(extension, &icon_);
}

void ExtensionInstallPrompt::OnInstallFailure(const string16& error) {
  install_ui_->OnInstallFailure(error);
}

void ExtensionInstallPrompt::SetIcon(const SkBitmap* image) {
  if (image)
    icon_ = *image;
  else
    icon_ = SkBitmap();
  if (icon_.empty())
    icon_ = Extension::GetDefaultIcon(extension_->is_app());
}

void ExtensionInstallPrompt::OnImageLoaded(const gfx::Image& image,
                                           const std::string& extension_id,
                                           int index) {
  SetIcon(image.IsEmpty() ? NULL : image.ToSkBitmap());
  ShowConfirmation();
}

void ExtensionInstallPrompt::LoadImageIfNeeded() {
  // Bundle install prompts do not have an icon.
  if (!icon_.empty()) {
    ShowConfirmation();
    return;
  }

  // Load the image asynchronously. For the response, check OnImageLoaded.
  ExtensionResource image =
      extension_->GetIconResource(ExtensionIconSet::EXTENSION_ICON_LARGE,
                                  ExtensionIconSet::MATCH_BIGGER);
  tracker_.LoadImage(extension_, image,
                     gfx::Size(kIconSize, kIconSize),
                     ImageLoadingTracker::DONT_CACHE);
}

void ExtensionInstallPrompt::ShowConfirmation() {
  prompt_.set_type(prompt_type_);
  prompt_.SetPermissions(permissions_->GetWarningMessages());

  switch (prompt_type_) {
    case PERMISSIONS_PROMPT:
    case RE_ENABLE_PROMPT:
    case INLINE_INSTALL_PROMPT:
    case INSTALL_PROMPT: {
      prompt_.set_extension(extension_);
      prompt_.set_icon(gfx::Image(icon_));
      ShowExtensionInstallDialog(profile_, delegate_, prompt_);
      break;
    }
    case BUNDLE_INSTALL_PROMPT: {
      prompt_.set_bundle(bundle_);
      ShowExtensionInstallDialog(profile_, delegate_, prompt_);
      break;
    }
    default:
      NOTREACHED() << "Unknown message";
      break;
  }
}
