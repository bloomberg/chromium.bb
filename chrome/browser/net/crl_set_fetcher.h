// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_CRL_SET_FETCHER_H_
#define CHROME_BROWSER_NET_CRL_SET_FETCHER_H_
#pragma once

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/task.h"
#include "base/time.h"
#include "chrome/browser/component_updater/component_updater_service.h"

class FilePath;

namespace base {
class DictionaryValue;
}

namespace net {
class CRLSet;
}

class ComponentUpdateService;

class CRLSetFetcher : public ComponentInstaller,
                      public base::RefCountedThreadSafe<CRLSetFetcher> {
 public:
  CRLSetFetcher();
  virtual ~CRLSetFetcher();

  void StartInitialLoad(ComponentUpdateService* cus);

  // ComponentInstaller interface
  virtual void OnUpdateError(int error) OVERRIDE;
  virtual bool Install(base::DictionaryValue* manifest,
                       const FilePath& unpack_path) OVERRIDE;

 private:
  // GetCRLSetFilePath gets the path of the CRL set file in the user data
  // dir.
  bool GetCRLSetFilePath(FilePath* path) const;

  // DoInitialLoadFromDisk runs on the FILE thread and attempts to load a CRL
  // set from the user-data dir. It then registers this object as a component
  // in order to get updates.
  void DoInitialLoadFromDisk();

  // LoadFromDisk runs on the FILE thread and attempts to load a CRL set
  // from |load_from|. If successful, it saves a copy of the CRLSet to
  // |save_to|, unless |save_to| is empty.
  void LoadFromDisk(FilePath load_from,
                    FilePath save_to,
                    scoped_refptr<net::CRLSet>* maybe_out_crl_set);

  // SetCRLSetIfNewer runs on the IO thread and installs a CRL set
  // as the global CRL set if it's newer than the existing one.
  void SetCRLSetIfNewer(scoped_refptr<net::CRLSet> crl_set);

  // RegisterComponent registers this object as a component updater.
  void RegisterComponent(uint32 sequence_of_loaded_crl);

  ComponentUpdateService* cus_;

  DISALLOW_COPY_AND_ASSIGN(CRLSetFetcher);
};

#endif  // CHROME_BROWSER_NET_CRL_SET_FETCHER_H_
