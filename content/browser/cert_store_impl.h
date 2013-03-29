// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CERT_STORE_IMPL_H_
#define CONTENT_BROWSER_CERT_STORE_IMPL_H_

#include <map>

#include "base/memory/singleton.h"
#include "base/synchronization/lock.h"
#include "content/public/browser/cert_store.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "net/cert/x509_certificate.h"

namespace content {

class CertStoreImpl : public CertStore,
                      public NotificationObserver {
 public:
  // Returns the singleton instance of the CertStore.
  static CertStoreImpl* GetInstance();

  // CertStore implementation:
  virtual int StoreCert(net::X509Certificate* cert,
                        int render_process_host_id) OVERRIDE;
  virtual bool RetrieveCert(int cert_id,
                            scoped_refptr<net::X509Certificate>* cert) OVERRIDE;

  // NotificationObserver implementation.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;
 protected:
  CertStoreImpl();
  virtual ~CertStoreImpl();

 private:
  friend struct DefaultSingletonTraits<CertStoreImpl>;

  void RegisterForNotification();

  // Remove the specified cert from id_to_cert_ and cert_to_id_.
  // NOTE: the caller (RemoveCertsForRenderProcesHost) must hold cert_lock_.
  void RemoveCertInternal(int cert_id);

  // Removes all the certs associated with the specified process from the store.
  void RemoveCertsForRenderProcesHost(int render_process_host_id);

  typedef std::multimap<int, int> IDMap;
  typedef std::map<int, scoped_refptr<net::X509Certificate> > CertMap;
  typedef std::map<net::X509Certificate*, int, net::X509Certificate::LessThan>
      ReverseCertMap;

  // Is only used on the UI Thread.
  NotificationRegistrar registrar_;

  IDMap process_id_to_cert_id_;
  IDMap cert_id_to_process_id_;

  CertMap id_to_cert_;
  ReverseCertMap cert_to_id_;

  int next_cert_id_;

  // This lock protects: process_to_ids_, id_to_processes_, id_to_cert_ and
  //                     cert_to_id_.
  base::Lock cert_lock_;

  DISALLOW_COPY_AND_ASSIGN(CertStoreImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_CERT_STORE_IMPL_H_
