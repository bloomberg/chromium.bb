// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_app_data.h"

#include <vector>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
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
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/image_loader.h"
#include "extensions/browser/sandboxed_unpacker.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/manifest_handlers/kiosk_mode_info.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image.h"

using content::BrowserThread;

namespace chromeos {

namespace {

// Keys for local state data. See sample layout in KioskAppManager.
const char kKeyName[] = "name";
const char kKeyIcon[] = "icon";
const char kKeyRequiredPlatformVersion[] = "required_platform_version";

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

std::string ValueToString(const base::Value& value) {
  std::string json;
  base::JSONWriter::Write(value, &json);
  return json;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// KioskAppData::CrxLoader
// Loads meta data from crx file.

class KioskAppData::CrxLoader : public extensions::SandboxedUnpackerClient {
 public:
  CrxLoader(const base::WeakPtr<KioskAppData>& client,
            const base::FilePath& crx_file)
      : client_(client),
        crx_file_(crx_file),
        success_(false) {
  }

  void Start() {
    base::SequencedWorkerPool* pool = BrowserThread::GetBlockingPool();
    base::SequencedWorkerPool::SequenceToken token =
        pool->GetNamedSequenceToken("KioskAppData.CrxLoaderWorker");
    task_runner_ = pool->GetSequencedTaskRunnerWithShutdownBehavior(
        token,
        base::SequencedWorkerPool::SKIP_ON_SHUTDOWN);
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(&CrxLoader::StartOnBlockingPool, this));
  }

  bool success() const { return success_; }
  const base::FilePath& crx_file() const { return crx_file_; }
  const std::string& name() const { return name_; }
  const SkBitmap& icon() const { return icon_; }
  const std::string& required_platform_version() const {
    return required_platform_version_;
  }

 private:
  ~CrxLoader() override {};

  // extensions::SandboxedUnpackerClient
  void OnUnpackSuccess(const base::FilePath& temp_dir,
                       const base::FilePath& extension_root,
                       const base::DictionaryValue* original_manifest,
                       const extensions::Extension* extension,
                       const SkBitmap& install_icon) override {
    DCHECK(task_runner_->RunsTasksOnCurrentThread());

    success_ = true;
    name_ = extension->name();
    icon_ = install_icon;
    required_platform_version_ =
        extensions::KioskModeInfo::Get(extension)->required_platform_version;
    NotifyFinishedOnBlockingPool();
  }
  void OnUnpackFailure(const extensions::CrxInstallError& error) override {
    DCHECK(task_runner_->RunsTasksOnCurrentThread());

    success_ = false;
    NotifyFinishedOnBlockingPool();
  }

  void StartOnBlockingPool() {
    DCHECK(task_runner_->RunsTasksOnCurrentThread());

    if (!temp_dir_.CreateUniqueTempDir()) {
      success_ = false;
      NotifyFinishedOnBlockingPool();
      return;
    }

    scoped_refptr<extensions::SandboxedUnpacker> unpacker(
        new extensions::SandboxedUnpacker(
            extensions::Manifest::INTERNAL, extensions::Extension::NO_FLAGS,
            temp_dir_.path(), task_runner_.get(), this));
    unpacker->StartWithCrx(extensions::CRXFileInfo(crx_file_));
  }

  void NotifyFinishedOnBlockingPool() {
    DCHECK(task_runner_->RunsTasksOnCurrentThread());

    if (!temp_dir_.Delete()) {
      LOG(WARNING) << "Can not delete temp directory at "
                   << temp_dir_.path().value();
    }

    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&CrxLoader::NotifyFinishedOnUIThread, this));
  }

  void NotifyFinishedOnUIThread() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    if (client_)
      client_->OnCrxLoadFinished(this);
  }

  base::WeakPtr<KioskAppData> client_;
  base::FilePath crx_file_;
  bool success_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::ScopedTempDir temp_dir_;

  // Extracted meta data.
  std::string name_;
  SkBitmap icon_;
  std::string required_platform_version_;

  DISALLOW_COPY_AND_ASSIGN(CrxLoader);
};

////////////////////////////////////////////////////////////////////////////////
// KioskAppData::IconLoader
// Loads locally stored icon data and decode it.

class KioskAppData::IconLoader {
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
        base::SequencedWorkerPool::SKIP_ON_SHUTDOWN);
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(&IconLoader::LoadOnBlockingPool,
                                      base::Unretained(this)));
  }

 private:
  friend class base::RefCountedThreadSafe<IconLoader>;

  ~IconLoader() {}

  class IconImageRequest : public ImageDecoder::ImageRequest {
   public:
    IconImageRequest(
        const scoped_refptr<base::SequencedTaskRunner>& task_runner,
        IconLoader* icon_loader)
        : ImageRequest(task_runner), icon_loader_(icon_loader) {}

    void OnImageDecoded(const SkBitmap& decoded_image) override {
      icon_loader_->icon_ = gfx::ImageSkia::CreateFrom1xBitmap(decoded_image);
      icon_loader_->icon_.MakeThreadSafe();
      icon_loader_->ReportResultOnBlockingPool(SUCCESS);
      delete this;
    }

    void OnDecodeImageFailed() override {
      icon_loader_->ReportResultOnBlockingPool(FAILED_TO_DECODE);
      delete this;
    }

   private:
    ~IconImageRequest() override {}
    IconLoader* icon_loader_;
  };

  // Loads the icon from locally stored |icon_path_| on the blocking pool
  void LoadOnBlockingPool() {
    DCHECK(task_runner_->RunsTasksOnCurrentThread());

    std::string data;
    if (!base::ReadFileToString(base::FilePath(icon_path_), &data)) {
      ReportResultOnBlockingPool(FAILED_TO_LOAD);
      return;
    }
    raw_icon_ = base::RefCountedString::TakeString(&data);

    IconImageRequest* image_request = new IconImageRequest(task_runner_, this);
    ImageDecoder::Start(image_request, raw_icon_->data());
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
      client_->OnIconLoadSuccess(icon_);
    else
      client_->OnIconLoadFailure();
  }

  void ReportResultOnUIThread() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    NotifyClient();
    delete this;
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
                                              icon_url,
                                              context_getter);
    webstore_helper->Start();
  }

 private:
  friend class base::RefCounted<WebstoreDataParser>;

  ~WebstoreDataParser() override {}

  void ReportFailure() {
    if (client_)
      client_->OnWebstoreParseFailure();

    delete this;
  }

  // WebstoreInstallHelper::Delegate overrides:
  void OnWebstoreParseSuccess(const std::string& id,
                              const SkBitmap& icon,
                              base::DictionaryValue* parsed_manifest) override {
    // Takes ownership of |parsed_manifest|.
    extensions::Manifest manifest(
        extensions::Manifest::INVALID_LOCATION,
        scoped_ptr<base::DictionaryValue>(parsed_manifest));

    if (!IsValidKioskAppManifest(manifest)) {
      ReportFailure();
      return;
    }

    std::string required_platform_version;
    if (manifest.HasPath(
            extensions::manifest_keys::kKioskRequiredPlatformVersion) &&
        (!manifest.GetString(
             extensions::manifest_keys::kKioskRequiredPlatformVersion,
             &required_platform_version) ||
         !extensions::KioskModeInfo::IsValidPlatformVersion(
             required_platform_version))) {
      ReportFailure();
      return;
    }

    if (client_)
      client_->OnWebstoreParseSuccess(icon, required_platform_version);
    delete this;
  }
  void OnWebstoreParseFailure(const std::string& id,
                              InstallHelperResultCode result_code,
                              const std::string& error_message) override {
    ReportFailure();
  }

  base::WeakPtr<KioskAppData> client_;

  DISALLOW_COPY_AND_ASSIGN(WebstoreDataParser);
};

////////////////////////////////////////////////////////////////////////////////
// KioskAppData

KioskAppData::KioskAppData(KioskAppDataDelegate* delegate,
                           const std::string& app_id,
                           const std::string& user_id,
                           const GURL& update_url)
    : delegate_(delegate),
      status_(STATUS_INIT),
      app_id_(app_id),
      user_id_(user_id),
      update_url_(update_url) {
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
  required_platform_version_ =
      extensions::KioskModeInfo::Get(app)->required_platform_version;

  const int kIconSize = extension_misc::EXTENSION_ICON_LARGE;
  extensions::ExtensionResource image = extensions::IconsInfo::GetIconResource(
      app, kIconSize, ExtensionIconSet::MATCH_BIGGER);
  extensions::ImageLoader::Get(profile)->LoadImageAsync(
      app, image, gfx::Size(kIconSize, kIconSize),
      base::Bind(&KioskAppData::OnExtensionIconLoaded, AsWeakPtr()));
}

void KioskAppData::SetCachedCrx(const base::FilePath& crx_file) {
  if (crx_file_ == crx_file)
    return;

  crx_file_ = crx_file;
  MaybeLoadFromCrx();
}

bool KioskAppData::IsLoading() const {
  return status_ == STATUS_LOADING;
}

bool KioskAppData::IsFromWebStore() const {
  return update_url_.is_empty() ||
         extension_urls::IsWebstoreUpdateUrl(update_url_);
}

void KioskAppData::SetStatusForTest(Status status) {
  SetStatus(status);
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
  const std::string app_key =
      std::string(KioskAppManager::kKeyApps) + '.' + app_id_;
  const std::string name_key = app_key + '.' + kKeyName;
  const std::string icon_path_key = app_key + '.' + kKeyIcon;
  const std::string required_platform_version_key =
      app_key + '.' + kKeyRequiredPlatformVersion;

  PrefService* local_state = g_browser_process->local_state();
  const base::DictionaryValue* dict =
      local_state->GetDictionary(KioskAppManager::kKioskDictionaryName);

  icon_path_.clear();
  std::string icon_path_string;
  if (!dict->GetString(name_key, &name_) ||
      !dict->GetString(icon_path_key, &icon_path_string) ||
      !dict->GetString(required_platform_version_key,
                       &required_platform_version_)) {
    return false;
  }
  icon_path_ = base::FilePath(icon_path_string);

  // IconLoader deletes itself when done.
  (new IconLoader(AsWeakPtr(), icon_path_))->Start();
  return true;
}

void KioskAppData::SetCache(const std::string& name,
                            const base::FilePath& icon_path,
                            const std::string& required_platform_version) {
  name_ = name;
  icon_path_ = icon_path;
  required_platform_version_ = required_platform_version;

  const std::string app_key =
      std::string(KioskAppManager::kKeyApps) + '.' + app_id_;
  const std::string name_key = app_key + '.' + kKeyName;
  const std::string icon_path_key = app_key + '.' + kKeyIcon;
  const std::string required_platform_version_key =
      app_key + '.' + kKeyRequiredPlatformVersion;

  PrefService* local_state = g_browser_process->local_state();
  DictionaryPrefUpdate dict_update(local_state,
                                   KioskAppManager::kKioskDictionaryName);
  dict_update->SetString(name_key, name);
  dict_update->SetString(icon_path_key, icon_path.value());
  dict_update->SetString(required_platform_version_key,
                         required_platform_version);
}

void KioskAppData::SetCache(const std::string& name,
                            const SkBitmap& icon,
                            const std::string& required_platform_version) {
  icon_ = gfx::ImageSkia::CreateFrom1xBitmap(icon);
  icon_.MakeThreadSafe();

  std::vector<unsigned char> image_data;
  CHECK(gfx::PNGCodec::EncodeBGRASkBitmap(icon, false, &image_data));
  scoped_refptr<base::RefCountedString> raw_icon(new base::RefCountedString);
  raw_icon->data().assign(image_data.begin(), image_data.end());

  base::FilePath cache_dir;
  if (delegate_)
    delegate_->GetKioskAppIconCacheDir(&cache_dir);

  base::FilePath icon_path =
      cache_dir.AppendASCII(app_id_).AddExtension(kIconFileExtension);
  BrowserThread::GetBlockingPool()->PostTask(
      FROM_HERE,
      base::Bind(&SaveIconToLocalOnBlockingPool, icon_path, raw_icon));

  SetCache(name, icon_path, required_platform_version);
}

void KioskAppData::OnExtensionIconLoaded(const gfx::Image& icon) {
  if (icon.IsEmpty()) {
    LOG(WARNING) << "Failed to load icon from installed app"
                 << ", id=" << app_id_;
    SetCache(name_, *extensions::util::GetDefaultAppIcon().bitmap(),
             required_platform_version_);
  } else {
    SetCache(name_, icon.AsBitmap(), required_platform_version_);
  }

  SetStatus(STATUS_LOADED);
}

void KioskAppData::OnIconLoadSuccess(const gfx::ImageSkia& icon) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  icon_ = icon;
  SetStatus(STATUS_LOADED);
}

void KioskAppData::OnIconLoadFailure() {
  // Re-fetch data from web store when failed to load cached data.
  StartFetch();
}

void KioskAppData::OnWebstoreParseSuccess(
    const SkBitmap& icon,
    const std::string& required_platform_version) {
  SetCache(name_, icon, required_platform_version);
  SetStatus(STATUS_LOADED);
}

void KioskAppData::OnWebstoreParseFailure() {
  SetStatus(STATUS_ERROR);
}

void KioskAppData::StartFetch() {
  if (!IsFromWebStore()) {
    MaybeLoadFromCrx();
    return;
  }

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
               << ValueToString(*webstore_data);
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
               << "): " << ValueToString(*response);
    OnWebstoreResponseParseFailure(kInvalidWebstoreResponseError);
    return false;
  }
  return true;
}

void KioskAppData::MaybeLoadFromCrx() {
  if (status_ == STATUS_LOADED || crx_file_.empty())
    return;

  scoped_refptr<CrxLoader> crx_loader(new CrxLoader(AsWeakPtr(), crx_file_));
  crx_loader->Start();
}

void KioskAppData::OnCrxLoadFinished(const CrxLoader* crx_loader) {
  DCHECK(crx_loader);

  if (crx_loader->crx_file() != crx_file_)
    return;

  if (!crx_loader->success()) {
    SetStatus(STATUS_ERROR);
    return;
  }

  SkBitmap icon = crx_loader->icon();
  if (icon.empty())
    icon = *extensions::util::GetDefaultAppIcon().bitmap();
  SetCache(crx_loader->name(), icon, crx_loader->required_platform_version());

  SetStatus(STATUS_LOADED);
}

}  // namespace chromeos
