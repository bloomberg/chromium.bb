// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/change_picture_options_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/camera_detector.h"
#include "chrome/browser/chromeos/login/default_user_images.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/options/take_photo_dialog.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/window.h"
#include "chrome/browser/ui/webui/web_ui_util.cc"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "content/common/notification_service.h"
#include "content/public/common/url_constants.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "views/widget/widget.h"

namespace chromeos {

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

}  // namespace

ChangePictureOptionsHandler::ChangePictureOptionsHandler()
    : previous_image_data_url_(chrome::kAboutBlankURL),
      profile_image_data_url_(chrome::kAboutBlankURL),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_IMAGE_UPDATED,
      NotificationService::AllSources());
}

ChangePictureOptionsHandler::~ChangePictureOptionsHandler() {
  if (select_file_dialog_.get())
    select_file_dialog_->ListenerDestroyed();
}

void ChangePictureOptionsHandler::Initialize() {
  const UserManager::User& user = UserManager::Get()->logged_in_user();

  // If no camera presence check has been performed in this session,
  // start one now.
  if (CameraDetector::camera_presence() ==
      CameraDetector::kCameraPresenceUnknown)
    CheckCameraPresence();

  // While the check is in progress, use previous camera presence state and
  // presume it is present if no check has been performed yet.
  SetCameraPresent(CameraDetector::camera_presence() !=
                   CameraDetector::kCameraAbsent);

  // Save previous profile image as data URL, if any.
  if (user.default_image_index() == UserManager::User::kProfileImageIndex)
    profile_image_data_url_ = web_ui_util::GetImageDataUrl(user.image());

  SendAvailableImages();
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
}

void ChangePictureOptionsHandler::RegisterMessages() {
  DCHECK(web_ui_);
  web_ui_->RegisterMessageCallback("chooseFile",
      base::Bind(&ChangePictureOptionsHandler::HandleChooseFile,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("takePhoto",
      base::Bind(&ChangePictureOptionsHandler::HandleTakePhoto,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("getSelectedImage",
      base::Bind(&ChangePictureOptionsHandler::HandleGetSelectedImage,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("selectImage",
      base::Bind(&ChangePictureOptionsHandler::HandleSelectImage,
                 base::Unretained(this)));
}

void ChangePictureOptionsHandler::SendAvailableImages() {
  ListValue image_urls;
  for (int i = 0; i < kDefaultImagesCount; ++i) {
    image_urls.Append(new StringValue(GetDefaultImageUrl(i)));
  }
  web_ui_->CallJavascriptFunction("ChangePictureOptions.setUserImages",
                                  image_urls);
}

void ChangePictureOptionsHandler::DownloadProfileImage() {
  if (!profile_image_downloader_.get())
    profile_image_downloader_.reset(new ProfileImageDownloader(this));
  profile_image_downloader_->Start();
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
  static SelectFileDialog::FileTypeInfo file_type_info =
      GetUserImageFileTypeInfo();

  select_file_dialog_->SelectFile(
      SelectFileDialog::SELECT_OPEN_FILE,
      l10n_util::GetStringUTF16(IDS_DOWNLOAD_TITLE),
      downloads_path,
      &file_type_info,
      0,
      FILE_PATH_LITERAL(""),
      web_ui_->tab_contents(),
      GetBrowserWindow(),
      NULL);
}

void ChangePictureOptionsHandler::HandleTakePhoto(const ListValue* args) {
  DCHECK(args && args->empty());
  views::Widget* window = browser::CreateViewsWindow(
      GetBrowserWindow(),
      new TakePhotoDialog(this));
  window->SetAlwaysOnTop(true);
  window->Show();
}

void ChangePictureOptionsHandler::HandleGetSelectedImage(
    const base::ListValue* args) {
  // TODO(ivankr): rename this function as it has become much bigger
  // than just querying the currently selected image.
  DCHECK(args && args->empty());
  CheckCameraPresence();
  SendSelectedImage();
  DownloadProfileImage();
}

void ChangePictureOptionsHandler::SendSelectedImage() {
  const UserManager::User& user = UserManager::Get()->logged_in_user();
  DCHECK(!user.email().empty());

  int image_index = user.default_image_index();
  if (image_index == UserManager::User::kInvalidImageIndex) {
    // This can happen in some test paths.
    image_index = 0;
  }

  if (image_index == UserManager::User::kExternalImageIndex) {
    // User has image from camera/file, copy it and add to the list of images.
    RecordPreviousImage(image_index, user.image());
    web_ui_->CallJavascriptFunction("ChangePictureOptions.setOldImage");
  } else if (image_index == UserManager::User::kProfileImageIndex) {
    // User has his/her Profile image as current image.
    RecordPreviousImage(image_index, user.image());
    base::StringValue image_data_url(previous_image_data_url_);
    base::FundamentalValue select(true);
    web_ui_->CallJavascriptFunction("ChangePictureOptions.setProfileImage",
                                    image_data_url, select);
  } else if (image_index >= 0 && image_index < kDefaultImagesCount) {
    // User has image from the set of default images.
    base::StringValue image_url(GetDefaultImageUrl(image_index));
    web_ui_->CallJavascriptFunction("ChangePictureOptions.setSelectedImage",
                                    image_url);
  } else {
    NOTREACHED();
  }
}

void ChangePictureOptionsHandler::HandleSelectImage(const ListValue* args) {
  std::string image_url;
  if (!args ||
      args->GetSize() != 1 ||
      !args->GetString(0, &image_url)) {
    NOTREACHED();
    return;
  }
  if (image_url.empty())
    return;

  UserManager* user_manager = UserManager::Get();
  const UserManager::User& user = user_manager->logged_in_user();
  int user_image_index = previous_image_index_;

  if (StartsWithASCII(image_url, chrome::kChromeUIUserImageURL, false) ||
      image_url == previous_image_data_url_) {
    // Image from file/camera uses kChromeUIUserImageURL as URL while
    // current profile image always has a full data URL.
    // This way transition from (current profile image) to
    // (profile image, current image from file) is easier.
    DCHECK(!previous_image_.empty());

    user_manager->SetLoggedInUserImage(previous_image_, user_image_index);
    user_manager->SaveUserImage(
        user.email(), previous_image_, user_image_index);
    UMA_HISTOGRAM_ENUMERATION("UserImage.ChangeChoice",
                              kHistogramImageOld,
                              kHistogramImagesCount);

    VLOG(1) << "Selected old user image";
  } else if (image_url == profile_image_data_url_) {
    DCHECK(!profile_image_.empty());

    user_manager->SetLoggedInUserImage(profile_image_,
                                       UserManager::User::kProfileImageIndex);
    user_manager->SaveUserImage(
        user.email(), profile_image_, UserManager::User::kProfileImageIndex);

    UMA_HISTOGRAM_ENUMERATION("UserImage.ChangeChoice",
                              kHistogramImageFromProfile,
                              kHistogramImagesCount);

    VLOG(1) << "Selected profile image";
  } else if (IsDefaultImageUrl(image_url, &user_image_index)) {
    const SkBitmap* image = ResourceBundle::GetSharedInstance().GetBitmapNamed(
        kDefaultImageResources[user_image_index]);

    user_manager->SetLoggedInUserImage(*image, user_image_index);
    user_manager->SaveUserImagePath(user.email(),
                                    GetDefaultImagePath(user_image_index),
                                    user_image_index);
    UMA_HISTOGRAM_ENUMERATION("UserImage.ChangeChoice",
                              user_image_index,
                              kHistogramImagesCount);

    VLOG(1) << "Selected default user image: " << user_image_index;
  }
}

void ChangePictureOptionsHandler::FileSelected(const FilePath& path,
                                               int index,
                                               void* params) {
  UserManager::Get()->LoadLoggedInUserImage(path);
  UMA_HISTOGRAM_ENUMERATION("UserImage.ChangeChoice",
                            kHistogramImageFromFile,
                            kHistogramImagesCount);
}

void ChangePictureOptionsHandler::OnPhotoAccepted(const SkBitmap& photo) {
  UserManager* user_manager = UserManager::Get();
  DCHECK(user_manager);

  const UserManager::User& user = user_manager->logged_in_user();
  DCHECK(!user.email().empty());

  user_manager->SetLoggedInUserImage(photo,
                                     UserManager::User::kExternalImageIndex);
  user_manager->SaveUserImage(
      user.email(), photo, UserManager::User::kExternalImageIndex);
  UMA_HISTOGRAM_ENUMERATION("UserImage.ChangeChoice",
                            kHistogramImageFromCamera,
                            kHistogramImagesCount);
}

void ChangePictureOptionsHandler::OnDownloadSuccess(const SkBitmap& image) {
  // Profile image has been downloaded.
  // TODO(ivankr): check if profile image is default image.

  std::string profile_image_data_url = web_ui_util::GetImageDataUrl(image);
  if (profile_image_data_url != profile_image_data_url_) {
    // Differs from the current profile image.
    profile_image_ = image;
    profile_image_data_url_ = profile_image_data_url;

    base::StringValue image_data_url(profile_image_data_url);
    base::FundamentalValue select(false);
    web_ui_->CallJavascriptFunction("ChangePictureOptions.setProfileImage",
                                    image_data_url, select);
  }
}

void ChangePictureOptionsHandler::CheckCameraPresence() {
  CameraDetector::StartPresenceCheck(
      base::Bind(&ChangePictureOptionsHandler::OnCameraPresenceCheckDone,
                 weak_factory_.GetWeakPtr()));
}

void ChangePictureOptionsHandler::SetCameraPresent(bool present) {
  base::FundamentalValue present_value(present);
  web_ui_->CallJavascriptFunction("ChangePictureOptions.setCameraPresent",
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
  // User profile image is currently set and it has been updated by UserManager.
  if (type == chrome::NOTIFICATION_PROFILE_IMAGE_UPDATED)
    SendSelectedImage();
}

void ChangePictureOptionsHandler::RecordPreviousImage(int image_index,
                                                      const SkBitmap& image) {
  previous_image_index_ = image_index;
  previous_image_ = image;
  previous_image_data_url_ = web_ui_util::GetImageDataUrl(image);
  if (image_index == UserManager::User::kProfileImageIndex) {
    profile_image_ = previous_image_;
    profile_image_data_url_ = previous_image_data_url_;
  }
}

gfx::NativeWindow ChangePictureOptionsHandler::GetBrowserWindow() const {
  Browser* browser =
      BrowserList::FindBrowserWithProfile(Profile::FromWebUI(web_ui_));
  if (!browser)
    return NULL;
  return browser->window()->GetNativeHandle();
}

}  // namespace chromeos
