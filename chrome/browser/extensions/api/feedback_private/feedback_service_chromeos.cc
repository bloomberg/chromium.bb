// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/feedback_private/feedback_service.h"

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/system_logs/scrubbed_system_logs_fetcher.h"

using extensions::api::feedback_private::SystemInformation;

namespace extensions {

class FeedbackServiceImpl
    : public FeedbackService,
      public base::SupportsWeakPtr<FeedbackServiceImpl> {
 public:
  FeedbackServiceImpl();
  virtual ~FeedbackServiceImpl();

  virtual std::string GetUserEmail() OVERRIDE;
  virtual void GetSystemInformation(
      const GetSystemInformationCallback& callback) OVERRIDE;

 private:
  void ProcessSystemLogs(scoped_ptr<chromeos::SystemLogsResponse> sys_info);

  // Overridden from FeedbackService:
  virtual base::WeakPtr<FeedbackService> GetWeakPtr() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(FeedbackServiceImpl);
};

FeedbackService* FeedbackService::CreateInstance() {
  return new FeedbackServiceImpl;
}

FeedbackServiceImpl::FeedbackServiceImpl() {
}

FeedbackServiceImpl::~FeedbackServiceImpl() {
}

std::string FeedbackServiceImpl::GetUserEmail() {
  chromeos::UserManager* manager = chromeos::UserManager::Get();
  if (!manager)
    return std::string();
  else
    return manager->GetLoggedInUser()->display_email();}

void FeedbackServiceImpl::GetSystemInformation(
    const GetSystemInformationCallback& callback) {
  system_information_callback_ = callback;

  chromeos::ScrubbedSystemLogsFetcher* fetcher =
      new chromeos::ScrubbedSystemLogsFetcher();
  fetcher->Fetch(base::Bind(&FeedbackServiceImpl::ProcessSystemLogs,
                            AsWeakPtr()));
}

void FeedbackServiceImpl::ProcessSystemLogs(
    scoped_ptr<chromeos::SystemLogsResponse> sys_info_map) {
  SystemInformationList sys_info_list;
  if (!sys_info_map.get()) {
    system_information_callback_.Run(sys_info_list);
    return;
  }

  for (chromeos::SystemLogsResponse::iterator it = sys_info_map->begin();
       it != sys_info_map->end(); ++it)
    FeedbackService::PopulateSystemInfo(&sys_info_list, it->first, it->second);

  system_information_callback_.Run(sys_info_list);
}

base::WeakPtr<FeedbackService> FeedbackServiceImpl::GetWeakPtr() {
  return AsWeakPtr();
}

}  // namespace extensions
