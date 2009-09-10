// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/communicator/talk_auth_task.h"

#include "chrome/browser/sync/notifier/communicator/login.h"
#include "chrome/browser/sync/notifier/communicator/login_settings.h"
#include "chrome/browser/sync/notifier/communicator/product_info.h"
#include "chrome/browser/sync/notifier/gaia_auth/gaiaauth.h"
#include "talk/base/common.h"
#include "talk/base/urlencode.h"
#include "talk/xmpp/xmppclient.h"

namespace notifier {
const char kTalkGadgetAuthPath[] = "/auth";

TalkAuthTask::TalkAuthTask(talk_base::Task* parent,
                           Login* login,
                           const char* url)
  : talk_base::Task(parent),
    login_(login),
    url_(url) {
  ASSERT(login && !url_.empty());
}

int TalkAuthTask::ProcessStart() {
  auth_.reset(new buzz::GaiaAuth(GetUserAgentString(),
                                 GetProductSignature()));
  auth_->SignalAuthDone.connect(
      this,
      &TalkAuthTask::OnAuthDone);
  auth_->StartAuth(login_->xmpp_client()->jid().BareJid(),
                   login_->login_settings().user_settings().pass(),
                   "talk");
  return STATE_RESPONSE;
}

int TalkAuthTask::ProcessResponse() {
  ASSERT(auth_.get());
  if (!auth_->IsAuthDone()) {
    return STATE_BLOCKED;
  }
  SignalAuthDone(*this);
  return STATE_DONE;
}


void TalkAuthTask::OnAuthDone() {
  Wake();
}

bool TalkAuthTask::HadError() const {
  return auth_->HadError();
}

std::string TalkAuthTask::GetAuthenticatedUrl(
    const char* talk_base_url) const {
  ASSERT(talk_base_url && *talk_base_url && !auth_->HadError());

  std::string auth_url(talk_base_url);
  auth_url.append(kTalkGadgetAuthPath);
  auth_url.append("?silent=true&redirect=true&host=");
  auth_url.append(UrlEncodeString(url_));
  auth_url.append("&auth=");
  auth_url.append(auth_->GetAuth());
  return auth_url;
}

std::string TalkAuthTask::GetSID() const {
  return auth_->GetSID();
}
}  // namespace notifier
