// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/extensions/fake_arc_support.h"

#include <string>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/chromeos/arc/extensions/arc_support_message_host.h"

namespace {

void SerializeAndSend(extensions::NativeMessageHost* native_message_host,
                      const base::DictionaryValue& message) {
  DCHECK(native_message_host);
  std::string message_string;
  if (!base::JSONWriter::Write(message, &message_string)) {
    NOTREACHED();
    return;
  }
  native_message_host->OnMessage(message_string);
}

// Posts onAgree with the three given bools as arguments.
void PostOnAgree(extensions::NativeMessageHost* native_message_host,
                 bool metrics_mode,
                 bool backup_and_restore_mode,
                 bool location_service_mode) {
  DCHECK(native_message_host);

  base::DictionaryValue message;
  message.SetString("event", "onAgreed");
  message.SetBoolean("isMetricsEnabled", metrics_mode);
  message.SetBoolean("isBackupRestoreEnabled", backup_and_restore_mode);
  message.SetBoolean("isLocationServiceEnabled", location_service_mode);
  SerializeAndSend(native_message_host, message);
}

}  // namespace

namespace arc {

FakeArcSupport::FakeArcSupport(ArcSupportHost* support_host)
    : support_host_(support_host), weak_ptr_factory_(this) {
  DCHECK(support_host_);
  support_host_->SetRequestOpenAppCallbackForTesting(
      base::Bind(&FakeArcSupport::Open, weak_ptr_factory_.GetWeakPtr()));
}

FakeArcSupport::~FakeArcSupport() {
  // Ensure that message host is disconnected.
  if (!native_message_host_)
    return;
  UnsetMessageHost();
}

void FakeArcSupport::Open(Profile* profile) {
  DCHECK(!native_message_host_);
  native_message_host_ = ArcSupportMessageHost::Create();
  native_message_host_->Start(this);
  support_host_->SetMessageHost(
      static_cast<ArcSupportMessageHost*>(native_message_host_.get()));
}

void FakeArcSupport::Close() {
  DCHECK(native_message_host_);
  native_message_host_->OnMessage("{\"event\": \"onWindowClosed\"}");
  UnsetMessageHost();
}

void FakeArcSupport::EmulateAuthCodeResponse(const std::string& auth_code) {
  DCHECK_EQ(ui_page_, ArcSupportHost::UIPage::LSO);
  base::DictionaryValue message;
  message.SetString("event", "onAuthSucceeded");
  message.SetString("code", auth_code);
  SerializeAndSend(native_message_host_.get(), message);
}

void FakeArcSupport::EmulateAuthFailure() {
  DCHECK(native_message_host_);
  DCHECK_EQ(ui_page_, ArcSupportHost::UIPage::LSO);
  native_message_host_->OnMessage("{\"event\": \"onAuthFailed\"}");
}

void FakeArcSupport::ClickAgreeButton() {
  DCHECK_EQ(ui_page_, ArcSupportHost::UIPage::TERMS);
  PostOnAgree(native_message_host_.get(), metrics_mode_,
              backup_and_restore_mode_, location_service_mode_);
}

void FakeArcSupport::ClickAdAuthNextButton() {
  DCHECK_EQ(ui_page_, ArcSupportHost::UIPage::AD_AUTH_NOTIFICATION);
  PostOnAgree(native_message_host_.get(), false, false, false);
}

void FakeArcSupport::ClickRetryButton() {
  DCHECK(native_message_host_);
  DCHECK_EQ(ui_page_, ArcSupportHost::UIPage::ERROR);
  native_message_host_->OnMessage("{\"event\": \"onRetryClicked\"}");
}

void FakeArcSupport::ClickSendFeedbackButton() {
  DCHECK(native_message_host_);
  DCHECK_EQ(ui_page_, ArcSupportHost::UIPage::ERROR);
  native_message_host_->OnMessage("{\"event\": \"onSendFeedbackClicked\"}");
}

void FakeArcSupport::UnsetMessageHost() {
  support_host_->UnsetMessageHost(
      static_cast<ArcSupportMessageHost*>(native_message_host_.get()));
  native_message_host_.reset();
}

void FakeArcSupport::PostMessageFromNativeHost(
    const std::string& message_string) {
  std::unique_ptr<base::DictionaryValue> message =
      base::DictionaryValue::From(base::JSONReader::Read(message_string));
  DCHECK(message);

  std::string action;
  if (!message->GetString("action", &action)) {
    NOTREACHED() << message_string;
    return;
  }

  if (action == "initialize") {
    // Do nothing as emulation.
  } else if (action == "showPage") {
    std::string page;
    if (!message->GetString("page", &page)) {
      NOTREACHED() << message_string;
      return;
    }
    if (page == "terms") {
      ui_page_ = ArcSupportHost::UIPage::TERMS;
    } else if (page == "lso-loading") {
      ui_page_ = ArcSupportHost::UIPage::LSO;
    } else if (page == "arc-loading") {
      ui_page_ = ArcSupportHost::UIPage::ARC_LOADING;
    } else if (page == "ad-auth-notification") {
      ui_page_ = ArcSupportHost::UIPage::AD_AUTH_NOTIFICATION;
    } else {
      NOTREACHED() << message_string;
    }
  } else if (action == "showErrorPage") {
    ui_page_ = ArcSupportHost::UIPage::ERROR;
  } else if (action == "setMetricsMode") {
    if (!message->GetBoolean("enabled", &metrics_mode_)) {
      NOTREACHED() << message_string;
      return;
    }
  } else if (action == "setBackupAndRestoreMode") {
    if (!message->GetBoolean("enabled", &backup_and_restore_mode_)) {
      NOTREACHED() << message_string;
      return;
    }
  } else if (action == "setLocationServiceMode") {
    if (!message->GetBoolean("enabled", &location_service_mode_)) {
      NOTREACHED() << message_string;
      return;
    }
  } else {
    // Unknown or unsupported action.
    NOTREACHED() << message_string;
  }
}

void FakeArcSupport::CloseChannel(const std::string& error_message) {
  NOTREACHED();
}

}  // namespace arc
