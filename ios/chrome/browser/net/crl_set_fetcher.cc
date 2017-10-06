// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/net/crl_set_fetcher.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task_scheduler/post_task.h"
#include "base/task_scheduler/task_traits.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "components/component_updater/component_updater_service.h"
#include "components/update_client/update_client.h"
#include "components/update_client/utils.h"
#include "ios/chrome/browser/chrome_constants.h"
#include "ios/web/public/web_thread.h"
#include "net/cert/crl_set.h"
#include "net/cert/crl_set_storage.h"
#include "net/ssl/ssl_config_service.h"

using component_updater::ComponentUpdateService;

CRLSetFetcher::CRLSetFetcher() : cus_(nullptr) {}

void CRLSetFetcher::SetCRLSetFilePath(const base::FilePath& path) {
  crl_path_ = path.Append(kIOSChromeCRLSetFilename);
}

base::FilePath CRLSetFetcher::GetCRLSetFilePath() const {
  return crl_path_;
}

void CRLSetFetcher::StartInitialLoad(ComponentUpdateService* cus,
                                     const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  if (path.empty())
    return;
  SetCRLSetFilePath(path);
  cus_ = cus;

  base::PostTaskWithTraits(
      FROM_HERE, {base::TaskPriority::BACKGROUND, base::MayBlock()},
      base::BindOnce(&CRLSetFetcher::DoInitialLoadFromDisk, this));
}

void CRLSetFetcher::DeleteFromDisk(const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);

  if (path.empty())
    return;
  SetCRLSetFilePath(path);
  base::PostTaskWithTraits(
      FROM_HERE, {base::TaskPriority::BACKGROUND, base::MayBlock()},
      base::BindOnce(&CRLSetFetcher::DoDeleteFromDisk, this));
}

void CRLSetFetcher::DoInitialLoadFromDisk() {
  base::ThreadRestrictions::AssertIOAllowed();

  LoadFromDisk(GetCRLSetFilePath(), &crl_set_);

  uint32_t sequence_of_loaded_crl = 0;
  if (crl_set_.get())
    sequence_of_loaded_crl = crl_set_->sequence();

  // Get updates, advertising the sequence number of the CRL set that we just
  // loaded, if any.
  if (!web::WebThread::GetTaskRunnerForThread(web::WebThread::UI)
           ->PostTask(FROM_HERE,
                      base::BindOnce(&CRLSetFetcher::RegisterComponent, this,
                                     sequence_of_loaded_crl))) {
    NOTREACHED();
  }
}

void CRLSetFetcher::LoadFromDisk(base::FilePath path,
                                 scoped_refptr<net::CRLSet>* out_crl_set) {
  TRACE_EVENT0("CRLSetFetcher", "LoadFromDisk");

  base::ThreadRestrictions::AssertIOAllowed();

  std::string crl_set_bytes;
  {
    TRACE_EVENT0("CRLSetFetcher", "ReadFileToString");
    if (!base::ReadFileToString(path, &crl_set_bytes))
      return;
  }

  if (!net::CRLSetStorage::Parse(crl_set_bytes, out_crl_set)) {
    LOG(WARNING) << "Failed to parse CRL set from " << path.MaybeAsASCII();
    return;
  }

  VLOG(1) << "Loaded " << crl_set_bytes.size() << " bytes of CRL set from disk";

  if (!web::WebThread::GetTaskRunnerForThread(web::WebThread::IO)
           ->PostTask(FROM_HERE,
                      base::BindOnce(&CRLSetFetcher::SetCRLSetIfNewer, this,
                                     *out_crl_set))) {
    NOTREACHED();
  }
}

void CRLSetFetcher::SetCRLSetIfNewer(scoped_refptr<net::CRLSet> crl_set) {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);

  scoped_refptr<net::CRLSet> old_crl_set(net::SSLConfigService::GetCRLSet());
  if (old_crl_set.get() && old_crl_set->sequence() > crl_set->sequence()) {
    LOG(WARNING) << "Refusing to downgrade CRL set from #"
                 << old_crl_set->sequence() << "to #" << crl_set->sequence();
  } else {
    net::SSLConfigService::SetCRLSet(crl_set);
    VLOG(1) << "Installed CRL set #" << crl_set->sequence();
  }
}

// kPublicKeySHA256 is the SHA256 hash of the SubjectPublicKeyInfo of the key
// that's used to sign generated CRL sets.
static const uint8_t kPublicKeySHA256[32] = {
    0x75, 0xda, 0xf8, 0xcb, 0x77, 0x68, 0x40, 0x33, 0x65, 0x4c, 0x97,
    0xe5, 0xc5, 0x1b, 0xcd, 0x81, 0x7b, 0x1e, 0xeb, 0x11, 0x2c, 0xe1,
    0xa4, 0x33, 0x8c, 0xf5, 0x72, 0x5e, 0xed, 0xb8, 0x43, 0x97,
};

void CRLSetFetcher::RegisterComponent(uint32_t sequence_of_loaded_crl) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);

  update_client::CrxComponent component;
  component.pk_hash.assign(kPublicKeySHA256,
                           kPublicKeySHA256 + sizeof(kPublicKeySHA256));
  component.installer = this;
  component.name = "CRLSet";
  component.version = base::Version(base::UintToString(sequence_of_loaded_crl));
  component.allows_background_download = false;
  component.requires_network_encryption = false;
  if (!component.version.IsValid()) {
    NOTREACHED();
    component.version = base::Version("0");
  }

  if (!cus_->RegisterComponent(component))
    NOTREACHED() << "RegisterComponent returned error";
}

void CRLSetFetcher::DoDeleteFromDisk() {
  base::ThreadRestrictions::AssertIOAllowed();

  DeleteFile(GetCRLSetFilePath(), false /* not recursive */);
}

void CRLSetFetcher::OnUpdateError(int error) {
  LOG(WARNING) << "CRLSetFetcher got error " << error
               << " from component installer";
}

void CRLSetFetcher::Install(std::unique_ptr<base::DictionaryValue> manifest,
                            const base::FilePath& unpack_path,
                            const Callback& callback) {
  const auto result = update_client::InstallFunctionWrapper(
      base::Bind(&CRLSetFetcher::DoInstall, base::Unretained(this),
                 base::ConstRef(*manifest), base::ConstRef(unpack_path)));
  base::DeleteFile(unpack_path, true /* recursive */);
  base::PostTask(FROM_HERE, base::BindOnce(callback, result));
}

bool CRLSetFetcher::DoInstall(const base::DictionaryValue& manifest,
                              const base::FilePath& unpack_path) {
  base::FilePath crl_set_file_path =
      unpack_path.Append(FILE_PATH_LITERAL("crl-set"));
  base::FilePath save_to = GetCRLSetFilePath();

  std::string crl_set_bytes;
  if (!base::ReadFileToString(crl_set_file_path, &crl_set_bytes)) {
    LOG(WARNING) << "Failed to find crl-set file inside CRX";
    return false;
  }

  bool is_delta;
  if (!net::CRLSetStorage::GetIsDeltaUpdate(crl_set_bytes, &is_delta)) {
    LOG(WARNING) << "GetIsDeltaUpdate failed on CRL set from update CRX";
    return false;
  }

  if (is_delta && !crl_set_.get()) {
    // The update server gave us a delta update, but we don't have any base
    // revision to apply it to.
    LOG(WARNING) << "Received unsolicited delta update.";
    return false;
  }

  if (!is_delta) {
    if (!net::CRLSetStorage::Parse(crl_set_bytes, &crl_set_)) {
      LOG(WARNING) << "Failed to parse CRL set from update CRX";
      return false;
    }
    int size = base::checked_cast<int>(crl_set_bytes.size());
    if (base::WriteFile(save_to, crl_set_bytes.data(), size) != size) {
      LOG(WARNING) << "Failed to save new CRL set to disk";
      // We don't return false here because we can still use this CRL set. When
      // we restart we might revert to an older version, then we'll
      // advertise the older version to Omaha and everything will still work.
    }
  } else {
    scoped_refptr<net::CRLSet> new_crl_set;
    if (!net::CRLSetStorage::ApplyDelta(crl_set_.get(), crl_set_bytes,
                                        &new_crl_set)) {
      LOG(WARNING) << "Failed to parse delta CRL set";
      return false;
    }
    VLOG(1) << "Applied CRL set delta #" << crl_set_->sequence() << "->#"
            << new_crl_set->sequence();
    const std::string new_crl_set_bytes =
        net::CRLSetStorage::Serialize(new_crl_set.get());
    int size = base::checked_cast<int>(new_crl_set_bytes.size());
    if (base::WriteFile(save_to, new_crl_set_bytes.data(), size) != size) {
      LOG(WARNING) << "Failed to save new CRL set to disk";
      // We don't return false here because we can still use this CRL set. When
      // we restart we might revert to an older version, then we'll
      // advertise the older version to Omaha and everything will still work.
    }
    crl_set_ = new_crl_set;
  }

  if (!web::WebThread::GetTaskRunnerForThread(web::WebThread::IO)
           ->PostTask(FROM_HERE,
                      base::BindOnce(&CRLSetFetcher::SetCRLSetIfNewer, this,
                                     crl_set_))) {
    NOTREACHED();
  }

  return true;
}

bool CRLSetFetcher::GetInstalledFile(const std::string& file,
                                     base::FilePath* installed_file) {
  return false;
}

bool CRLSetFetcher::Uninstall() {
  return false;
}

CRLSetFetcher::~CRLSetFetcher() {}
