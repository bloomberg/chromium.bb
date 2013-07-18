// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_server_bound_cert_helper.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "net/ssl/server_bound_cert_service.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

namespace {

class BrowsingDataServerBoundCertHelperImpl
    : public BrowsingDataServerBoundCertHelper {
 public:
  explicit BrowsingDataServerBoundCertHelperImpl(Profile* profile);

  // BrowsingDataServerBoundCertHelper methods.
  virtual void StartFetching(const FetchResultCallback& callback) OVERRIDE;
  virtual void DeleteServerBoundCert(const std::string& server_id) OVERRIDE;

 private:
  virtual ~BrowsingDataServerBoundCertHelperImpl();

  // Fetch the certs. This must be called in the IO thread.
  void FetchOnIOThread();

  void OnFetchComplete(
      const net::ServerBoundCertStore::ServerBoundCertList& cert_list);

  // Notifies the completion callback. This must be called in the UI thread.
  void NotifyInUIThread(
      const net::ServerBoundCertStore::ServerBoundCertList& cert_list);

  // Delete a single cert. This must be called in IO thread.
  void DeleteOnIOThread(const std::string& server_id);

  // Called when deletion is done.
  void DeleteCallback();

  // Indicates whether or not we're currently fetching information:
  // it's true when StartFetching() is called in the UI thread, and it's reset
  // after we notify the callback in the UI thread.
  // This only mutates on the UI thread.
  bool is_fetching_;

  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;

  // This only mutates on the UI thread.
  FetchResultCallback completion_callback_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataServerBoundCertHelperImpl);
};

BrowsingDataServerBoundCertHelperImpl::
BrowsingDataServerBoundCertHelperImpl(Profile* profile)
    : is_fetching_(false),
      request_context_getter_(profile->GetRequestContext()) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

BrowsingDataServerBoundCertHelperImpl::
~BrowsingDataServerBoundCertHelperImpl() {
}

void BrowsingDataServerBoundCertHelperImpl::StartFetching(
    const FetchResultCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!is_fetching_);
  DCHECK(!callback.is_null());
  DCHECK(completion_callback_.is_null());
  is_fetching_ = true;
  completion_callback_ = callback;
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&BrowsingDataServerBoundCertHelperImpl::FetchOnIOThread,
                 this));
}

void BrowsingDataServerBoundCertHelperImpl::DeleteServerBoundCert(
    const std::string& server_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&BrowsingDataServerBoundCertHelperImpl::DeleteOnIOThread,
                 this, server_id));
}

void BrowsingDataServerBoundCertHelperImpl::FetchOnIOThread() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  net::ServerBoundCertStore* cert_store =
      request_context_getter_->GetURLRequestContext()->
      server_bound_cert_service()->GetCertStore();
  if (cert_store) {
    cert_store->GetAllServerBoundCerts(base::Bind(
        &BrowsingDataServerBoundCertHelperImpl::OnFetchComplete, this));
  } else {
    OnFetchComplete(net::ServerBoundCertStore::ServerBoundCertList());
  }
}

void BrowsingDataServerBoundCertHelperImpl::OnFetchComplete(
    const net::ServerBoundCertStore::ServerBoundCertList& cert_list) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&BrowsingDataServerBoundCertHelperImpl::NotifyInUIThread,
                 this, cert_list));
}

void BrowsingDataServerBoundCertHelperImpl::NotifyInUIThread(
    const net::ServerBoundCertStore::ServerBoundCertList& cert_list) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(is_fetching_);
  is_fetching_ = false;
  completion_callback_.Run(cert_list);
  completion_callback_.Reset();
}

void BrowsingDataServerBoundCertHelperImpl::DeleteOnIOThread(
    const std::string& server_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  net::ServerBoundCertStore* cert_store =
      request_context_getter_->GetURLRequestContext()->
      server_bound_cert_service()->GetCertStore();
  if (cert_store) {
    cert_store->DeleteServerBoundCert(
        server_id,
        base::Bind(&BrowsingDataServerBoundCertHelperImpl::DeleteCallback,
                   this));
  }
}

void BrowsingDataServerBoundCertHelperImpl::DeleteCallback() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  // Need to close open SSL connections which may be using the channel ids we
  // are deleting.
  // TODO(mattm): http://crbug.com/166069 Make the server bound cert
  // service/store have observers that can notify relevant things directly.
  request_context_getter_->GetURLRequestContext()->ssl_config_service()->
      NotifySSLConfigChange();
}

}  // namespace

// static
BrowsingDataServerBoundCertHelper*
BrowsingDataServerBoundCertHelper::Create(Profile* profile) {
  return new BrowsingDataServerBoundCertHelperImpl(profile);
}

CannedBrowsingDataServerBoundCertHelper::
CannedBrowsingDataServerBoundCertHelper() {}

CannedBrowsingDataServerBoundCertHelper::
~CannedBrowsingDataServerBoundCertHelper() {}

CannedBrowsingDataServerBoundCertHelper*
CannedBrowsingDataServerBoundCertHelper::Clone() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  CannedBrowsingDataServerBoundCertHelper* clone =
      new CannedBrowsingDataServerBoundCertHelper();

  clone->server_bound_cert_map_ = server_bound_cert_map_;
  return clone;
}

void CannedBrowsingDataServerBoundCertHelper::AddServerBoundCert(
    const net::ServerBoundCertStore::ServerBoundCert& server_bound_cert) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  server_bound_cert_map_[server_bound_cert.server_identifier()] =
      server_bound_cert;
}

void CannedBrowsingDataServerBoundCertHelper::Reset() {
  server_bound_cert_map_.clear();
}

bool CannedBrowsingDataServerBoundCertHelper::empty() const {
  return server_bound_cert_map_.empty();
}

size_t CannedBrowsingDataServerBoundCertHelper::GetCertCount() const {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  return server_bound_cert_map_.size();
}

void CannedBrowsingDataServerBoundCertHelper::StartFetching(
    const FetchResultCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (callback.is_null())
    return;
  // We post a task to emulate async fetching behavior.
  completion_callback_ = callback;
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&CannedBrowsingDataServerBoundCertHelper::FinishFetching,
                 this));
}

void CannedBrowsingDataServerBoundCertHelper::FinishFetching() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  net::ServerBoundCertStore::ServerBoundCertList cert_list;
  for (ServerBoundCertMap::iterator i = server_bound_cert_map_.begin();
       i != server_bound_cert_map_.end(); ++i)
    cert_list.push_back(i->second);
  completion_callback_.Run(cert_list);
}

void CannedBrowsingDataServerBoundCertHelper::DeleteServerBoundCert(
    const std::string& server_id) {
  NOTREACHED();
}
