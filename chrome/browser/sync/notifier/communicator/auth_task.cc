// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/communicator/auth_task.h"

#include "chrome/browser/sync/notifier/gaia_auth/gaiaauth.h"
#include "chrome/browser/sync/notifier/communicator/login.h"
#include "chrome/browser/sync/notifier/communicator/login_settings.h"
#include "chrome/browser/sync/notifier/communicator/product_info.h"
#include "talk/base/common.h"
#include "talk/base/urlencode.h"
#include "talk/xmpp/xmppclient.h"

namespace notifier {
const char kTalkGadgetAuthPath[] = "/auth";

AuthTask::AuthTask(talk_base::Task* parent, Login* login, const char* url)
  : talk_base::Task(parent),
    login_(login),
    url_(url),
    use_gaia_redirect_(true) {
  ASSERT(login && !url_.empty());
}

int AuthTask::ProcessStart() {
  auth_.reset(new buzz::GaiaAuth(GetUserAgentString(),
                                 GetProductSignature()));
  auth_->SignalAuthDone.connect(this, &AuthTask::OnAuthDone);
  auth_->StartTokenAuth(login_->xmpp_client()->jid().BareJid(),
                        login_->login_settings().user_settings().pass(),
                        use_gaia_redirect_ ? "gaia" : service_);
  return STATE_RESPONSE;
}

int AuthTask::ProcessResponse() {
  ASSERT(auth_.get());
  if (!auth_->IsAuthDone()) {
    return STATE_BLOCKED;
  }
  if (!auth_->IsAuthorized()) {
    SignalAuthError(!auth_->HadError());
    return STATE_ERROR;
  }

  std::string uber_url;
  if (use_gaia_redirect_) {
    uber_url = auth_->CreateAuthenticatedUrl(url_, service_);
  } else {
    uber_url = redir_auth_prefix_ + auth_->GetAuthCookie();
    uber_url += redir_continue_;
    uber_url += UrlEncodeString(url_);
  }

  if (uber_url == "") {
    SignalAuthError(true);
    return STATE_ERROR;
  }

  SignalAuthDone(uber_url);
  return STATE_DONE;
}

void AuthTask::OnAuthDone() {
  Wake();
}

}  // namespace notifier
