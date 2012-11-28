// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/oauth2_token_service_request.h"

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/oauth2_token_service.h"
#include "chrome/browser/signin/oauth2_token_service_factory.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"

class OAuth2TokenServiceRequest::Core
    : public base::RefCountedThreadSafe<OAuth2TokenServiceRequest::Core>,
      public OAuth2TokenService::Consumer {
 public:
  // Note the thread where an instance of Core is constructed is referred to as
  // the "owner thread" here. This will be the thread of |owner_task_runner_|.
  Core(Profile* profile,
       OAuth2TokenServiceRequest* owner);
  // Starts fetching an OAuth2 access token for |scopes|. It should be called
  // on the owner thread.
  void Start(const OAuth2TokenService::ScopeSet& scopes);
  // Stops the OAuth2 access token fetching. It should be called on the owner
  // thread.
  void Stop();

  // OAuth2TokenService::Consumer. It should be called on the UI thread.
  virtual void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                                 const std::string& access_token,
                                 const base::Time& expiration_time) OVERRIDE;
  virtual void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                                 const GoogleServiceAuthError& error) OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<OAuth2TokenServiceRequest::Core>;

  // Note this can be destructed on the owner thread or on the UI thread,
  // depending on the reference count.
  virtual ~Core();

  // Starts an OAuth2TokenService::Request on the UI thread.
  void StartOnUIThread(OAuth2TokenService::ScopeSet scopes);
  // Stops the OAuth2TokenService::Request on the UI thread.
  void StopOnUIThread();

  // Method posted to the owner thread to call back |owner_|. Note when this
  // posted task is actually running on the owner thread, it is possible that
  // |owner_| has been reset NULL.
  void InformOwnerOnGetTokenSuccess(std::string access_token,
                                    base::Time expiration_time);
  void InformOwnerOnGetTokenFailure(GoogleServiceAuthError error);

  // The profile with which this instance was initialized.
  Profile* const profile_;

  // The object to call back when fetching completes. |owner_| should be
  // called back only on the owner thread.
  OAuth2TokenServiceRequest* owner_;
  // Task runner on which |owner_| should be called back.
  scoped_refptr<base::SingleThreadTaskRunner> owner_task_runner_;

  // OAuth2TokenService request for fetching OAuth2 access token; it should be
  // created, reset and accessed only on the UI thread.
  scoped_ptr<OAuth2TokenService::Request> request_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

OAuth2TokenServiceRequest::Core::Core(
    Profile* profile,
    OAuth2TokenServiceRequest* owner)
    : profile_(profile),
      owner_(owner),
      owner_task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  DCHECK(profile);
  DCHECK(owner);
}

OAuth2TokenServiceRequest::Core::~Core() {
}

void OAuth2TokenServiceRequest::Core::Start(
    const OAuth2TokenService::ScopeSet& scopes) {
  DCHECK(owner_task_runner_->BelongsToCurrentThread());

  if (content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    StartOnUIThread(scopes);
  } else {
    content::BrowserThread::PostTask(
        content::BrowserThread::UI,
        FROM_HERE,
        base::Bind(&OAuth2TokenServiceRequest::Core::StartOnUIThread,
                   this, scopes));
  }
}

void OAuth2TokenServiceRequest::Core::Stop() {
  DCHECK(owner_task_runner_->BelongsToCurrentThread());

  // Detaches |owner_| from this instance so |owner_| will be called back only
  // if |Stop()| has never been called.
  owner_ = NULL;
  if (content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    StopOnUIThread();
  } else {
    content::BrowserThread::PostTask(
        content::BrowserThread::UI,
        FROM_HERE,
        base::Bind(&OAuth2TokenServiceRequest::Core::StopOnUIThread, this));
  }
}

void OAuth2TokenServiceRequest::Core::StopOnUIThread() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  request_.reset();
}

void OAuth2TokenServiceRequest::Core::StartOnUIThread(
    OAuth2TokenService::ScopeSet scopes) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  OAuth2TokenService* service = OAuth2TokenServiceFactory::GetForProfile(
      profile_);
  DCHECK(service);
  request_.reset(service->StartRequest(scopes, this).release());
}

void OAuth2TokenServiceRequest::Core::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const std::string& access_token,
    const base::Time& expiration_time) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK_EQ(request_.get(), request);
  owner_task_runner_->PostTask(FROM_HERE, base::Bind(
      &OAuth2TokenServiceRequest::Core::InformOwnerOnGetTokenSuccess, this,
      access_token, expiration_time));
  request_.reset();
}

void OAuth2TokenServiceRequest::Core::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK_EQ(request_.get(), request);
  owner_task_runner_->PostTask(FROM_HERE, base::Bind(
      &OAuth2TokenServiceRequest::Core::InformOwnerOnGetTokenFailure, this,
      error));
  request_.reset();
}

void OAuth2TokenServiceRequest::Core::InformOwnerOnGetTokenSuccess(
    std::string access_token,
    base::Time expiration_time) {
  DCHECK(owner_task_runner_->BelongsToCurrentThread());

  if (owner_)
    owner_->consumer_->OnGetTokenSuccess(owner_, access_token, expiration_time);
}

void OAuth2TokenServiceRequest::Core::InformOwnerOnGetTokenFailure(
    GoogleServiceAuthError error) {
  DCHECK(owner_task_runner_->BelongsToCurrentThread());

  if (owner_)
    owner_->consumer_->OnGetTokenFailure(owner_, error);
}

// static
OAuth2TokenServiceRequest* OAuth2TokenServiceRequest::CreateAndStart(
    Profile* profile,
    const OAuth2TokenService::ScopeSet& scopes,
    OAuth2TokenService::Consumer* consumer) {
  return new OAuth2TokenServiceRequest(profile, scopes, consumer);
}

OAuth2TokenServiceRequest::OAuth2TokenServiceRequest(
    Profile* profile,
    const OAuth2TokenService::ScopeSet& scopes,
    OAuth2TokenService::Consumer* consumer)
    : consumer_(consumer),
      core_(new Core(profile, this)) {
  core_->Start(scopes);
}

OAuth2TokenServiceRequest::~OAuth2TokenServiceRequest() {
  DCHECK(CalledOnValidThread());
  core_->Stop();
}
