// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cert_store.h"

#include <algorithm>
#include <functional>

#include "base/stl_util.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"

template <typename T>
struct MatchSecond {
  explicit MatchSecond(const T& t) : value(t) {}

  template<typename Pair>
  bool operator()(const Pair& p) const {
    return (value == p.second);
  }
  T value;
};

//  static
CertStore* CertStore::GetInstance() {
  return Singleton<CertStore>::get();
}

CertStore::CertStore() : next_cert_id_(1) {
  // We watch for RenderProcess termination, as this is how we clear
  // certificates for now.
  // TODO(jcampan): we should be listening to events such as resource cached/
  //                removed from cache, and remove the cert when we know it
  //                is not used anymore.

  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                 content::NotificationService::AllBrowserContextsAndSources());
}

CertStore::~CertStore() {
}

int CertStore::StoreCert(net::X509Certificate* cert, int process_id) {
  DCHECK(cert);
  base::AutoLock auto_lock(cert_lock_);

  int cert_id;

  // Do we already know this cert?
  ReverseCertMap::iterator cert_iter = cert_to_id_.find(cert);
  if (cert_iter == cert_to_id_.end()) {
    cert_id = next_cert_id_++;
    // We use 0 as an invalid cert_id value.  In the unlikely event that
    // next_cert_id_ wraps around, we reset it to 1.
    if (next_cert_id_ == 0)
      next_cert_id_ = 1;
    cert->AddRef();
    id_to_cert_[cert_id] = cert;
    cert_to_id_[cert] = cert_id;
  } else {
    cert_id = cert_iter->second;
  }

  // Let's update process_id_to_cert_id_.
  std::pair<IDMap::iterator, IDMap::iterator> process_ids =
      process_id_to_cert_id_.equal_range(process_id);
  if (std::find_if(process_ids.first, process_ids.second,
                   MatchSecond<int>(cert_id)) == process_ids.second) {
    process_id_to_cert_id_.insert(std::make_pair(process_id, cert_id));
  }

  // And cert_id_to_process_id_.
  std::pair<IDMap::iterator, IDMap::iterator> cert_ids =
      cert_id_to_process_id_.equal_range(cert_id);
  if (std::find_if(cert_ids.first, cert_ids.second,
                   MatchSecond<int>(process_id)) == cert_ids.second) {
    cert_id_to_process_id_.insert(std::make_pair(cert_id, process_id));
  }

  return cert_id;
}

bool CertStore::RetrieveCert(int cert_id,
                             scoped_refptr<net::X509Certificate>* cert) {
  base::AutoLock auto_lock(cert_lock_);

  CertMap::iterator iter = id_to_cert_.find(cert_id);
  if (iter == id_to_cert_.end())
    return false;
  if (cert)
    *cert = iter->second;
  return true;
}

void CertStore::RemoveCertInternal(int cert_id) {
  CertMap::iterator cert_iter = id_to_cert_.find(cert_id);
  DCHECK(cert_iter != id_to_cert_.end());

  ReverseCertMap::iterator id_iter = cert_to_id_.find(cert_iter->second);
  DCHECK(id_iter != cert_to_id_.end());
  cert_to_id_.erase(id_iter);

  cert_iter->second->Release();
  id_to_cert_.erase(cert_iter);
}

void CertStore::RemoveCertsForRenderProcesHost(int process_id) {
  base::AutoLock auto_lock(cert_lock_);

  // We iterate through all the cert ids for that process.
  std::pair<IDMap::iterator, IDMap::iterator> process_ids =
      process_id_to_cert_id_.equal_range(process_id);
  for (IDMap::iterator ids_iter = process_ids.first;
       ids_iter != process_ids.second; ++ids_iter) {
    int cert_id = ids_iter->second;
    // Find all the processes referring to this cert id in
    // cert_id_to_process_id_, then locate the process being removed within
    // that range.
    std::pair<IDMap::iterator, IDMap::iterator> cert_ids =
        cert_id_to_process_id_.equal_range(cert_id);
    IDMap::iterator proc_iter =
        std::find_if(cert_ids.first, cert_ids.second,
                     MatchSecond<int>(process_id));
    DCHECK(proc_iter != cert_ids.second);

    // Before removing, determine if no other processes refer to the current
    // cert id. If |proc_iter| (the current process) is the lower bound of
    // processes containing the current cert id and if |next_proc_iter| is the
    // upper bound (the first process that does not), then only one process,
    // the one being removed, refers to the cert id.
    IDMap::iterator next_proc_iter = proc_iter;
    ++next_proc_iter;
    bool last_process_for_cert_id =
        (proc_iter == cert_ids.first && next_proc_iter == cert_ids.second);
    cert_id_to_process_id_.erase(proc_iter);

    if (last_process_for_cert_id) {
      // The current cert id is not referenced by any other processes, so
      // remove it from id_to_cert_ and cert_to_id_.
      RemoveCertInternal(cert_id);
    }
  }
  if (process_ids.first != process_ids.second)
    process_id_to_cert_id_.erase(process_ids.first, process_ids.second);
}

void CertStore::Observe(int type,
                        const content::NotificationSource& source,
                        const content::NotificationDetails& details) {
  DCHECK(type == content::NOTIFICATION_RENDERER_PROCESS_TERMINATED ||
         type == content::NOTIFICATION_RENDERER_PROCESS_CLOSED);
  content::RenderProcessHost* rph =
      content::Source<content::RenderProcessHost>(source).ptr();
  DCHECK(rph);
  RemoveCertsForRenderProcesHost(rph->GetID());
}
