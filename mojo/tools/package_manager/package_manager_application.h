// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PACKAGE_MANAGER_PACKAGE_MANAGER_APPLICATION_H_
#define MOJO_PACKAGE_MANAGER_PACKAGE_MANAGER_APPLICATION_H_

#include <map>
#include <set>

#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/interface_factory.h"
#include "mojo/public/cpp/system/macros.h"
#include "mojo/services/public/interfaces/network/network_service.mojom.h"
#include "mojo/tools/package_manager/package_fetcher.h"

namespace mojo {

class Manifest;

class PackageManagerApplication
    : public ApplicationDelegate,
      public PackageFetcherDelegate {
 public:
  PackageManagerApplication();
  virtual ~PackageManagerApplication();

 private:
  struct PendingLoad {
    PendingLoad();
    ~PendingLoad();

    scoped_ptr<PackageFetcher> fetcher;
  };
  typedef std::map<GURL, PendingLoad*> PendingLoadMap;

  void StartLoad(const GURL& url);

  void LoadDeps(const Manifest& manifest);

  // Deletes the pending load entry for the given URL and possibly exits the
  // message loop if all loads are done.
  void PendingLoadComplete(const GURL& url);

  // ApplicationDelegate implementation.
  virtual void Initialize(ApplicationImpl* app) MOJO_OVERRIDE;

  // PackageFetcher.
  virtual void FetchSucceeded(const GURL& url,
                              const base::FilePath& name) override;
  virtual void FetchFailed(const GURL& url) override;

  mojo::NetworkServicePtr network_service_;

  PendingLoadMap pending_;  // Owning pointers.
  std::set<GURL> completed_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(PackageManagerApplication);
};

}  // namespace mojo

#endif  // MOJO_PACKAGE_MANAGER_PACKAGE_MANAGER_APPLICATION_H
