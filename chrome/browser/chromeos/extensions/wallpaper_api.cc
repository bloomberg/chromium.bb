// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/wallpaper_api.h"

#include <string>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/extensions/wallpaper_private_api.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/login/users/wallpaper/wallpaper_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/wallpaper_controller_client.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/wallpaper/wallpaper_info.h"
#include "extensions/browser/event_router.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "url/gurl.h"

using base::Value;
using content::BrowserThread;

typedef base::Callback<void(bool success, const std::string&)> FetchCallback;

namespace set_wallpaper = extensions::api::wallpaper::SetWallpaper;

namespace {

class WallpaperFetcher : public net::URLFetcherDelegate {
 public:
  WallpaperFetcher() {}

  ~WallpaperFetcher() override {}

  void FetchWallpaper(const GURL& url, FetchCallback callback) {
    CancelPreviousFetch();
    callback_ = callback;
    url_fetcher_ = net::URLFetcher::Create(url, net::URLFetcher::GET, this);
    url_fetcher_->SetRequestContext(
        g_browser_process->system_request_context());
    url_fetcher_->SetLoadFlags(net::LOAD_DISABLE_CACHE);
    url_fetcher_->Start();
  }

 private:
  // URLFetcherDelegate overrides:
  void OnURLFetchComplete(const net::URLFetcher* source) override {
    DCHECK(url_fetcher_.get() == source);

    bool success = source->GetStatus().is_success() &&
                   source->GetResponseCode() == net::HTTP_OK;
    std::string response;
    if (success) {
      source->GetResponseAsString(&response);
    } else {
      response = base::StringPrintf(
          "Downloading wallpaper %s failed. The response code is %d.",
          source->GetOriginalURL().ExtractFileName().c_str(),
          source->GetResponseCode());
    }
    url_fetcher_.reset();
    callback_.Run(success, response);
    callback_.Reset();
  }

  void CancelPreviousFetch() {
    if (url_fetcher_.get()) {
      callback_.Run(false, wallpaper_api_util::kCancelWallpaperMessage);
      callback_.Reset();
      url_fetcher_.reset();
    }
  }

  std::unique_ptr<net::URLFetcher> url_fetcher_;
  FetchCallback callback_;
};

base::LazyInstance<WallpaperFetcher>::DestructorAtExit g_wallpaper_fetcher =
    LAZY_INSTANCE_INITIALIZER;

// Gets the |User| for a given |BrowserContext|. The function will only return
// valid objects.
const user_manager::User* GetUserFromBrowserContext(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  DCHECK(profile);
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  DCHECK(user);
  return user;
}

}  // namespace

WallpaperSetWallpaperFunction::WallpaperSetWallpaperFunction() {
}

WallpaperSetWallpaperFunction::~WallpaperSetWallpaperFunction() {
}

bool WallpaperSetWallpaperFunction::RunAsync() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  params_ = set_wallpaper::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_);

  // Gets account id from the caller, ensuring multiprofile compatibility.
  const user_manager::User* user = GetUserFromBrowserContext(browser_context());
  account_id_ = user->GetAccountId();
  wallpaper_files_id_ =
      WallpaperControllerClient::Get()->GetFilesId(account_id_);

  if (params_->details.data) {
    StartDecode(*params_->details.data);
  } else if (params_->details.url) {
    GURL wallpaper_url(*params_->details.url);
    if (wallpaper_url.is_valid()) {
      g_wallpaper_fetcher.Get().FetchWallpaper(
          wallpaper_url,
          base::Bind(&WallpaperSetWallpaperFunction::OnWallpaperFetched, this));
    } else {
      SetError("URL is invalid.");
      SendResponse(false);
    }
  } else {
    SetError("Either url or data field is required.");
    SendResponse(false);
  }
  return true;
}

void WallpaperSetWallpaperFunction::OnWallpaperDecoded(
    const gfx::ImageSkia& image) {
  chromeos::WallpaperManager* wallpaper_manager =
      chromeos::WallpaperManager::Get();
  base::FilePath thumbnail_path = wallpaper_manager->GetCustomWallpaperPath(
      ash::WallpaperController::kThumbnailWallpaperSubDir, wallpaper_files_id_,
      params_->details.filename);

  wallpaper::WallpaperLayout layout = wallpaper_api_util::GetLayoutEnum(
      extensions::api::wallpaper::ToString(params_->details.layout));
  wallpaper_api_util::RecordCustomWallpaperLayout(layout);

  bool update_wallpaper =
      account_id_ ==
      user_manager::UserManager::Get()->GetActiveUser()->GetAccountId();
  wallpaper_manager->SetCustomWallpaper(
      account_id_, wallpaper_files_id_, params_->details.filename, layout,
      wallpaper::CUSTOMIZED, image, update_wallpaper);
  unsafe_wallpaper_decoder_ = NULL;

  // Save current extension name. It will be displayed in the component
  // wallpaper picker app. If current extension is the component wallpaper
  // picker, set an empty string.
  // TODO(xdai): This preference is unused now. For compatiblity concern, we
  // need to keep it until it's safe to clean it up.
  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (extension()->id() == extension_misc::kWallpaperManagerId) {
    profile->GetPrefs()->SetString(prefs::kCurrentWallpaperAppName,
                                   std::string());
  } else {
    profile->GetPrefs()->SetString(prefs::kCurrentWallpaperAppName,
                                   extension()->name());
  }

  if (!params_->details.thumbnail)
    SendResponse(true);

  // We need to generate thumbnail image anyway to make the current third party
  // wallpaper syncable through different devices.
  image.EnsureRepsForSupportedScales();
  std::unique_ptr<gfx::ImageSkia> deep_copy(image.DeepCopy());
  // Generates thumbnail before call api function callback. We can then
  // request thumbnail in the javascript callback.
  GetBlockingTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&WallpaperSetWallpaperFunction::GenerateThumbnail, this,
                     thumbnail_path, std::move(deep_copy)));
}

void WallpaperSetWallpaperFunction::GenerateThumbnail(
    const base::FilePath& thumbnail_path,
    std::unique_ptr<gfx::ImageSkia> image) {
  chromeos::AssertCalledOnWallpaperSequence(GetBlockingTaskRunner());
  if (!base::PathExists(thumbnail_path.DirName()))
    base::CreateDirectory(thumbnail_path.DirName());

  scoped_refptr<base::RefCountedBytes> original_data;
  scoped_refptr<base::RefCountedBytes> thumbnail_data;
  chromeos::WallpaperManager::Get()->ResizeImage(
      *image, wallpaper::WALLPAPER_LAYOUT_STRETCH, image->width(),
      image->height(), &original_data, NULL);
  chromeos::WallpaperManager::Get()->ResizeImage(
      *image, wallpaper::WALLPAPER_LAYOUT_STRETCH,
      chromeos::kWallpaperThumbnailWidth, chromeos::kWallpaperThumbnailHeight,
      &thumbnail_data, NULL);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&WallpaperSetWallpaperFunction::ThumbnailGenerated, this,
                     base::RetainedRef(original_data),
                     base::RetainedRef(thumbnail_data)));
}

void WallpaperSetWallpaperFunction::ThumbnailGenerated(
    base::RefCountedBytes* original_data,
    base::RefCountedBytes* thumbnail_data) {
  std::unique_ptr<Value> original_result = Value::CreateWithCopiedBuffer(
      reinterpret_cast<const char*>(original_data->front()),
      original_data->size());
  std::unique_ptr<Value> thumbnail_result = Value::CreateWithCopiedBuffer(
      reinterpret_cast<const char*>(thumbnail_data->front()),
      thumbnail_data->size());

  if (params_->details.thumbnail) {
    SetResult(thumbnail_result->CreateDeepCopy());
    SendResponse(true);
  }

  // Inform the native Wallpaper Picker Application that the current wallpaper
  // has been modified by a third party application.
  if (extension()->id() != extension_misc::kWallpaperManagerId) {
    Profile* profile = Profile::FromBrowserContext(browser_context());
    extensions::EventRouter* event_router =
        extensions::EventRouter::Get(profile);
    std::unique_ptr<base::ListValue> event_args(new base::ListValue());
    event_args->Append(original_result->CreateDeepCopy());
    event_args->Append(thumbnail_result->CreateDeepCopy());
    event_args->AppendString(
        extensions::api::wallpaper::ToString(params_->details.layout));
    // Setting wallpaper from right click menu in 'Files' app is a feature that
    // was implemented in crbug.com/578935. Since 'Files' app is a built-in v1
    // app in ChromeOS, we should treat it slightly differently with other third
    // party apps: the wallpaper set by the 'Files' app should still be syncable
    // and it should not appear in the wallpaper grid in the Wallpaper Picker.
    // But we should not display the 'wallpaper-set-by-mesage' since it might
    // introduce confusion as shown in crbug.com/599407.
    event_args->AppendString(
        (extension()->id() == file_manager::kFileManagerAppId)
            ? std::string()
            : extension()->name());
    std::unique_ptr<extensions::Event> event(new extensions::Event(
        extensions::events::WALLPAPER_PRIVATE_ON_WALLPAPER_CHANGED_BY_3RD_PARTY,
        extensions::api::wallpaper_private::OnWallpaperChangedBy3rdParty::
            kEventName,
        std::move(event_args)));
    event_router->DispatchEventToExtension(extension_misc::kWallpaperManagerId,
                                           std::move(event));
  }
}

void WallpaperSetWallpaperFunction::OnWallpaperFetched(
    bool success,
    const std::string& response) {
  if (success) {
    params_->details.data.reset(
        new std::vector<char>(response.begin(), response.end()));
    StartDecode(*params_->details.data);
  } else {
    SetError(response);
    SendResponse(false);
  }
}
