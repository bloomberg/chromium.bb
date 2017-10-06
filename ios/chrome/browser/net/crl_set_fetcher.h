// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NET_CRL_SET_FETCHER_H_
#define IOS_CHROME_BROWSER_NET_CRL_SET_FETCHER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/update_client/update_client.h"

namespace base {
class DictionaryValue;
class FilePath;
}

namespace net {
class CRLSet;
}

namespace component_updater {
class ComponentUpdateService;
}

class CRLSetFetcher : public update_client::CrxInstaller {
 public:
  CRLSetFetcher();

  void StartInitialLoad(component_updater::ComponentUpdateService* cus,
                        const base::FilePath& path);

  // DeleteFromDisk asynchronously delete the CRLSet file.
  void DeleteFromDisk(const base::FilePath& path);

  // ComponentInstaller interface
  void OnUpdateError(int error) override;
  void Install(std::unique_ptr<base::DictionaryValue> manifest,
               const base::FilePath& unpack_path,
               const Callback& callback) override;
  bool GetInstalledFile(const std::string& file,
                        base::FilePath* installed_file) override;
  bool Uninstall() override;

 private:
  friend class base::RefCountedThreadSafe<CRLSetFetcher>;

  ~CRLSetFetcher() override;

  // GetCRLSetbase::FilePath gets the path of the CRL set file in the user data
  // dir.
  base::FilePath GetCRLSetFilePath() const;

  // Sets the path of the CRL set file in the user data dir.
  void SetCRLSetFilePath(const base::FilePath& path);

  // DoInitialLoadFromDisk runs on the FILE thread and attempts to load a CRL
  // set from the user-data dir. It then registers this object as a component
  // in order to get updates.
  void DoInitialLoadFromDisk();

  // LoadFromDisk runs on the FILE thread and attempts to load a CRL set
  // from |load_from|.
  void LoadFromDisk(base::FilePath load_from,
                    scoped_refptr<net::CRLSet>* out_crl_set);

  // SetCRLSetIfNewer runs on the IO thread and installs a CRL set
  // as the global CRL set if it's newer than the existing one.
  void SetCRLSetIfNewer(scoped_refptr<net::CRLSet> crl_set);

  // RegisterComponent registers this object as a component updater.
  void RegisterComponent(uint32_t sequence_of_loaded_crl);

  // DoDeleteFromDisk runs on the FILE thread and removes the CRLSet file from
  // the disk.
  void DoDeleteFromDisk();

  bool DoInstall(const base::DictionaryValue& manifest,
                 const base::FilePath& unpack_path);

  component_updater::ComponentUpdateService* cus_;

  // Path where the CRL file is stored.
  base::FilePath crl_path_;

  // We keep a pointer to the current CRLSet for use on the FILE thread when
  // applying delta updates.
  scoped_refptr<net::CRLSet> crl_set_;

  DISALLOW_COPY_AND_ASSIGN(CRLSetFetcher);
};

#endif  // IOS_CHROME_BROWSER_NET_CRL_SET_FETCHER_H_
