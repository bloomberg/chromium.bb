// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/one_google_bar/one_google_bar_service.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/search/one_google_bar/one_google_bar_fetcher.h"
#include "components/signin/core/browser/signin_manager_base.h"

class OneGoogleBarService::SigninObserver : public SigninManagerBase::Observer {
 public:
  using SigninStatusChangedCallback = base::Closure;

  SigninObserver(SigninManagerBase* signin_manager,
                 const SigninStatusChangedCallback& callback)
      : signin_manager_(signin_manager), callback_(callback) {
    signin_manager_->AddObserver(this);
  }

  ~SigninObserver() override { signin_manager_->RemoveObserver(this); }

 private:
  // SigninManagerBase::Observer implementation.
  void GoogleSigninSucceeded(const std::string& account_id,
                             const std::string& username,
                             const std::string& password) override {
    callback_.Run();
  }

  void GoogleSignedOut(const std::string& account_id,
                       const std::string& username) override {
    callback_.Run();
  }

  SigninManagerBase* const signin_manager_;
  SigninStatusChangedCallback callback_;
};

OneGoogleBarService::OneGoogleBarService(
    SigninManagerBase* signin_manager,
    std::unique_ptr<OneGoogleBarFetcher> fetcher)
    : fetcher_(std::move(fetcher)),
      signin_observer_(base::MakeUnique<SigninObserver>(
          signin_manager,
          base::Bind(&OneGoogleBarService::SigninStatusChanged,
                     base::Unretained(this)))) {}

OneGoogleBarService::~OneGoogleBarService() = default;

void OneGoogleBarService::Shutdown() {
  for (auto& observer : observers_) {
    observer.OnOneGoogleBarServiceShuttingDown();
  }

  signin_observer_.reset();
}

void OneGoogleBarService::Refresh() {
  fetcher_->Fetch(base::BindOnce(&OneGoogleBarService::OneGoogleBarDataFetched,
                                 base::Unretained(this)));
}

void OneGoogleBarService::AddObserver(OneGoogleBarServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void OneGoogleBarService::RemoveObserver(
    OneGoogleBarServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

void OneGoogleBarService::SigninStatusChanged() {
  SetOneGoogleBarData(base::nullopt);
}

void OneGoogleBarService::OneGoogleBarDataFetched(
    const base::Optional<OneGoogleBarData>& data) {
  SetOneGoogleBarData(data);
  if (!data) {
    for (auto& observer : observers_) {
      observer.OnOneGoogleBarFetchFailed();
    }
  }
}

void OneGoogleBarService::SetOneGoogleBarData(
    const base::Optional<OneGoogleBarData>& data) {
  if (one_google_bar_data_ == data) {
    return;
  }

  one_google_bar_data_ = data;
  for (auto& observer : observers_) {
    observer.OnOneGoogleBarDataChanged();
  }
}
