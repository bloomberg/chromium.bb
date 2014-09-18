// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_app_data.h"

#include <vector>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/memory/ref_counted_memory.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_data_delegate.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/webstore_data_fetcher.h"
#include "chrome/browser/extensions/webstore_install_helper.h"
#include "chrome/browser/image_decoder.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/image_loader.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image.h"

using content::BrowserThread;

namespace chromeos {

namespace {

// Keys for local state data. See sample layout in KioskAppManager.
const char kKeyName[] = "name";
const char kKeyIcon[] = "icon";

const char kInvalidWebstoreResponseError[] = "Invalid Chrome Web Store reponse";

// Icon file extension.
const char kIconFileExtension[] = ".png";

// Save |raw_icon| for given |app_id|.
void SaveIconToLocalOnBlockingPool(
    const base::FilePath& icon_path,
    scoped_refptr<base::RefCountedString> raw_icon) {
  DCHECK(BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());

  base::FilePath dir = icon_path.DirName();
  if (!base::PathExists(dir))
    CHECK(base::CreateDirectory(dir));

  CHECK_EQ(static_cast<int>(raw_icon->size()),
           base::WriteFile(icon_path,
                           raw_icon->data().c_str(), raw_icon->size()));
}

// Returns true for valid kiosk app manifest.
bool IsValidKioskAppManifest(const extensions::Manifest& manifest) {
  bool kiosk_enabled;
  if (manifest.GetBoolean(extensions::manifest_keys::kKioskEnabled,
                          &kiosk_enabled)) {
    return kiosk_enabled;
  }

  return false;
}

std::string ValueToString(const base::Value* value) {
  std::string json;
  base::JSONWriter::Write(value, &json);
  return json;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// KioskAppData::IconLoader
// Loads locally stored icon data and decode it.

class KioskAppData::IconLoader : public ImageDecoder::Delegate {
 public:
  enum LoadResult {
    SUCCESS,
    FAILED_TO_LOAD,
    FAILED_TO_DECODE,
  };

  IconLoader(const base::WeakPtr<KioskAppData>& client,
             const base::FilePath& icon_path)
      : client_(client),
        icon_path_(icon_path),
        load_result_(SUCCESS) {}

  void Start() {
    base::SequencedWorkerPool* pool = BrowserThread::GetBlockingPool();
    base::SequencedWorkerPool::SequenceToken token = pool->GetSequenceToken();
    task_runner_ = pool->GetSequencedTaskRunnerWithShutdownBehavior(
        token,
        base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN);
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(&IconLoader::LoadOnBlockingPool,
                                      base::Unretained(this)));
  }

 private:
  friend class base::RefCountedThreadSafe<IconLoader>;

  virtual ~IconLoader() {}

  // Loads the icon from locally stored |icon_path_| on the blocking pool
  void LoadOnBlockingPool() {
    DCHECK(task_runner_->RunsTasksOnCurrentThread());

    std::string data;
    if (!base::ReadFileToString(base::FilePath(icon_path_), &data)) {
      ReportResultOnBlockingPool(FAILED_TO_LOAD);
      return;
    }
    raw_icon_ = base::RefCountedString::TakeString(&data);

    scoped_refptr<ImageDecoder> image_decoder = new ImageDecoder(
        this, raw_icon_->data(), ImageDecoder::DEFAULT_CODEC);
    image_decoder->Start(task_runner_);
  }

  void ReportResultOnBlockingPool(LoadResult result) {
    DCHECK(task_runner_->RunsTasksOnCurrentThread());

    load_result_ = result;
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&IconLoader::ReportResultOnUIThread,
                   base::Unretained(this)));
  }

  void NotifyClient() {
    if (!client_)
      return;

    if (load_result_ == SUCCESS)
      client_->OnIconLoadSuccess(raw_icon_, icon_);
    else
      client_->OnIconLoadFailure();
  }

  void ReportResultOnUIThread() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    NotifyClient();
    delete this;
  }

  // ImageDecoder::Delegate overrides:
  virtual void OnImageDecoded(const ImageDecoder* decoder,
                              const SkBitmap& decoded_image) OVERRIDE {
    icon_ = gfx::ImageSkia::CreateFrom1xBitmap(decoded_image);
    icon_.MakeThreadSafe();
    ReportResultOnBlockingPool(SUCCESS);
  }

  virtual void OnDecodeImageFailed(const ImageDecoder* decoder) OVERRIDE {
    ReportResultOnBlockingPool(FAILED_TO_DECODE);
  }

  base::WeakPtr<KioskAppData> client_;
  base::FilePath icon_path_;

  LoadResult load_result_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  gfx::ImageSkia icon_;
  scoped_refptr<base::RefCountedString> raw_icon_;

  DISALLOW_COPY_AND_ASSIGN(IconLoader);
};

////////////////////////////////////////////////////////////////////////////////
// KioskAppData::WebstoreDataParser
// Use WebstoreInstallHelper to parse the manifest and decode the icon.

class KioskAppData::WebstoreDataParser
    : public extensions::WebstoreInstallHelper::Delegate {
 public:
  explicit WebstoreDataParser(const base::WeakPtr<KioskAppData>& client)
      : client_(client) {}

  void Start(const std::string& app_id,
             const std::string& manifest,
             const GURL& icon_url,
             net::URLRequestContextGetter* context_getter) {
    scoped_refptr<extensions::WebstoreInstallHelper> webstore_helper =
        new extensions::WebstoreInstallHelper(this,
                                              app_id,
                                              manifest,
                                              "",  // No icon data.
                                              icon_url,
                                              context_getter);
    webstore_helper->Start();
  }

 private:
  friend class base::RefCounted<WebstoreDataParser>;

  virtual ~WebstoreDataParser() {}

  void ReportFailure() {
    if (client_)
      client_->OnWebstoreParseFailure();

    delete this;
  }

  // WebstoreInstallHelper::Delegate overrides:
  virtual void OnWebstoreParseSuccess(
      const std::string& id,
      const SkBitmap& icon,
      base::DictionaryValue* parsed_manifest) OVERRIDE {
    // Takes ownership of |parsed_manifest|.
    extensions::Manifest manifest(
        extensions::Manifest::INVALID_LOCATION,
        scoped_ptr<base::DictionaryValue>(parsed_manifest));

    if (!IsValidKioskAppManifest(manifest)) {
      ReportFailure();
      return;
    }

    if (client_)
      client_->OnWebstoreParseSuccess(icon);
    delete this;
  }
  virtual void OnWebstoreParseFailure(
      const std::string& id,
      InstallHelperResultCode result_code,
      const std::string& error_message) OVERRIDE {
    ReportFailure();
  }

  base::WeakPtr<KioskAppData> client_;

  DISALLOW_COPY_AND_ASSIGN(WebstoreDataParser);
};

////////////////////////////////////////////////////////////////////////////////
// KioskAppData

KioskAppData::KioskAppData(KioskAppDataDelegate* delegate,
                           const std::string& app_id,
                           const std::string& user_id)
    : delegate_(delegate),
      status_(STATUS_INIT),
      app_id_(app_id),
      user_id_(user_id) {
}

KioskAppData::~KioskAppData() {}

void KioskAppData::Load() {
  SetStatus(STATUS_LOADING);

  if (LoadFromCache())
    return;

  StartFetch();
}

void KioskAppData::ClearCache() {
  PrefService* local_state = g_browser_process->local_state();

  DictionaryPrefUpdate dict_update(local_state,
                                   KioskAppManager::kKioskDictionaryName);

  std::string app_key = std::string(KioskAppManager::kKeyApps) + '.' + app_id_;
  dict_update->Remove(app_key, NULL);

  if (!icon_path_.empty()) {
    BrowserThread::PostBlockingPoolTask(
        FROM_HERE,
        base::Bind(base::IgnoreResult(&base::DeleteFile), icon_path_, false));
  }
}

void KioskAppData::LoadFromInstalledApp(Profile* profile,
                                        const extensions::Extension* app) {
  SetStatus(STATUS_LOADING);

  if (!app) {
    app = extensions::ExtensionSystem::Get(profile)
              ->extension_service()
              ->GetInstalledExtension(app_id_);
  }

  DCHECK_EQ(app_id_, app->id());

  name_ = app->name();

  const int kIconSize = extension_misc::EXTENSION_ICON_LARGE;
  extensions::ExtensionResource image = extensions::IconsInfo::GetIconResource(
      app, kIconSize, ExtensionIconSet::MATCH_BIGGER);
  extensions::ImageLoader::Get(profile)->LoadImageAsync(
      app, image, gfx::Size(kIconSize, kIconSize),
      base::Bind(&KioskAppData::OnExtensionIconLoaded, AsWeakPtr()));
}

bool KioskAppData::IsLoading() const {
  return status_ == STATUS_LOADING;
}

void KioskAppData::SetStatus(Status status) {
  if (status_ == status)
    return;

  status_ = status;

  if (!delegate_)
    return;

  switch (status_) {
    case STATUS_INIT:
      break;
    case STATUS_LOADING:
    case STATUS_LOADED:
      delegate_->OnKioskAppDataChanged(app_id_);
      break;
    case STATUS_ERROR:
      delegate_->OnKioskAppDataLoadFailure(app_id_);
      break;
  }
}

net::URLRequestContextGetter* KioskAppData::GetRequestContextGetter() {
  return g_browser_process->system_request_context();
}

bool KioskAppData::LoadFromCache() {
  std::string app_key = std::string(KioskAppManager::kKeyApps) + '.' + app_id_;
  std::string name_key = app_key + '.' + kKeyName;
  std::string icon_path_key = app_key + '.' + kKeyIcon;

  PrefService* local_state = g_browser_process->local_state();
  const base::DictionaryValue* dict =
      local_state->GetDictionary(KioskAppManager::kKioskDictionaryName);

  icon_path_.clear();
  std::string icon_path_string;
  if (!dict->GetString(name_key, &name_) ||
      !dict->GetString(icon_path_key, &icon_path_string)) {
    return false;
  }
  icon_path_ = base::FilePath(icon_path_string);

  // IconLoader deletes itself when done.
  (new IconLoader(AsWeakPtr(), icon_path_))->Start();
  return true;
}

void KioskAppData::SetCache(const std::string& name,
                            const base::FilePath& icon_path) {
  std::string app_key = std::string(KioskAppManager::kKeyApps) + '.' + app_id_;
  std::string name_key = app_key + '.' + kKeyName;
  std::string icon_path_key = app_key + '.' + kKeyIcon;

  PrefService* local_state = g_browser_process->local_state();
  DictionaryPrefUpdate dict_update(local_state,
                                   KioskAppManager::kKioskDictionaryName);
  dict_update->SetString(name_key, name);
  dict_update->SetString(icon_path_key, icon_path.value());
  icon_path_ = icon_path;
}

void KioskAppData::SetCache(const std::string& name, const SkBitmap& icon) {
  icon_ = gfx::ImageSkia::CreateFrom1xBitmap(icon);
  icon_.MakeThreadSafe();

  std::vector<unsigned char> image_data;
  CHECK(gfx::PNGCodec::EncodeBGRASkBitmap(icon, false, &image_data));
  raw_icon_ = new base::RefCountedString;
  raw_icon_->data().assign(image_data.begin(), image_data.end());

  base::FilePath cache_dir;
  if (delegate_)
    delegate_->GetKioskAppIconCacheDir(&cache_dir);

  base::FilePath icon_path =
      cache_dir.AppendASCII(app_id_).AddExtension(kIconFileExtension);
  BrowserThread::GetBlockingPool()->PostTask(
      FROM_HERE,
      base::Bind(&SaveIconToLocalOnBlockingPool, icon_path, raw_icon_));

  SetCache(name, icon_path);
}

void KioskAppData::OnExtensionIconLoaded(const gfx::Image& icon) {
  if (icon.IsEmpty()) {
    LOG(WARNING) << "Failed to load icon from installed app"
                 << ", id=" << app_id_;
    SetCache(name_, *extensions::util::GetDefaultAppIcon().bitmap());
  } else {
    SetCache(name_, icon.AsBitmap());
  }

  SetStatus(STATUS_LOADED);
}

void KioskAppData::OnIconLoadSuccess(
    const scoped_refptr<base::RefCountedString>& raw_icon,
    const gfx::ImageSkia& icon) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  raw_icon_ = raw_icon;
  icon_ = icon;
  SetStatus(STATUS_LOADED);
}

void KioskAppData::OnIconLoadFailure() {
  // Re-fetch data from web store when failed to load cached data.
  StartFetch();
}

void KioskAppData::OnWebstoreParseSuccess(const SkBitmap& icon) {
  SetCache(name_, icon);
  SetStatus(STATUS_LOADED);
}

void KioskAppData::OnWebstoreParseFailure() {
  SetStatus(STATUS_ERROR);
}

void KioskAppData::StartFetch() {
  webstore_fetcher_.reset(new extensions::WebstoreDataFetcher(
      this,
      GetRequestContextGetter(),
      GURL(),
      app_id_));
  webstore_fetcher_->set_max_auto_retries(3);
  webstore_fetcher_->Start();
}

void KioskAppData::OnWebstoreRequestFailure() {
  SetStatus(STATUS_ERROR);
}

void KioskAppData::OnWebstoreResponseParseSuccess(
      scoped_ptr<base::DictionaryValue> webstore_data) {
  // Takes ownership of |webstore_data|.
  webstore_fetcher_.reset();

  std::string manifest;
  if (!CheckResponseKeyValue(webstore_data.get(), kManifestKey, &manifest))
    return;

  if (!CheckResponseKeyValue(webstore_data.get(), kLocalizedNameKey, &name_))
    return;

  std::string icon_url_string;
  if (!CheckResponseKeyValue(webstore_data.get(), kIconUrlKey,
                             &icon_url_string))
    return;

  GURL icon_url = GURL(extension_urls::GetWebstoreLaunchURL()).Resolve(
      icon_url_string);
  if (!icon_url.is_valid()) {
    LOG(ERROR) << "Webstore response error (icon url): "
               << ValueToString(webstore_data.get());
    OnWebstoreResponseParseFailure(kInvalidWebstoreResponseError);
    return;
  }

  // WebstoreDataParser deletes itself when done.
  (new WebstoreDataParser(AsWeakPtr()))->Start(app_id_,
                                               manifest,
                                               icon_url,
                                               GetRequestContextGetter());
}

void KioskAppData::OnWebstoreResponseParseFailure(const std::string& error) {
  LOG(ERROR) << "Webstore failed for kiosk app " << app_id_
             << ", " << error;
  webstore_fetcher_.reset();
  SetStatus(STATUS_ERROR);
}

bool KioskAppData::CheckResponseKeyValue(const base::DictionaryValue* response,
                                         const char* key,
                                         std::string* value) {
  if (!response->GetString(key, value)) {
    LOG(ERROR) << "Webstore response error (" << key
               << "): " << ValueToString(response);
    OnWebstoreResponseParseFailure(kInvalidWebstoreResponseError);
    return false;
  }
  return true;
}

}  // namespace chromeos
