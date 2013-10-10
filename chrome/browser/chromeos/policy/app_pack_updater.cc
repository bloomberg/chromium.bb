// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/app_pack_updater.h"

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/values.h"
#include "chrome/browser/chromeos/policy/enterprise_install_attributes.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/extensions/external_loader.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "chromeos/settings/cros_settings_names.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace policy {

namespace {

// Directory where the AppPack extensions are cached.
const char kAppPackCacheDir[] = "/var/cache/app_pack";

}  // namespace

// A custom extensions::ExternalLoader that the AppPackUpdater creates and uses
// to publish AppPack updates to the extensions system.
class AppPackExternalLoader
    : public extensions::ExternalLoader,
      public base::SupportsWeakPtr<AppPackExternalLoader> {
 public:
  AppPackExternalLoader() {}

  // Used by the AppPackUpdater to update the current list of extensions.
  // The format of |prefs| is detailed in the extensions::ExternalLoader/
  // Provider headers.
  void SetCurrentAppPackExtensions(scoped_ptr<base::DictionaryValue> prefs) {
    app_pack_prefs_.Swap(prefs.get());
    StartLoading();
  }

  // Implementation of extensions::ExternalLoader:
  virtual void StartLoading() OVERRIDE {
    prefs_.reset(app_pack_prefs_.DeepCopy());
    VLOG(1) << "AppPack extension loader publishing "
            << app_pack_prefs_.size() << " crx files.";
    LoadFinished();
  }

 protected:
  virtual ~AppPackExternalLoader() {}

 private:
  base::DictionaryValue app_pack_prefs_;

  DISALLOW_COPY_AND_ASSIGN(AppPackExternalLoader);
};

AppPackUpdater::AppPackUpdater(net::URLRequestContextGetter* request_context,
                               EnterpriseInstallAttributes* install_attributes)
    : weak_ptr_factory_(this),
      created_extension_loader_(false),
      install_attributes_(install_attributes),
      external_cache_(base::FilePath(kAppPackCacheDir),
                      request_context,
                      content::BrowserThread::GetBlockingPool()->
                          GetSequencedTaskRunnerWithShutdownBehavior(
                              content::BrowserThread::GetBlockingPool()->
                                  GetSequenceToken(),
                              base::SequencedWorkerPool::SKIP_ON_SHUTDOWN),
                      this,
                      false,
                      false) {
  app_pack_subscription_ = chromeos::CrosSettings::Get()->AddSettingsObserver(
      chromeos::kAppPack,
      base::Bind(&AppPackUpdater::AppPackChanged, base::Unretained(this)));

  if (install_attributes_->GetMode() == DEVICE_MODE_RETAIL_KIOSK) {
    // Already in Kiosk mode, start loading.
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(&AppPackUpdater::LoadPolicy,
                                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    // Linger until the device switches to DEVICE_MODE_RETAIL_KIOSK and the
    // app pack device setting appears.
  }
}

AppPackUpdater::~AppPackUpdater() {
}

extensions::ExternalLoader* AppPackUpdater::CreateExternalLoader() {
  if (created_extension_loader_) {
    NOTREACHED();
    return NULL;
  }
  created_extension_loader_ = true;
  AppPackExternalLoader* loader = new AppPackExternalLoader();
  extension_loader_ = loader->AsWeakPtr();

  // The cache may have been already checked. In that case, load the current
  // extensions into the loader immediately.
  UpdateExtensionLoader();

  return loader;
}

void AppPackUpdater::SetScreenSaverUpdateCallback(
    const AppPackUpdater::ScreenSaverUpdateCallback& callback) {
  screen_saver_update_callback_ = callback;
  if (!screen_saver_update_callback_.is_null() && !screen_saver_path_.empty()) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(screen_saver_update_callback_, screen_saver_path_));
  }
}

void AppPackUpdater::AppPackChanged() {
  if (install_attributes_->GetMode() == DEVICE_MODE_RETAIL_KIOSK)
    LoadPolicy();
}

void AppPackUpdater::LoadPolicy() {
  chromeos::CrosSettings* settings = chromeos::CrosSettings::Get();
  if (chromeos::CrosSettingsProvider::TRUSTED != settings->PrepareTrustedValues(
          base::Bind(&AppPackUpdater::LoadPolicy,
                     weak_ptr_factory_.GetWeakPtr()))) {
    return;
  }

  scoped_ptr<base::DictionaryValue> prefs(new base::DictionaryValue());
  const base::Value* value = settings->GetPref(chromeos::kAppPack);
  const base::ListValue* list = NULL;
  if (value && value->GetAsList(&list)) {
    for (base::ListValue::const_iterator it = list->begin();
         it != list->end(); ++it) {
      base::DictionaryValue* dict = NULL;
      if (!(*it)->GetAsDictionary(&dict)) {
        LOG(WARNING) << "AppPack entry is not a dictionary, ignoring.";
        continue;
      }
      std::string id;
      std::string update_url;
      if (dict->GetString(chromeos::kAppPackKeyExtensionId, &id) &&
          dict->GetString(chromeos::kAppPackKeyUpdateUrl, &update_url)) {
        base::DictionaryValue* entry = new base::DictionaryValue();
        entry->SetString(extensions::ExternalProviderImpl::kExternalUpdateUrl,
                         update_url);
        prefs->Set(id, entry);
      } else {
        LOG(WARNING) << "Failed to read required fields for an AppPack entry, "
                     << "ignoring.";
      }
    }
  }

  VLOG(1) << "Refreshed AppPack policy, got " << prefs->size()
          << " entries.";

  value = settings->GetPref(chromeos::kScreenSaverExtensionId);
  if (!value || !value->GetAsString(&screen_saver_id_)) {
    screen_saver_id_.clear();
    SetScreenSaverPath(base::FilePath());
  }

  external_cache_.UpdateExtensionsList(prefs.Pass());
}

void AppPackUpdater::OnExtensionListsUpdated(
    const base::DictionaryValue* prefs) {
  std::string crx_path;
  const base::DictionaryValue* screen_saver = NULL;
  if (prefs->GetDictionary(screen_saver_id_, &screen_saver)) {
    screen_saver->GetString(extensions::ExternalProviderImpl::kExternalCrx,
                            &crx_path);
  }
  if (!crx_path.empty())
    SetScreenSaverPath(base::FilePath(crx_path));
  else
    SetScreenSaverPath(base::FilePath());

  UpdateExtensionLoader();
}

void AppPackUpdater::UpdateExtensionLoader() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!extension_loader_) {
    VLOG(1) << "No AppPack loader created yet, not pushing extensions.";
    return;
  }

  scoped_ptr<base::DictionaryValue> prefs(
      external_cache_.cached_extensions()->DeepCopy());

  // The screensaver isn't installed into the Profile.
  prefs->Remove(screen_saver_id_, NULL);

  extension_loader_->SetCurrentAppPackExtensions(prefs.Pass());
}

void AppPackUpdater::OnDamagedFileDetected(const base::FilePath& path) {
  external_cache_.OnDamagedFileDetected(path);
}

void AppPackUpdater::SetScreenSaverPath(const base::FilePath& path) {
  // Don't invoke the callback if the path isn't changing.
  if (path != screen_saver_path_) {
    screen_saver_path_ = path;
    if (!screen_saver_update_callback_.is_null())
      screen_saver_update_callback_.Run(screen_saver_path_);
  }
}

}  // namespace policy
