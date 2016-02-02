// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/supervised_user_whitelist_installer.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/path_service.h"
#include "base/scoped_observer.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_util.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_info_cache_observer.h"
#include "chrome/browser/supervised_user/supervised_user_whitelist_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/default_component_installer.h"
#include "components/crx_file/id_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/safe_json/json_sanitizer.h"
#include "content/public/browser/browser_thread.h"

namespace component_updater {

namespace {

const char kSanitizedWhitelistExtension[] = ".json";

const char kWhitelistedContent[] = "whitelisted_content";
const char kSites[] = "sites";

const char kClients[] = "clients";
const char kName[] = "name";

// These are copies of extensions::manifest_keys::kName and kShortName. They
// are duplicated here because we mustn't depend on code from extensions/
// (since it's not built on Android).
const char kExtensionName[] = "name";
const char kExtensionShortName[] = "short_name";

base::string16 GetWhitelistTitle(const base::DictionaryValue& manifest) {
  base::string16 title;
  if (!manifest.GetString(kExtensionShortName, &title))
    manifest.GetString(kExtensionName, &title);
  return title;
}

base::FilePath GetRawWhitelistPath(const base::DictionaryValue& manifest,
                                   const base::FilePath& install_dir) {
  const base::DictionaryValue* whitelist_dict = nullptr;
  if (!manifest.GetDictionary(kWhitelistedContent, &whitelist_dict))
    return base::FilePath();

  base::FilePath::StringType whitelist_file;
  if (!whitelist_dict->GetString(kSites, &whitelist_file))
    return base::FilePath();

  return install_dir.Append(whitelist_file);
}

base::FilePath GetSanitizedWhitelistPath(const std::string& crx_id) {
  base::FilePath base_dir;
  PathService::Get(chrome::DIR_SUPERVISED_USER_INSTALLED_WHITELISTS, &base_dir);
  return base_dir.empty()
             ? base::FilePath()
             : base_dir.AppendASCII(crx_id + kSanitizedWhitelistExtension);
}

void RecordUncleanUninstall() {
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(
          &base::RecordAction,
          base::UserMetricsAction("ManagedUsers_Whitelist_UncleanUninstall")));
}

void OnWhitelistSanitizationError(const base::FilePath& whitelist,
                                  const std::string& error) {
  LOG(WARNING) << "Invalid whitelist " << whitelist.value() << ": " << error;
}

void OnWhitelistSanitizationResult(
    const std::string& crx_id,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const base::Closure& callback,
    const std::string& result) {
  const base::FilePath sanitized_whitelist_path =
      GetSanitizedWhitelistPath(crx_id);
  const base::FilePath install_directory = sanitized_whitelist_path.DirName();
  if (!base::DirectoryExists(install_directory)) {
    if (!base::CreateDirectory(install_directory)) {
      PLOG(ERROR) << "Could't create directory " << install_directory.value();
      return;
    }
  }

  const int size = result.size();
  if (base::WriteFile(sanitized_whitelist_path, result.data(), size) != size) {
    PLOG(ERROR) << "Couldn't write file " << sanitized_whitelist_path.value();
    return;
  }
  task_runner->PostTask(FROM_HERE, callback);
}

void CheckForSanitizedWhitelistOnTaskRunner(
    const std::string& crx_id,
    const base::FilePath& whitelist_path,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const base::Closure& callback) {
  if (base::PathExists(GetSanitizedWhitelistPath(crx_id))) {
    task_runner->PostTask(FROM_HERE, callback);
    return;
  }

  std::string unsafe_json;
  if (!base::ReadFileToString(whitelist_path, &unsafe_json)) {
    PLOG(ERROR) << "Couldn't read file " << whitelist_path.value();
    return;
  }

  safe_json::JsonSanitizer::Sanitize(
      unsafe_json,
      base::Bind(&OnWhitelistSanitizationResult, crx_id, task_runner, callback),
      base::Bind(&OnWhitelistSanitizationError, whitelist_path));
}

void RemoveUnregisteredWhitelistsOnTaskRunner(
    const std::set<std::string>& registered_whitelists) {
  base::FilePath base_dir;
  PathService::Get(DIR_SUPERVISED_USER_WHITELISTS, &base_dir);
  if (!base_dir.empty()) {
    base::FileEnumerator file_enumerator(base_dir, false,
                                         base::FileEnumerator::DIRECTORIES);
    for (base::FilePath path = file_enumerator.Next(); !path.value().empty();
         path = file_enumerator.Next()) {
      const std::string crx_id = path.BaseName().MaybeAsASCII();

      // Ignore folders that don't have valid CRX ID names. These folders are
      // not managed by the component installer, so do not try to remove them.
      if (!crx_file::id_util::IdIsValid(crx_id))
        continue;

      // Ignore folders that correspond to registered whitelists.
      if (registered_whitelists.count(crx_id) > 0)
        continue;

      RecordUncleanUninstall();

      if (!base::DeleteFile(path, true))
        DPLOG(ERROR) << "Couldn't delete " << path.value();
    }
  }

  PathService::Get(chrome::DIR_SUPERVISED_USER_INSTALLED_WHITELISTS, &base_dir);
  if (!base_dir.empty()) {
    base::FilePath pattern(FILE_PATH_LITERAL("*"));
    pattern = pattern.AppendASCII(kSanitizedWhitelistExtension);
    base::FileEnumerator file_enumerator(
        base_dir, false, base::FileEnumerator::FILES, pattern.value());
    for (base::FilePath path = file_enumerator.Next(); !path.value().empty();
         path = file_enumerator.Next()) {
      // Ignore files that don't have valid CRX ID names. These files are not
      // managed by the component installer, so do not try to remove them.
      const std::string filename = path.BaseName().MaybeAsASCII();
      DCHECK(base::EndsWith(filename, kSanitizedWhitelistExtension,
                            base::CompareCase::SENSITIVE));

      const std::string crx_id = filename.substr(
          filename.size() - strlen(kSanitizedWhitelistExtension));

      if (!crx_file::id_util::IdIsValid(crx_id))
        continue;

      // Ignore files that correspond to registered whitelists.
      if (registered_whitelists.count(crx_id) > 0)
        continue;

      RecordUncleanUninstall();

      if (!base::DeleteFile(path, true))
        DPLOG(ERROR) << "Couldn't delete " << path.value();
    }
  }
}

class SupervisedUserWhitelistComponentInstallerTraits
    : public ComponentInstallerTraits {
 public:
  using RawWhitelistReadyCallback =
      base::Callback<void(const base::string16&, const base::FilePath&)>;

  SupervisedUserWhitelistComponentInstallerTraits(
      const std::string& crx_id,
      const std::string& name,
      const RawWhitelistReadyCallback& callback)
      : crx_id_(crx_id), name_(name), callback_(callback) {}
  ~SupervisedUserWhitelistComponentInstallerTraits() override {}

 private:
  // ComponentInstallerTraits overrides:
  bool VerifyInstallation(const base::DictionaryValue& manifest,
                          const base::FilePath& install_dir) const override;
  bool CanAutoUpdate() const override;
  bool OnCustomInstall(const base::DictionaryValue& manifest,
                       const base::FilePath& install_dir) override;
  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      scoped_ptr<base::DictionaryValue> manifest) override;
  base::FilePath GetBaseDirectory() const override;
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetName() const override;

  std::string crx_id_;
  std::string name_;
  RawWhitelistReadyCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(SupervisedUserWhitelistComponentInstallerTraits);
};

bool SupervisedUserWhitelistComponentInstallerTraits::VerifyInstallation(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) const {
  // Check whether the whitelist exists at the path specified by the manifest.
  // This does not check whether the whitelist is wellformed.
  return base::PathExists(GetRawWhitelistPath(manifest, install_dir));
}

bool SupervisedUserWhitelistComponentInstallerTraits::CanAutoUpdate() const {
  return true;
}

bool SupervisedUserWhitelistComponentInstallerTraits::OnCustomInstall(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) {
  // Delete the existing sanitized whitelist.
  return base::DeleteFile(GetSanitizedWhitelistPath(crx_id_), false);
}

void SupervisedUserWhitelistComponentInstallerTraits::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    scoped_ptr<base::DictionaryValue> manifest) {
  // TODO(treib): Before getting the title, we should localize the manifest
  // using extension_l10n_util::LocalizeExtension, but that doesn't exist on
  // Android. crbug.com/558387
  callback_.Run(GetWhitelistTitle(*manifest),
                GetRawWhitelistPath(*manifest, install_dir));
}

base::FilePath
SupervisedUserWhitelistComponentInstallerTraits::GetBaseDirectory() const {
  base::FilePath whitelist_directory;
  PathService::Get(DIR_SUPERVISED_USER_WHITELISTS, &whitelist_directory);
  return whitelist_directory.AppendASCII(crx_id_);
}

void SupervisedUserWhitelistComponentInstallerTraits::GetHash(
    std::vector<uint8_t>* hash) const {
  *hash = SupervisedUserWhitelistInstaller::GetHashFromCrxId(crx_id_);
}

std::string SupervisedUserWhitelistComponentInstallerTraits::GetName() const {
  return name_;
}

class SupervisedUserWhitelistInstallerImpl
    : public SupervisedUserWhitelistInstaller,
      public ProfileInfoCacheObserver {
 public:
  SupervisedUserWhitelistInstallerImpl(ComponentUpdateService* cus,
                                       ProfileInfoCache* profile_info_cache,
                                       PrefService* local_state);
  ~SupervisedUserWhitelistInstallerImpl() override {}

 private:
  void RegisterComponent(const std::string& crx_id,
                         const std::string& name,
                         const base::Closure& callback);
  void RegisterNewComponent(const std::string& crx_id, const std::string& name);
  bool UnregisterWhitelistInternal(base::DictionaryValue* pref_dict,
                                   const std::string& client_id,
                                   const std::string& crx_id);

  void OnRawWhitelistReady(const std::string& crx_id,
                           const base::string16& title,
                           const base::FilePath& whitelist_path);
  void OnSanitizedWhitelistReady(const std::string& crx_id,
                                 const base::string16& title);

  // SupervisedUserWhitelistInstaller overrides:
  void RegisterComponents() override;
  void Subscribe(const WhitelistReadyCallback& callback) override;
  void RegisterWhitelist(const std::string& client_id,
                         const std::string& crx_id,
                         const std::string& name) override;
  void UnregisterWhitelist(const std::string& client_id,
                           const std::string& crx_id) override;

  // ProfileInfoCacheObserver overrides:
  void OnProfileWillBeRemoved(const base::FilePath& profile_path) override;

  ComponentUpdateService* cus_;
  PrefService* local_state_;

  std::vector<WhitelistReadyCallback> callbacks_;

  ScopedObserver<ProfileInfoCache, ProfileInfoCacheObserver> observer_;

  base::WeakPtrFactory<SupervisedUserWhitelistInstallerImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SupervisedUserWhitelistInstallerImpl);
};

SupervisedUserWhitelistInstallerImpl::SupervisedUserWhitelistInstallerImpl(
    ComponentUpdateService* cus,
    ProfileInfoCache* profile_info_cache,
    PrefService* local_state)
    : cus_(cus),
      local_state_(local_state),
      observer_(this),
      weak_ptr_factory_(this) {
  DCHECK(cus);
  DCHECK(local_state);
  // In unit tests, the profile info cache can be null.
  if (profile_info_cache)
    observer_.Add(profile_info_cache);
}

void SupervisedUserWhitelistInstallerImpl::RegisterComponent(
    const std::string& crx_id,
    const std::string& name,
    const base::Closure& callback) {
  scoped_ptr<ComponentInstallerTraits> traits(
      new SupervisedUserWhitelistComponentInstallerTraits(
          crx_id, name,
          base::Bind(&SupervisedUserWhitelistInstallerImpl::OnRawWhitelistReady,
                     weak_ptr_factory_.GetWeakPtr(), crx_id)));
  scoped_refptr<DefaultComponentInstaller> installer(
      new DefaultComponentInstaller(std::move(traits)));
  installer->Register(cus_, callback);
}

void SupervisedUserWhitelistInstallerImpl::RegisterNewComponent(
    const std::string& crx_id,
    const std::string& name) {
  RegisterComponent(
      crx_id, name,
      base::Bind(&SupervisedUserWhitelistInstaller::TriggerComponentUpdate,
                 &cus_->GetOnDemandUpdater(), crx_id));
}

bool SupervisedUserWhitelistInstallerImpl::UnregisterWhitelistInternal(
    base::DictionaryValue* pref_dict,
    const std::string& client_id,
    const std::string& crx_id) {
  base::DictionaryValue* whitelist_dict = nullptr;
  bool success =
      pref_dict->GetDictionaryWithoutPathExpansion(crx_id, &whitelist_dict);
  DCHECK(success);
  base::ListValue* clients = nullptr;
  success = whitelist_dict->GetList(kClients, &clients);

  const bool removed = clients->Remove(base::StringValue(client_id), nullptr);

  if (!clients->empty())
    return removed;

  pref_dict->RemoveWithoutPathExpansion(crx_id, nullptr);
  bool result = cus_->UnregisterComponent(crx_id);
  DCHECK(result);

  result = base::DeleteFile(GetSanitizedWhitelistPath(crx_id), false);
  DCHECK(result);

  return removed;
}

void SupervisedUserWhitelistInstallerImpl::OnRawWhitelistReady(
    const std::string& crx_id,
    const base::string16& title,
    const base::FilePath& whitelist_path) {
  cus_->GetSequencedTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(
          &CheckForSanitizedWhitelistOnTaskRunner, crx_id, whitelist_path,
          base::ThreadTaskRunnerHandle::Get(),
          base::Bind(
              &SupervisedUserWhitelistInstallerImpl::OnSanitizedWhitelistReady,
              weak_ptr_factory_.GetWeakPtr(), crx_id, title)));
}

void SupervisedUserWhitelistInstallerImpl::OnSanitizedWhitelistReady(
    const std::string& crx_id,
    const base::string16& title) {
  for (const WhitelistReadyCallback& callback : callbacks_)
    callback.Run(crx_id, title, GetSanitizedWhitelistPath(crx_id));
}

void SupervisedUserWhitelistInstallerImpl::RegisterComponents() {
  const std::map<std::string, std::string> command_line_whitelists =
      SupervisedUserWhitelistService::GetWhitelistsFromCommandLine();

  std::set<std::string> registered_whitelists;
  std::set<std::string> stale_whitelists;
  DictionaryPrefUpdate update(local_state_,
                              prefs::kRegisteredSupervisedUserWhitelists);
  base::DictionaryValue* whitelists = update.Get();
  for (base::DictionaryValue::Iterator it(*whitelists); !it.IsAtEnd();
       it.Advance()) {
    const base::DictionaryValue* dict = nullptr;
    it.value().GetAsDictionary(&dict);

    const std::string& id = it.key();

    // Skip whitelists with no clients. This can happen when a whitelist was
    // previously registered on the command line but isn't anymore.
    const base::ListValue* clients = nullptr;
    if ((!dict->GetList(kClients, &clients) || clients->empty()) &&
        command_line_whitelists.count(id) == 0) {
      stale_whitelists.insert(id);
      continue;
    }

    std::string name;
    bool result = dict->GetString(kName, &name);
    DCHECK(result);
    RegisterComponent(id, name, base::Closure());

    registered_whitelists.insert(id);
  }

  // Clean up stale whitelists as determined above.
  for (const std::string& id : stale_whitelists)
    whitelists->RemoveWithoutPathExpansion(id, nullptr);

  cus_->GetSequencedTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&RemoveUnregisteredWhitelistsOnTaskRunner,
                            registered_whitelists));
}

void SupervisedUserWhitelistInstallerImpl::Subscribe(
    const WhitelistReadyCallback& callback) {
  return callbacks_.push_back(callback);
}

void SupervisedUserWhitelistInstallerImpl::RegisterWhitelist(
    const std::string& client_id,
    const std::string& crx_id,
    const std::string& name) {
  DictionaryPrefUpdate update(local_state_,
                              prefs::kRegisteredSupervisedUserWhitelists);
  base::DictionaryValue* pref_dict = update.Get();
  base::DictionaryValue* whitelist_dict = nullptr;
  const bool newly_added =
      !pref_dict->GetDictionaryWithoutPathExpansion(crx_id, &whitelist_dict);
  if (newly_added) {
    whitelist_dict = new base::DictionaryValue;
    whitelist_dict->SetString(kName, name);
    pref_dict->SetWithoutPathExpansion(crx_id, whitelist_dict);
  }

  if (!client_id.empty()) {
    base::ListValue* clients = nullptr;
    if (!whitelist_dict->GetList(kClients, &clients)) {
      DCHECK(newly_added);
      clients = new base::ListValue;
      whitelist_dict->Set(kClients, clients);
    }
    bool success =
        clients->AppendIfNotPresent(new base::StringValue(client_id));
    DCHECK(success);
  }

  if (!newly_added) {
    // Sanity-check that the stored name is equal to the name passed in.
    // In release builds this is a no-op.
    std::string stored_name;
    DCHECK(whitelist_dict->GetString(kName, &stored_name));
    DCHECK_EQ(stored_name, name);
    return;
  }

  RegisterNewComponent(crx_id, name);
}

void SupervisedUserWhitelistInstallerImpl::UnregisterWhitelist(
    const std::string& client_id,
    const std::string& crx_id) {
  DictionaryPrefUpdate update(local_state_,
                              prefs::kRegisteredSupervisedUserWhitelists);
  bool removed = UnregisterWhitelistInternal(update.Get(), client_id, crx_id);
  DCHECK(removed);
}

void SupervisedUserWhitelistInstallerImpl::OnProfileWillBeRemoved(
    const base::FilePath& profile_path) {
  std::string client_id = ClientIdForProfilePath(profile_path);

  // Go through all registered whitelists and possibly unregister them for this
  // client.
  DictionaryPrefUpdate update(local_state_,
                              prefs::kRegisteredSupervisedUserWhitelists);
  base::DictionaryValue* pref_dict = update.Get();
  for (base::DictionaryValue::Iterator it(*pref_dict); !it.IsAtEnd();
       it.Advance()) {
    UnregisterWhitelistInternal(pref_dict, client_id, it.key());
  }
}

}  // namespace

// static
scoped_ptr<SupervisedUserWhitelistInstaller>
SupervisedUserWhitelistInstaller::Create(ComponentUpdateService* cus,
                                         ProfileInfoCache* profile_info_cache,
                                         PrefService* local_state) {
  return make_scoped_ptr(new SupervisedUserWhitelistInstallerImpl(
      cus, profile_info_cache, local_state));
}

// static
void SupervisedUserWhitelistInstaller::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kRegisteredSupervisedUserWhitelists);
}

// static
std::string SupervisedUserWhitelistInstaller::ClientIdForProfilePath(
    const base::FilePath& profile_path) {
  // See ProfileInfoCache::CacheKeyFromProfilePath().
  return profile_path.BaseName().MaybeAsASCII();
}

// static
std::vector<uint8_t> SupervisedUserWhitelistInstaller::GetHashFromCrxId(
    const std::string& crx_id) {
  DCHECK(crx_file::id_util::IdIsValid(crx_id));

  std::vector<uint8_t> hash;
  uint8_t byte = 0;
  for (size_t i = 0; i < crx_id.size(); ++i) {
    // Uppercase characters in IDs are technically legal.
    int val = base::ToLowerASCII(crx_id[i]) - 'a';
    DCHECK_GE(val, 0);
    DCHECK_LT(val, 16);
    if (i % 2 == 0) {
      byte = val;
    } else {
      hash.push_back(16 * byte + val);
      byte = 0;
    }
  }
  return hash;
}

// static
void SupervisedUserWhitelistInstaller::TriggerComponentUpdate(
    OnDemandUpdater* updater,
    const std::string& crx_id) {
  const bool result = updater->OnDemandUpdate(crx_id);
  DCHECK(result);
}

}  // namespace component_updater
