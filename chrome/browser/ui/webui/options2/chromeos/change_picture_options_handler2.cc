// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options2/chromeos/change_picture_options_handler2.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/camera_detector.h"
#include "chrome/browser/chromeos/login/default_user_images.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/options/take_photo_dialog.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/web_ui_util.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/url_constants.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/widget/widget.h"

namespace chromeos {
namespace options2 {

namespace {

// Returns info about extensions for files we support as user images.
SelectFileDialog::FileTypeInfo GetUserImageFileTypeInfo() {
  SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions.resize(5);

  file_type_info.extensions[0].push_back(FILE_PATH_LITERAL("bmp"));

  file_type_info.extensions[1].push_back(FILE_PATH_LITERAL("gif"));

  file_type_info.extensions[2].push_back(FILE_PATH_LITERAL("jpg"));
  file_type_info.extensions[2].push_back(FILE_PATH_LITERAL("jpeg"));

  file_type_info.extensions[3].push_back(FILE_PATH_LITERAL("png"));

  file_type_info.extensions[4].push_back(FILE_PATH_LITERAL("tif"));
  file_type_info.extensions[4].push_back(FILE_PATH_LITERAL("tiff"));

  return file_type_info;
}

// Time histogram suffix for profile image download.
const char kProfileDownloadReason[] = "Preferences";

}  // namespace

ChangePictureOptionsHandler::ChangePictureOptionsHandler()
    : previous_image_data_url_(chrome::kAboutBlankURL),
      previous_image_index_(User::kInvalidImageIndex),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_IMAGE_UPDATED,
      content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_IMAGE_UPDATE_FAILED,
      content::NotificationService::AllSources());
}

ChangePictureOptionsHandler::~ChangePictureOptionsHandler() {
  if (select_file_dialog_.get())
    select_file_dialog_->ListenerDestroyed();
}

void ChangePictureOptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  localized_strings->SetString("changePicturePage",
      l10n_util::GetStringUTF16(IDS_OPTIONS_CHANGE_PICTURE_DIALOG_TITLE));
  localized_strings->SetString("changePicturePageDescription",
      l10n_util::GetStringUTF16(IDS_OPTIONS_CHANGE_PICTURE_DIALOG_TEXT));
  localized_strings->SetString("takePhoto",
      l10n_util::GetStringUTF16(IDS_OPTIONS_CHANGE_PICTURE_TAKE_PHOTO));
  localized_strings->SetString("chooseFile",
      l10n_util::GetStringUTF16(IDS_OPTIONS_CHANGE_PICTURE_CHOOSE_FILE));
  localized_strings->SetString("profilePhoto",
      l10n_util::GetStringUTF16(IDS_OPTIONS_CHANGE_PICTURE_PROFILE_PHOTO));
  localized_strings->SetString("profilePhotoLoading",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_CHANGE_PICTURE_PROFILE_LOADING_PHOTO));

  localized_strings->SetBoolean("userIsEphemeral",
                                UserManager::Get()->IsCurrentUserEphemeral());
}

void ChangePictureOptionsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("chooseFile",
      base::Bind(&ChangePictureOptionsHandler::HandleChooseFile,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("takePhoto",
      base::Bind(&ChangePictureOptionsHandler::HandleTakePhoto,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("onChangePicturePageShown",
      base::Bind(&ChangePictureOptionsHandler::HandlePageShown,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("onChangePicturePageInitialized",
      base::Bind(&ChangePictureOptionsHandler::HandlePageInitialized,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("selectImage",
      base::Bind(&ChangePictureOptionsHandler::HandleSelectImage,
                 base::Unretained(this)));
}

void ChangePictureOptionsHandler::SendDefaultImages() {
  ListValue image_urls;
  for (int i = 0; i < kDefaultImagesCount; ++i) {
    image_urls.Append(new StringValue(GetDefaultImageUrl(i)));
  }
  web_ui()->CallJavascriptFunction("ChangePictureOptions.setDefaultImages",
                                   image_urls);
}

void ChangePictureOptionsHandler::HandleChooseFile(const ListValue* args) {
  DCHECK(args && args->empty());
  if (!select_file_dialog_.get())
    select_file_dialog_ = SelectFileDialog::Create(this);

  FilePath downloads_path;
  if (!PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS, &downloads_path)) {
    NOTREACHED();
    return;
  }

  // Static so we initialize it only once.
  CR_DEFINE_STATIC_LOCAL(SelectFileDialog::FileTypeInfo, file_type_info,
      (GetUserImageFileTypeInfo()));

  select_file_dialog_->SelectFile(
      SelectFileDialog::SELECT_OPEN_FILE,
      l10n_util::GetStringUTF16(IDS_DOWNLOAD_TITLE),
      downloads_path,
      &file_type_info,
      0,
      FILE_PATH_LITERAL(""),
      web_ui()->GetWebContents(),
      GetBrowserWindow(),
      NULL);
}

void ChangePictureOptionsHandler::HandleTakePhoto(const ListValue* args) {
  DCHECK(args && args->empty());
  views::Widget* window = views::Widget::CreateWindowWithParent(
      new TakePhotoDialog(this), GetBrowserWindow());
  window->SetAlwaysOnTop(true);
  window->Show();
}

void ChangePictureOptionsHandler::HandlePageInitialized(
    const base::ListValue* args) {
  DCHECK(args && args->empty());
  // If no camera presence check has been performed in this session,
  // start one now.
  if (CameraDetector::camera_presence() ==
      CameraDetector::kCameraPresenceUnknown) {
    CheckCameraPresence();
  }

  // While the check is in progress, use previous camera presence state and
  // presume it is present if no check has been performed yet.
  SetCameraPresent(CameraDetector::camera_presence() !=
                   CameraDetector::kCameraAbsent);

  SendDefaultImages();
}

void ChangePictureOptionsHandler::HandlePageShown(const base::ListValue* args) {
  DCHECK(args && args->empty());
  // TODO(ivankr): If user opens settings and goes to Change Picture page right
  // after the check started |HandlePageInitialized| has been completed,
  // |CheckCameraPresence| will be called twice, should be throttled.
  CheckCameraPresence();
  SendSelectedImage();
  UpdateProfileImage();
}

void ChangePictureOptionsHandler::SendSelectedImage() {
  const User& user = UserManager::Get()->GetLoggedInUser();
  DCHECK(!user.email().empty());

  previous_image_index_ = user.image_index();
  switch (previous_image_index_) {
    case User::kExternalImageIndex: {
      // User has image from camera/file, record it and add to the image list.
      previous_image_ = user.image();
      previous_image_data_url_ = web_ui_util::GetImageDataUrl(previous_image_);
      web_ui()->CallJavascriptFunction("ChangePictureOptions.setOldImage");
      break;
    }
    case User::kProfileImageIndex: {
      // User has his/her Profile image as the current image.
      SendProfileImage(user.image(), true);
      break;
    }
    default: {
      DCHECK(previous_image_index_ >= 0 &&
             previous_image_index_ < kDefaultImagesCount);
      // User has image from the set of default images.
      base::StringValue image_url(GetDefaultImageUrl(previous_image_index_));
      web_ui()->CallJavascriptFunction("ChangePictureOptions.setSelectedImage",
                                       image_url);
    }
  }
}

void ChangePictureOptionsHandler::SendProfileImage(const SkBitmap& image,
                                                   bool should_select) {
  base::StringValue data_url(web_ui_util::GetImageDataUrl(image));
  base::FundamentalValue select(should_select);
  web_ui()->CallJavascriptFunction("ChangePictureOptions.setProfileImage",
                                   data_url, select);
}

void ChangePictureOptionsHandler::UpdateProfileImage() {
  UserManager* user_manager = UserManager::Get();

  // If we have a downloaded profile image and haven't sent it in
  // |SendSelectedImage|, send it now (without selecting).
  if (previous_image_index_ != User::kProfileImageIndex &&
      !user_manager->DownloadedProfileImage().empty())
    SendProfileImage(user_manager->DownloadedProfileImage(), false);

  user_manager->DownloadProfileImage(kProfileDownloadReason);
}

void ChangePictureOptionsHandler::HandleSelectImage(const ListValue* args) {
  std::string image_url;
  if (!args ||
      args->GetSize() != 1 ||
      !args->GetString(0, &image_url)) {
    NOTREACHED();
    return;
  }
  DCHECK(!image_url.empty());

  UserManager* user_manager = UserManager::Get();
  const User& user = user_manager->GetLoggedInUser();
  int image_index = User::kInvalidImageIndex;

  if (StartsWithASCII(image_url, chrome::kChromeUIUserImageURL, false)) {
    // Image from file/camera uses kChromeUIUserImageURL as URL while
    // current profile image always has a full data URL.
    // This way transition from (current profile image) to
    // (profile image, current image from file) is easier.

    DCHECK(!previous_image_.empty());
    user_manager->SaveUserImage(user.email(), previous_image_);

    UMA_HISTOGRAM_ENUMERATION("UserImage.ChangeChoice",
                              kHistogramImageOld,
                              kHistogramImagesCount);
    VLOG(1) << "Selected old user image";
  } else if (IsDefaultImageUrl(image_url, &image_index)) {
    // One of the default user images.
    user_manager->SaveUserDefaultImageIndex(user.email(), image_index);

    UMA_HISTOGRAM_ENUMERATION("UserImage.ChangeChoice",
                              image_index,
                              kHistogramImagesCount);
    VLOG(1) << "Selected default user image: " << image_index;
  } else {
    // Profile image selected. Could be previous (old) user image.
    user_manager->SaveUserImageFromProfileImage(user.email());

    if (previous_image_index_ == User::kProfileImageIndex) {
      UMA_HISTOGRAM_ENUMERATION("UserImage.ChangeChoice",
                                kHistogramImageOld,
                                kHistogramImagesCount);
      VLOG(1) << "Selected old (profile) user image";
    } else {
      UMA_HISTOGRAM_ENUMERATION("UserImage.ChangeChoice",
                                kHistogramImageFromProfile,
                                kHistogramImagesCount);
      VLOG(1) << "Selected profile image";
    }
  }
}

void ChangePictureOptionsHandler::FileSelected(const FilePath& path,
                                               int index,
                                               void* params) {
  UserManager* user_manager = UserManager::Get();
  user_manager->SaveUserImageFromFile(user_manager->GetLoggedInUser().email(),
                                      path);
  UMA_HISTOGRAM_ENUMERATION("UserImage.ChangeChoice",
                            kHistogramImageFromFile,
                            kHistogramImagesCount);
}

void ChangePictureOptionsHandler::OnPhotoAccepted(const SkBitmap& photo) {
  UserManager* user_manager = UserManager::Get();
  user_manager->SaveUserImage(user_manager->GetLoggedInUser().email(), photo);
  UMA_HISTOGRAM_ENUMERATION("UserImage.ChangeChoice",
                            kHistogramImageFromCamera,
                            kHistogramImagesCount);
}

void ChangePictureOptionsHandler::CheckCameraPresence() {
  CameraDetector::StartPresenceCheck(
      base::Bind(&ChangePictureOptionsHandler::OnCameraPresenceCheckDone,
                 weak_factory_.GetWeakPtr()));
}

void ChangePictureOptionsHandler::SetCameraPresent(bool present) {
  base::FundamentalValue present_value(present);
  web_ui()->CallJavascriptFunction("ChangePictureOptions.setCameraPresent",
                                   present_value);
}

void ChangePictureOptionsHandler::OnCameraPresenceCheckDone() {
  SetCameraPresent(CameraDetector::camera_presence() ==
                   CameraDetector::kCameraPresent);
}

void ChangePictureOptionsHandler::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  OptionsPageUIHandler::Observe(type, source, details);
  if (type == chrome::NOTIFICATION_PROFILE_IMAGE_UPDATED) {
    // User profile image has been updated.
    SendProfileImage(*content::Details<const SkBitmap>(details).ptr(), false);
  }
}

gfx::NativeWindow ChangePictureOptionsHandler::GetBrowserWindow() const {
  Browser* browser =
      browser::FindBrowserWithProfile(Profile::FromWebUI(web_ui()));
  if (!browser)
    return NULL;
  return browser->window()->GetNativeHandle();
}

}  // namespace options2
}  // namespace chromeos
