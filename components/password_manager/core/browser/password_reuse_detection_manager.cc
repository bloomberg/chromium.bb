// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_reuse_detection_manager.h"

#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#if defined(SAFE_BROWSING_DB_LOCAL) || defined(SAFE_BROWSING_DB_REMOTE)
#include "components/safe_browsing/password_protection/password_protection_service.h"
#endif

namespace password_manager {

namespace {
constexpr size_t kMaxNumberOfCharactersToStore = 30;
}

PasswordReuseDetectionManager::PasswordReuseDetectionManager(
    PasswordManagerClient* client)
    : client_(client) {
  DCHECK(client_);
}

PasswordReuseDetectionManager::~PasswordReuseDetectionManager() {}

void PasswordReuseDetectionManager::DidNavigateMainFrame(
    const GURL& main_frame_url) {
  main_frame_url_ = main_frame_url;
  input_characters_.clear();
}

void PasswordReuseDetectionManager::OnKeyPressed(const base::string16& text) {
  input_characters_ += text;
  if (input_characters_.size() > kMaxNumberOfCharactersToStore) {
    input_characters_.erase(
        0, input_characters_.size() - kMaxNumberOfCharactersToStore);
  }

  PasswordStore* store = client_->GetPasswordStore();
  if (!store)
    return;
  store->CheckReuse(input_characters_, main_frame_url_.GetOrigin().spec(),
                    this);
}

void PasswordReuseDetectionManager::OnReuseFound(
    const base::string16& password,
    const std::string& legitimate_domain,
    int saved_passwords,
    int number_matches) {
  std::unique_ptr<BrowserSavePasswordProgressLogger> logger;
  if (password_manager_util::IsLoggingActive(client_)) {
    logger.reset(
        new BrowserSavePasswordProgressLogger(client_->GetLogManager()));
    logger->LogString(BrowserSavePasswordProgressLogger::STRING_REUSE_FOUND,
                      legitimate_domain);
  }

  metrics_util::LogPasswordReuse(
      password.size(), saved_passwords, number_matches,
      client_->GetPasswordManager()->IsPasswordFieldDetectedOnPage());
#if defined(SAFE_BROWSING_DB_LOCAL)
  // TODO(jialiul): After CSD whitelist being added to Android, we should gate
  // this by either SAFE_BROWSING_DB_LOCAL or SAFE_BROWSING_DB_REMOTE.
  client_->CheckProtectedPasswordEntry(legitimate_domain);
#endif
}

}  // namespace password_manager
