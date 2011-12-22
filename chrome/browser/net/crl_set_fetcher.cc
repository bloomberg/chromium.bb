// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/crl_set_fetcher.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/string_number_conversions.h"
#include "base/time.h"
#include "chrome/browser/component_updater/component_updater_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/crl_set.h"
#include "net/base/ssl_config_service.h"

using content::BrowserThread;

CRLSetFetcher::CRLSetFetcher() {
}

CRLSetFetcher::~CRLSetFetcher() {
}

bool CRLSetFetcher::GetCRLSetFilePath(FilePath* path) const {
  bool ok = PathService::Get(chrome::DIR_USER_DATA, path);
  if (!ok) {
    NOTREACHED();
    return false;
  }
  *path = path->Append(chrome::kCRLSetFilename);
  return true;
}

void CRLSetFetcher::StartInitialLoad(ComponentUpdateService* cus) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  cus_ = cus;

  if (!BrowserThread::PostTask(
          BrowserThread::FILE, FROM_HERE,
          base::Bind(&CRLSetFetcher::DoInitialLoadFromDisk, this))) {
    NOTREACHED();
  }
}

void CRLSetFetcher::DoInitialLoadFromDisk() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  FilePath crl_set_file_path;
  if (!GetCRLSetFilePath(&crl_set_file_path))
    return;

  scoped_refptr<net::CRLSet> crl_set;
  LoadFromDisk(crl_set_file_path,
               FilePath() /* don't copy the data anywhere */,
               &crl_set);

  uint32 sequence_of_loaded_crl = 0;
  if (crl_set.get())
    sequence_of_loaded_crl = crl_set->sequence();

  // Get updates, advertising the sequence number of the CRL set that we just
  // loaded, if any.
  if (!BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(
              &CRLSetFetcher::RegisterComponent,
              this,
              sequence_of_loaded_crl))) {
    NOTREACHED();
  }
}

void CRLSetFetcher::LoadFromDisk(FilePath path,
                                 FilePath save_to,
                                 scoped_refptr<net::CRLSet>* out_crl_set) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  std::string crl_set_bytes;
  if (!file_util::ReadFileToString(path, &crl_set_bytes))
    return;

  scoped_refptr<net::CRLSet> crl_set;
  if (!net::CRLSet::Parse(crl_set_bytes, &crl_set)) {
    LOG(WARNING) << "Failed to parse CRL set from " << path.MaybeAsASCII();
    return;
  }

  if (out_crl_set)
    *out_crl_set = crl_set;

  if (!save_to.empty())
    file_util::WriteFile(save_to, crl_set_bytes.data(), crl_set_bytes.size());

  VLOG(1) << "Loaded " << crl_set_bytes.size() << " bytes of CRL set from disk";

  if (!BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::Bind(
              &CRLSetFetcher::SetCRLSetIfNewer, this, crl_set))) {
    NOTREACHED();
  }
}

void CRLSetFetcher::SetCRLSetIfNewer(
    scoped_refptr<net::CRLSet> crl_set) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  scoped_refptr<net::CRLSet> old_crl_set(net::SSLConfigService::GetCRLSet());
  if (old_crl_set.get() && old_crl_set->sequence() > crl_set->sequence()) {
    LOG(WARNING) << "Refusing to downgrade CRL set from #"
                 << old_crl_set->sequence()
                 << "to #"
                 << crl_set->sequence();
  } else {
    net::SSLConfigService::SetCRLSet(crl_set);
    VLOG(1) << "Installed CRL set #" << crl_set->sequence();
  }
}

// kPublicKeySHA256 is the SHA256 hash of the SubjectPublicKeyInfo of the key
// that's used to sign generated CRL sets.
static const uint8 kPublicKeySHA256[32] = {
  0x75, 0xda, 0xf8, 0xcb, 0x77, 0x68, 0x40, 0x33,
  0x65, 0x4c, 0x97, 0xe5, 0xc5, 0x1b, 0xcd, 0x81,
  0x7b, 0x1e, 0xeb, 0x11, 0x2c, 0xe1, 0xa4, 0x33,
  0x8c, 0xf5, 0x72, 0x5e, 0xed, 0xb8, 0x43, 0x97,
};

void CRLSetFetcher::RegisterComponent(uint32 sequence_of_loaded_crl) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  CrxComponent component;
  component.pk_hash.assign(kPublicKeySHA256,
                           kPublicKeySHA256 + sizeof(kPublicKeySHA256));
  component.installer = this;
  component.name = "CRLSet";
  component.version = Version(base::UintToString(sequence_of_loaded_crl));
  if (!component.version.IsValid()) {
    NOTREACHED();
    component.version = Version("0");
  }

  if (cus_->RegisterComponent(component) !=
      ComponentUpdateService::kOk) {
    NOTREACHED() << "RegisterComponent returned error";
  }
}

void CRLSetFetcher::OnUpdateError(int error) {
  LOG(WARNING) << "CRLSetFetcher got error " << error
               << " from component installer";
}

bool CRLSetFetcher::Install(base::DictionaryValue* manifest,
                            const FilePath& unpack_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  FilePath crl_set_file_path = unpack_path.Append(FILE_PATH_LITERAL("crl-set"));
  FilePath save_to;
  if (!GetCRLSetFilePath(&save_to))
    return true;
  LoadFromDisk(crl_set_file_path, save_to, NULL);
  return true;
}
