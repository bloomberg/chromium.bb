// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/change_picture_options_handler.h"

#include "ash/audio/sounds.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/camera_presence_notifier.h"
#include "chrome/browser/chromeos/login/users/avatar/user_image_manager.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chromeos/audio/chromeos_sounds.h"
#include "components/user_manager/user_image/default_user_images.h"
#include "components/user_manager/user_image/user_image.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/url_constants.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "net/base/data_url.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace chromeos {
namespace options {

namespace {

// Returns info about extensions for files we support as user images.
ui::SelectFileDialog::FileTypeInfo GetUserImageFileTypeInfo() {
  ui::SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions.resize(1);

  file_type_info.extensions[0].push_back(FILE_PATH_LITERAL("bmp"));

  file_type_info.extensions[0].push_back(FILE_PATH_LITERAL("jpg"));
  file_type_info.extensions[0].push_back(FILE_PATH_LITERAL("jpeg"));

  file_type_info.extensions[0].push_back(FILE_PATH_LITERAL("png"));

  file_type_info.extensions[0].push_back(FILE_PATH_LITERAL("tif"));
  file_type_info.extensions[0].push_back(FILE_PATH_LITERAL("tiff"));

  file_type_info.extension_description_overrides.resize(1);
  file_type_info.extension_description_overrides[0] =
      l10n_util::GetStringUTF16(IDS_IMAGE_FILES);

  return file_type_info;
}

// Time histogram suffix for profile image download.
const char kProfileDownloadReason[] = "Preferences";

}  // namespace

ChangePictureOptionsHandler::ChangePictureOptionsHandler()
    : previous_image_url_(url::kAboutBlankURL),
      previous_image_index_(User::kInvalidImageIndex) {
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_IMAGE_UPDATED,
      content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_IMAGE_UPDATE_FAILED,
      content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_LOGIN_USER_IMAGE_CHANGED,
      content::NotificationService::AllSources());

  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  media::SoundsManager* manager = media::SoundsManager::Get();
  manager->Initialize(SOUND_OBJECT_DELETE,
                      bundle.GetRawDataResource(IDR_SOUND_OBJECT_DELETE_WAV));
  manager->Initialize(SOUND_CAMERA_SNAP,
                      bundle.GetRawDataResource(IDR_SOUND_CAMERA_SNAP_WAV));
}

ChangePictureOptionsHandler::~ChangePictureOptionsHandler() {
  CameraPresenceNotifier::GetInstance()->RemoveObserver(this);
  if (select_file_dialog_.get())
    select_file_dialog_->ListenerDestroyed();
  if (image_decoder_.get())
    image_decoder_->set_delegate(NULL);
}

void ChangePictureOptionsHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  localized_strings->SetString("changePicturePage",
      l10n_util::GetStringUTF16(IDS_OPTIONS_CHANGE_PICTURE_DIALOG_TITLE));
  localized_strings->SetString("changePicturePageDescription",
      l10n_util::GetStringUTF16(IDS_OPTIONS_CHANGE_PICTURE_DIALOG_TEXT));
  localized_strings->SetString("takePhoto",
      l10n_util::GetStringUTF16(IDS_OPTIONS_CHANGE_PICTURE_TAKE_PHOTO));
  localized_strings->SetString("discardPhoto",
      l10n_util::GetStringUTF16(IDS_OPTIONS_CHANGE_PICTURE_DISCARD_PHOTO));
  localized_strings->SetString("flipPhoto",
      l10n_util::GetStringUTF16(IDS_OPTIONS_CHANGE_PICTURE_FLIP_PHOTO));
  localized_strings->SetString("chooseFile",
      l10n_util::GetStringUTF16(IDS_OPTIONS_CHANGE_PICTURE_CHOOSE_FILE));
  localized_strings->SetString("profilePhoto",
      l10n_util::GetStringUTF16(IDS_OPTIONS_CHANGE_PICTURE_PROFILE_PHOTO));
  localized_strings->SetString("profilePhotoLoading",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_CHANGE_PICTURE_PROFILE_LOADING_PHOTO));
  localized_strings->SetString("previewAltText",
      l10n_util::GetStringUTF16(IDS_OPTIONS_CHANGE_PICTURE_PREVIEW_ALT));
  localized_strings->SetString("authorCredit",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SET_WALLPAPER_AUTHOR_TEXT));
  localized_strings->SetString("photoFromCamera",
      l10n_util::GetStringUTF16(IDS_OPTIONS_CHANGE_PICTURE_PHOTO_FROM_CAMERA));
  localized_strings->SetString("photoFlippedAccessibleText",
      l10n_util::GetStringUTF16(IDS_OPTIONS_PHOTO_FLIP_ACCESSIBLE_TEXT));
  localized_strings->SetString("photoFlippedBackAccessibleText",
      l10n_util::GetStringUTF16(IDS_OPTIONS_PHOTO_FLIPBACK_ACCESSIBLE_TEXT));
  localized_strings->SetString("photoCaptureAccessibleText",
      l10n_util::GetStringUTF16(IDS_OPTIONS_PHOTO_CAPTURE_ACCESSIBLE_TEXT));
  localized_strings->SetString("photoDiscardAccessibleText",
      l10n_util::GetStringUTF16(IDS_OPTIONS_PHOTO_DISCARD_ACCESSIBLE_TEXT));
}

void ChangePictureOptionsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("chooseFile",
      base::Bind(&ChangePictureOptionsHandler::HandleChooseFile,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("takePhoto",
      base::Bind(&ChangePictureOptionsHandler::HandleTakePhoto,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("photoTaken",
      base::Bind(&ChangePictureOptionsHandler::HandlePhotoTaken,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("discardPhoto",
      base::Bind(&ChangePictureOptionsHandler::HandleDiscardPhoto,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("onChangePicturePageShown",
      base::Bind(&ChangePictureOptionsHandler::HandlePageShown,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("onChangePicturePageHidden",
      base::Bind(&ChangePictureOptionsHandler::HandlePageHidden,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("onChangePicturePageInitialized",
      base::Bind(&ChangePictureOptionsHandler::HandlePageInitialized,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("selectImage",
      base::Bind(&ChangePictureOptionsHandler::HandleSelectImage,
                 base::Unretained(this)));
}

void ChangePictureOptionsHandler::SendDefaultImages() {
  base::ListValue image_urls;
  for (int i = user_manager::kFirstDefaultImageIndex;
       i < user_manager::kDefaultImagesCount;
       ++i) {
    scoped_ptr<base::DictionaryValue> image_data(new base::DictionaryValue);
    image_data->SetString("url", user_manager::GetDefaultImageUrl(i));
    image_data->SetString(
        "author",
        l10n_util::GetStringUTF16(user_manager::kDefaultImageAuthorIDs[i]));
    image_data->SetString(
        "website",
        l10n_util::GetStringUTF16(user_manager::kDefaultImageWebsiteIDs[i]));
    image_data->SetString("title", user_manager::GetDefaultImageDescription(i));
    image_urls.Append(image_data.release());
  }
  web_ui()->CallJavascriptFunction("ChangePictureOptions.setDefaultImages",
                                   image_urls);
}

void ChangePictureOptionsHandler::HandleChooseFile(
    const base::ListValue* args) {
  DCHECK(args && args->empty());
  select_file_dialog_ = ui::SelectFileDialog::Create(
      this, new ChromeSelectFilePolicy(web_ui()->GetWebContents()));

  base::FilePath downloads_path;
  if (!PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS, &downloads_path)) {
    NOTREACHED();
    return;
  }

  // Static so we initialize it only once.
  CR_DEFINE_STATIC_LOCAL(ui::SelectFileDialog::FileTypeInfo, file_type_info,
      (GetUserImageFileTypeInfo()));

  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_OPEN_FILE,
      l10n_util::GetStringUTF16(IDS_DOWNLOAD_TITLE),
      downloads_path,
      &file_type_info,
      0,
      FILE_PATH_LITERAL(""),
      GetBrowserWindow(),
      NULL);
}

void ChangePictureOptionsHandler::HandleTakePhoto(
    const base::ListValue* args) {
  DCHECK(args->empty());
  ash::PlaySystemSoundIfSpokenFeedback(SOUND_CAMERA_SNAP);
}

void ChangePictureOptionsHandler::HandleDiscardPhoto(
    const base::ListValue* args) {
  DCHECK(args->empty());
  ash::PlaySystemSoundIfSpokenFeedback(SOUND_OBJECT_DELETE);
}

void ChangePictureOptionsHandler::HandlePhotoTaken(
    const base::ListValue* args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::string image_url;
  if (!args || args->GetSize() != 1 || !args->GetString(0, &image_url))
    NOTREACHED();
  DCHECK(!image_url.empty());

  std::string mime_type, charset, raw_data;
  if (!net::DataURL::Parse(GURL(image_url), &mime_type, &charset, &raw_data))
    NOTREACHED();
  DCHECK_EQ("image/png", mime_type);

  user_photo_ = gfx::ImageSkia();
  user_photo_data_url_ = image_url;

  if (image_decoder_.get())
    image_decoder_->set_delegate(NULL);
  image_decoder_ = new ImageDecoder(this, raw_data,
                                    ImageDecoder::DEFAULT_CODEC);
  scoped_refptr<base::MessageLoopProxy> task_runner =
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI);
  image_decoder_->Start(task_runner);
}

void ChangePictureOptionsHandler::HandlePageInitialized(
    const base::ListValue* args) {
  DCHECK(args && args->empty());
  SendDefaultImages();
}

void ChangePictureOptionsHandler::HandlePageShown(const base::ListValue* args) {
  DCHECK(args && args->empty());
  SendSelectedImage();
  UpdateProfileImage();
  CameraPresenceNotifier::GetInstance()->AddObserver(this);
}

void ChangePictureOptionsHandler::HandlePageHidden(
    const base::ListValue* args) {
  CameraPresenceNotifier::GetInstance()->RemoveObserver(this);
}

void ChangePictureOptionsHandler::SendSelectedImage() {
  const User* user = GetUser();
  DCHECK(!user->email().empty());

  previous_image_index_ = user->image_index();
  switch (previous_image_index_) {
    case User::kExternalImageIndex: {
      // User has image from camera/file, record it and add to the image list.
      previous_image_ = user->GetImage();
      SendOldImage(webui::GetBitmapDataUrl(*previous_image_.bitmap()));
      break;
    }
    case User::kProfileImageIndex: {
      // User has his/her Profile image as the current image.
      SendProfileImage(user->GetImage(), true);
      break;
    }
    default: {
      DCHECK(previous_image_index_ >= 0 &&
             previous_image_index_ < user_manager::kDefaultImagesCount);
      if (previous_image_index_ >= user_manager::kFirstDefaultImageIndex) {
        // User has image from the current set of default images.
        base::StringValue image_url(
            user_manager::GetDefaultImageUrl(previous_image_index_));
        web_ui()->CallJavascriptFunction(
            "ChangePictureOptions.setSelectedImage", image_url);
      } else {
        // User has an old default image, so present it in the same manner as a
        // previous image from file.
        SendOldImage(user_manager::GetDefaultImageUrl(previous_image_index_));
      }
    }
  }
}

void ChangePictureOptionsHandler::SendProfileImage(const gfx::ImageSkia& image,
                                                   bool should_select) {
  base::StringValue data_url(webui::GetBitmapDataUrl(*image.bitmap()));
  base::FundamentalValue select(should_select);
  web_ui()->CallJavascriptFunction("ChangePictureOptions.setProfileImage",
                                   data_url, select);
}

void ChangePictureOptionsHandler::UpdateProfileImage() {
  UserImageManager* user_image_manager =
      UserManager::Get()->GetUserImageManager(GetUser()->email());
  // If we have a downloaded profile image and haven't sent it in
  // |SendSelectedImage|, send it now (without selecting).
  if (previous_image_index_ != User::kProfileImageIndex &&
      !user_image_manager->DownloadedProfileImage().isNull())
    SendProfileImage(user_image_manager->DownloadedProfileImage(), false);

  user_image_manager->DownloadProfileImage(kProfileDownloadReason);
}

void ChangePictureOptionsHandler::SendOldImage(const std::string& image_url) {
  previous_image_url_ = image_url;
  base::StringValue url(image_url);
  web_ui()->CallJavascriptFunction("ChangePictureOptions.setOldImage", url);
}

void ChangePictureOptionsHandler::HandleSelectImage(
    const base::ListValue* args) {
  std::string image_url;
  std::string image_type;
  if (!args ||
      args->GetSize() != 2 ||
      !args->GetString(0, &image_url) ||
      !args->GetString(1, &image_type)) {
    NOTREACHED();
    return;
  }
  DCHECK(!image_url.empty());
  DCHECK(!image_type.empty());

  UserImageManager* user_image_manager =
      UserManager::Get()->GetUserImageManager(GetUser()->email());
  int image_index = User::kInvalidImageIndex;
  bool waiting_for_camera_photo = false;

  if (image_type == "old") {
    // Previous image (from camera or manually uploaded) re-selected.
    DCHECK(!previous_image_.isNull());
    user_image_manager->SaveUserImage(
        user_manager::UserImage::CreateAndEncode(previous_image_));

    UMA_HISTOGRAM_ENUMERATION("UserImage.ChangeChoice",
                              user_manager::kHistogramImageOld,
                              user_manager::kHistogramImagesCount);
    VLOG(1) << "Selected old user image";
  } else if (image_type == "default" &&
             user_manager::IsDefaultImageUrl(image_url, &image_index)) {
    // One of the default user images.
    user_image_manager->SaveUserDefaultImageIndex(image_index);

    UMA_HISTOGRAM_ENUMERATION(
        "UserImage.ChangeChoice",
        user_manager::GetDefaultImageHistogramValue(image_index),
        user_manager::kHistogramImagesCount);
    VLOG(1) << "Selected default user image: " << image_index;
  } else if (image_type == "camera") {
    // Camera image is selected.
    if (user_photo_.isNull()) {
      DCHECK(image_decoder_.get());
      waiting_for_camera_photo = true;
      VLOG(1) << "Still waiting for camera image to decode";
    } else {
      SetImageFromCamera(user_photo_);
    }
  } else if (image_type == "profile") {
    // Profile image selected. Could be previous (old) user image.
    user_image_manager->SaveUserImageFromProfileImage();

    if (previous_image_index_ == User::kProfileImageIndex) {
      UMA_HISTOGRAM_ENUMERATION("UserImage.ChangeChoice",
                                user_manager::kHistogramImageOld,
                                user_manager::kHistogramImagesCount);
      VLOG(1) << "Selected old (profile) user image";
    } else {
      UMA_HISTOGRAM_ENUMERATION("UserImage.ChangeChoice",
                                user_manager::kHistogramImageFromProfile,
                                user_manager::kHistogramImagesCount);
      VLOG(1) << "Selected profile image";
    }
  } else {
    NOTREACHED() << "Unexpected image type: " << image_type;
  }

  // Ignore the result of the previous decoding if it's no longer needed.
  if (!waiting_for_camera_photo && image_decoder_.get())
    image_decoder_->set_delegate(NULL);
}

void ChangePictureOptionsHandler::FileSelected(const base::FilePath& path,
                                               int index,
                                               void* params) {
  UserManager* user_manager = UserManager::Get();
  user_manager->GetUserImageManager(GetUser()->email())->
      SaveUserImageFromFile(path);
  UMA_HISTOGRAM_ENUMERATION("UserImage.ChangeChoice",
                            user_manager::kHistogramImageFromFile,
                            user_manager::kHistogramImagesCount);
  VLOG(1) << "Selected image from file";
}

void ChangePictureOptionsHandler::SetImageFromCamera(
    const gfx::ImageSkia& photo) {
  UserManager* user_manager = UserManager::Get();
  user_manager->GetUserImageManager(GetUser()->email())
      ->SaveUserImage(user_manager::UserImage::CreateAndEncode(photo));
  UMA_HISTOGRAM_ENUMERATION("UserImage.ChangeChoice",
                            user_manager::kHistogramImageFromCamera,
                            user_manager::kHistogramImagesCount);
  VLOG(1) << "Selected camera photo";
}

void ChangePictureOptionsHandler::SetCameraPresent(bool present) {
  base::FundamentalValue present_value(present);

  web_ui()->CallJavascriptFunction("ChangePictureOptions.setCameraPresent",
                                   present_value);
}

void ChangePictureOptionsHandler::OnCameraPresenceCheckDone(
    bool is_camera_present) {
  SetCameraPresent(is_camera_present);
}

void ChangePictureOptionsHandler::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_PROFILE_IMAGE_UPDATED) {
    // User profile image has been updated.
    SendProfileImage(*content::Details<const gfx::ImageSkia>(details).ptr(),
                     false);
  } else if (type == chrome::NOTIFICATION_LOGIN_USER_IMAGE_CHANGED) {
    // Not initialized yet.
    if (previous_image_index_ == User::kInvalidImageIndex)
      return;
    SendSelectedImage();
  }
}

gfx::NativeWindow ChangePictureOptionsHandler::GetBrowserWindow() const {
  Browser* browser =
      chrome::FindBrowserWithWebContents(web_ui()->GetWebContents());
  return browser->window()->GetNativeWindow();
}

void ChangePictureOptionsHandler::OnImageDecoded(
    const ImageDecoder* decoder,
    const SkBitmap& decoded_image) {
  DCHECK_EQ(image_decoder_.get(), decoder);
  image_decoder_ = NULL;
  user_photo_ = gfx::ImageSkia::CreateFrom1xBitmap(decoded_image);
  SetImageFromCamera(user_photo_);
}

void ChangePictureOptionsHandler::OnDecodeImageFailed(
    const ImageDecoder* decoder) {
  NOTREACHED() << "Failed to decode PNG image from WebUI";
}

User* ChangePictureOptionsHandler::GetUser() const {
  Profile* profile = Profile::FromWebUI(web_ui());
  User* user = ProfileHelper::Get()->GetUserByProfile(profile);
  if (!user)
    return UserManager::Get()->GetActiveUser();
  return user;
}

}  // namespace options
}  // namespace chromeos
