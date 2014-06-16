// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_CRL_SET_FETCHER_H_
#define CHROME_BROWSER_NET_CRL_SET_FETCHER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/component_updater/component_updater_service.h"

namespace base {
class DictionaryValue;
class FilePath;
}

namespace net {
class CRLSet;
}

class CRLSetFetcher : public component_updater::ComponentInstaller,
                      public base::RefCountedThreadSafe<CRLSetFetcher> {
 public:
  CRLSetFetcher();

  void StartInitialLoad(component_updater::ComponentUpdateService* cus);

  // DeleteFromDisk asynchronously delete the CRLSet file.
  void DeleteFromDisk();

  // ComponentInstaller interface
  virtual void OnUpdateError(int error) OVERRIDE;
  virtual bool Install(const base::DictionaryValue& manifest,
                       const base::FilePath& unpack_path) OVERRIDE;
  virtual bool GetInstalledFile(const std::string& file,
                                base::FilePath* installed_file) OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<CRLSetFetcher>;

  virtual ~CRLSetFetcher();

  // GetCRLSetbase::FilePath gets the path of the CRL set file in the user data
  // dir.
  bool GetCRLSetFilePath(base::FilePath* path) const;

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
  void RegisterComponent(uint32 sequence_of_loaded_crl);

  // DoDeleteFromDisk runs on the FILE thread and removes the CRLSet file from
  // the disk.
  void DoDeleteFromDisk();

  component_updater::ComponentUpdateService* cus_;

  // We keep a pointer to the current CRLSet for use on the FILE thread when
  // applying delta updates.
  scoped_refptr<net::CRLSet> crl_set_;

  DISALLOW_COPY_AND_ASSIGN(CRLSetFetcher);
};

#endif  // CHROME_BROWSER_NET_CRL_SET_FETCHER_H_
