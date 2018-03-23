// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/assistant_manager_service_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "base/task_scheduler/post_task.h"
#include "chromeos/assistant/internal/internal_constants.h"
#include "chromeos/assistant/internal/internal_util.h"
#include "chromeos/services/assistant/service.h"
#include "chromeos/system/version_loader.h"
#include "libassistant/shared/internal_api/assistant_manager_internal.h"
#include "url/gurl.h"

namespace chromeos {
namespace assistant {

AssistantManagerServiceImpl::AssistantManagerServiceImpl(
    mojom::AudioInputPtr audio_input)
    : platform_api_(kDefaultConfigStr, std::move(audio_input)),
      action_module_(std::make_unique<action::CrosActionModule>(this)),
      assistant_manager_(
          assistant_client::AssistantManager::Create(&platform_api_,
                                                     kDefaultConfigStr)),
      assistant_manager_internal_(
          UnwrapAssistantManagerInternal(assistant_manager_.get())),
      display_connection_(std::make_unique<CrosDisplayConnection>(this)) {}

AssistantManagerServiceImpl::~AssistantManagerServiceImpl() {}

void AssistantManagerServiceImpl::Start(const std::string& access_token) {
  // Posting to background thread because GetARCVersion may be blocking
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&chromeos::version_loader::GetARCVersion),
      base::BindOnce(&AssistantManagerServiceImpl::StartAssistantInternal,
                     base::Unretained(this), access_token));

  // Set the flag to avoid starting the service multiple times.
  running_ = true;
}

void AssistantManagerServiceImpl::StartAssistantInternal(
    const std::string& access_token,
    const std::string& arc_version) {
  auto* internal_options =
      assistant_manager_internal_->CreateDefaultInternalOptions();
  SetAssistantOptions(internal_options, BuildUserAgent(arc_version));
  assistant_manager_internal_->SetOptions(*internal_options, [](bool success) {
    DVLOG(2) << "set options: " << success;
  });

  assistant_manager_internal_->SetDisplayConnection(display_connection_.get());
  assistant_manager_internal_->RegisterActionModule(action_module_.get());

  SetAccessToken(access_token);
  assistant_manager_->Start();
}

std::string AssistantManagerServiceImpl::BuildUserAgent(
    const std::string& arc_version) const {
  int32_t os_major_version = 0;
  int32_t os_minor_version = 0;
  int32_t os_bugfix_version = 0;
  base::SysInfo::OperatingSystemVersionNumbers(
      &os_major_version, &os_minor_version, &os_bugfix_version);

  std::string user_agent;
  base::StringAppendF(&user_agent, "Mozilla/5.0 (X11; CrOS %s %d.%d.%d)",
                      base::SysInfo::OperatingSystemArchitecture().c_str(),
                      os_major_version, os_minor_version, os_bugfix_version);

  if (!arc_version.empty()) {
    base::StringAppendF(&user_agent, " ARC/%s", arc_version.c_str());
  }
  return user_agent;
}

bool AssistantManagerServiceImpl::IsRunning() const {
  return running_;
}

void AssistantManagerServiceImpl::SetAccessToken(
    const std::string& access_token) {
  // Push the |access_token| we got as an argument into AssistantManager before
  // starting to ensure that all server requests will be authenticated once
  // it is started. |user_id| is used to pair a user to their |access_token|,
  // since we do not support multi-user in this example we can set it to a
  // dummy value like "0".
  assistant_manager_->SetAuthTokens({std::pair<std::string, std::string>(
      /* user_id: */ "0", access_token)});
}

void AssistantManagerServiceImpl::EnableListening(bool enable) {
  assistant_manager_->EnableListening(enable);
}

void AssistantManagerServiceImpl::SendTextQuery(const std::string& query) {
  assistant_manager_internal_->SendTextQuery(query);
}

void AssistantManagerServiceImpl::AddAssistantEventSubscriber(
    mojom::AssistantEventSubscriberPtr subscriber) {
  subscribers_.AddPtr(std::move(subscriber));
}

void AssistantManagerServiceImpl::OnShowHtml(const std::string& html) {
  subscribers_.ForAllPtrs([&html](auto* ptr) { ptr->OnHtmlResponse(html); });
}

void AssistantManagerServiceImpl::OnShowText(const std::string& text) {
  subscribers_.ForAllPtrs([&text](auto* ptr) { ptr->OnTextResponse(text); });
}

void AssistantManagerServiceImpl::OnOpenUrl(const std::string& url) {
  subscribers_.ForAllPtrs(
      [&url](auto* ptr) { ptr->OnOpenUrlResponse(GURL(url)); });
}

void AssistantManagerServiceImpl::OnSpeechLevelUpdated(
    const float speech_level) {}

}  // namespace assistant
}  // namespace chromeos
