// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_channel_id_helper.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "net/ssl/channel_id_service.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

using content::BrowserThread;

namespace {

class BrowsingDataChannelIDHelperImpl
    : public BrowsingDataChannelIDHelper {
 public:
  explicit BrowsingDataChannelIDHelperImpl(Profile* profile);

  // BrowsingDataChannelIDHelper methods.
  virtual void StartFetching(const FetchResultCallback& callback) OVERRIDE;
  virtual void DeleteChannelID(const std::string& server_id) OVERRIDE;

 private:
  virtual ~BrowsingDataChannelIDHelperImpl();

  // Fetch the certs. This must be called in the IO thread.
  void FetchOnIOThread();

  void OnFetchComplete(
      const net::ChannelIDStore::ChannelIDList& channel_id_list);

  // Notifies the completion callback. This must be called in the UI thread.
  void NotifyInUIThread(
      const net::ChannelIDStore::ChannelIDList& channel_id_list);

  // Delete a single cert. This must be called in IO thread.
  void DeleteOnIOThread(const std::string& server_id);

  // Called when deletion is done.
  void DeleteCallback();

  // Indicates whether or not we're currently fetching information:
  // it's true when StartFetching() is called in the UI thread, and it's reset
  // after we notify the callback in the UI thread.
  // This member is only mutated on the UI thread.
  bool is_fetching_;

  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;

  // This member is only mutated on the UI thread.
  FetchResultCallback completion_callback_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataChannelIDHelperImpl);
};

BrowsingDataChannelIDHelperImpl::
BrowsingDataChannelIDHelperImpl(Profile* profile)
    : is_fetching_(false),
      request_context_getter_(profile->GetRequestContext()) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

BrowsingDataChannelIDHelperImpl::
~BrowsingDataChannelIDHelperImpl() {
}

void BrowsingDataChannelIDHelperImpl::StartFetching(
    const FetchResultCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!is_fetching_);
  DCHECK(!callback.is_null());
  DCHECK(completion_callback_.is_null());
  is_fetching_ = true;
  completion_callback_ = callback;
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&BrowsingDataChannelIDHelperImpl::FetchOnIOThread, this));
}

void BrowsingDataChannelIDHelperImpl::DeleteChannelID(
    const std::string& server_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(
          &BrowsingDataChannelIDHelperImpl::DeleteOnIOThread, this, server_id));
}

void BrowsingDataChannelIDHelperImpl::FetchOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  net::ChannelIDStore* cert_store =
      request_context_getter_->GetURLRequestContext()->
      channel_id_service()->GetChannelIDStore();
  if (cert_store) {
    cert_store->GetAllChannelIDs(base::Bind(
        &BrowsingDataChannelIDHelperImpl::OnFetchComplete, this));
  } else {
    OnFetchComplete(net::ChannelIDStore::ChannelIDList());
  }
}

void BrowsingDataChannelIDHelperImpl::OnFetchComplete(
    const net::ChannelIDStore::ChannelIDList& channel_id_list) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&BrowsingDataChannelIDHelperImpl::NotifyInUIThread,
                 this,
                 channel_id_list));
}

void BrowsingDataChannelIDHelperImpl::NotifyInUIThread(
    const net::ChannelIDStore::ChannelIDList& channel_id_list) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(is_fetching_);
  is_fetching_ = false;
  completion_callback_.Run(channel_id_list);
  completion_callback_.Reset();
}

void BrowsingDataChannelIDHelperImpl::DeleteOnIOThread(
    const std::string& server_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  net::ChannelIDStore* cert_store =
      request_context_getter_->GetURLRequestContext()->
      channel_id_service()->GetChannelIDStore();
  if (cert_store) {
    cert_store->DeleteChannelID(
        server_id,
        base::Bind(&BrowsingDataChannelIDHelperImpl::DeleteCallback,
                   this));
  }
}

void BrowsingDataChannelIDHelperImpl::DeleteCallback() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Need to close open SSL connections which may be using the channel ids we
  // are deleting.
  // TODO(mattm): http://crbug.com/166069 Make the server bound cert
  // service/store have observers that can notify relevant things directly.
  request_context_getter_->GetURLRequestContext()->ssl_config_service()->
      NotifySSLConfigChange();
}

}  // namespace

// static
BrowsingDataChannelIDHelper*
BrowsingDataChannelIDHelper::Create(Profile* profile) {
  return new BrowsingDataChannelIDHelperImpl(profile);
}

CannedBrowsingDataChannelIDHelper::
CannedBrowsingDataChannelIDHelper() {}

CannedBrowsingDataChannelIDHelper::
~CannedBrowsingDataChannelIDHelper() {}

CannedBrowsingDataChannelIDHelper*
CannedBrowsingDataChannelIDHelper::Clone() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CannedBrowsingDataChannelIDHelper* clone =
      new CannedBrowsingDataChannelIDHelper();

  clone->channel_id_map_ = channel_id_map_;
  return clone;
}

void CannedBrowsingDataChannelIDHelper::AddChannelID(
    const net::ChannelIDStore::ChannelID& channel_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  channel_id_map_[channel_id.server_identifier()] =
      channel_id;
}

void CannedBrowsingDataChannelIDHelper::Reset() {
  channel_id_map_.clear();
}

bool CannedBrowsingDataChannelIDHelper::empty() const {
  return channel_id_map_.empty();
}

size_t CannedBrowsingDataChannelIDHelper::GetChannelIDCount() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return channel_id_map_.size();
}

void CannedBrowsingDataChannelIDHelper::StartFetching(
    const FetchResultCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (callback.is_null())
    return;
  // We post a task to emulate async fetching behavior.
  completion_callback_ = callback;
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&CannedBrowsingDataChannelIDHelper::FinishFetching,
                 this));
}

void CannedBrowsingDataChannelIDHelper::FinishFetching() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  net::ChannelIDStore::ChannelIDList channel_id_list;
  for (ChannelIDMap::iterator i = channel_id_map_.begin();
       i != channel_id_map_.end(); ++i)
    channel_id_list.push_back(i->second);
  completion_callback_.Run(channel_id_list);
}

void CannedBrowsingDataChannelIDHelper::DeleteChannelID(
    const std::string& server_id) {
  NOTREACHED();
}
