// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/dom_distiller/core/dom_distiller_store.h"

namespace dom_distiller {

ViewerHandle::ViewerHandle() {}
ViewerHandle::~ViewerHandle() {}

DomDistillerService::DomDistillerService(
    scoped_ptr<DomDistillerStoreInterface> store,
    scoped_ptr<DistillerFactory> distiller_factory)
    : store_(store.Pass()), distiller_factory_(distiller_factory.Pass()) {}

DomDistillerService::~DomDistillerService() {}

syncer::SyncableService* DomDistillerService::GetSyncableService() const {
  return store_->GetSyncableService();
}

void DomDistillerService::AddToList(const GURL& url) { NOTIMPLEMENTED(); }

std::vector<ArticleEntry> DomDistillerService::GetEntries() const {
  return store_->GetEntries();
}

scoped_ptr<ViewerHandle> DomDistillerService::ViewEntry(
    ViewerContext* context,
    const std::string& entry_id) {
  NOTIMPLEMENTED();
  return scoped_ptr<ViewerHandle>();
}

scoped_ptr<ViewerHandle> DomDistillerService::ViewUrl(ViewerContext* context,
                                                      const GURL& url) {
  NOTIMPLEMENTED();
  return scoped_ptr<ViewerHandle>();
}

}  // namespace dom_distiller
