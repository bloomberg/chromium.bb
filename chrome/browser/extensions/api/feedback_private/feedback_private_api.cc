// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/feedback_private/feedback_private_api.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/statistics_recorder.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/feedback_private/feedback_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/common/extensions/api/feedback_private.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/feedback/tracing_manager.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/strings/grit/components_strings.h"
#include "extensions/browser/event_router.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "url/url_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/arc/arc_util.h"
#endif  // defined(OS_CHROMEOS)

#if defined(OS_WIN)
#include "base/feature_list.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/reporter_runner_win.h"
#endif

using extensions::api::feedback_private::SystemInformation;
using feedback::FeedbackData;

namespace {

// Getting the filename of a blob prepends a "C:\fakepath" to the filename.
// This is undesirable, strip it if it exists.
std::string StripFakepath(const std::string& path) {
  const char kFakePathStr[] = "C:\\fakepath\\";
  if (base::StartsWith(path, kFakePathStr,
                       base::CompareCase::INSENSITIVE_ASCII))
    return path.substr(arraysize(kFakePathStr) - 1);
  return path;
}

#if defined(OS_WIN)
// Allows enabling/disabling SRT Prompt as a Variations feature.
constexpr base::Feature kSrtPromptOnFeedbackForm {
  "SrtPromptOnFeedbackForm", base::FEATURE_DISABLED_BY_DEFAULT
};
#endif

}  // namespace

namespace extensions {

namespace feedback_private = api::feedback_private;

using feedback_private::SystemInformation;
using feedback_private::FeedbackInfo;
using feedback_private::FeedbackFlow;

using SystemInformationList =
    std::vector<api::feedback_private::SystemInformation>;

static base::LazyInstance<BrowserContextKeyedAPIFactory<FeedbackPrivateAPI>>::
    DestructorAtExit g_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<FeedbackPrivateAPI>*
FeedbackPrivateAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

FeedbackPrivateAPI::FeedbackPrivateAPI(content::BrowserContext* context)
    : browser_context_(context), service_(new FeedbackService()) {
}

FeedbackPrivateAPI::~FeedbackPrivateAPI() {
  delete service_;
  service_ = NULL;
}

FeedbackService* FeedbackPrivateAPI::GetService() const {
  return service_;
}

void FeedbackPrivateAPI::RequestFeedback(
    const std::string& description_template,
    const std::string& category_tag,
    const GURL& page_url) {
#if defined(OS_WIN)
  // Show prompt for Software Removal Tool if the Reporter component has found
  // unwanted software, and the user has never run the cleaner before.
  if (base::FeatureList::IsEnabled(kSrtPromptOnFeedbackForm) &&
      safe_browsing::ReporterFoundUws() &&
      !safe_browsing::UserHasRunCleaner()) {
    RequestFeedbackForFlow(description_template, category_tag, page_url,
                           FeedbackFlow::FEEDBACK_FLOW_SHOWSRTPROMPT);
    return;
  }
#endif
  RequestFeedbackForFlow(description_template, category_tag, page_url,
                         FeedbackFlow::FEEDBACK_FLOW_REGULAR);
}

void FeedbackPrivateAPI::RequestFeedbackForFlow(
    const std::string& description_template,
    const std::string& category_tag,
    const GURL& page_url,
    api::feedback_private::FeedbackFlow flow) {
  if (browser_context_ && EventRouter::Get(browser_context_)) {
    FeedbackInfo info;
    info.description = description_template;
    info.category_tag = base::MakeUnique<std::string>(category_tag);
    info.page_url = base::MakeUnique<std::string>(page_url.spec());
    info.system_information.reset(new SystemInformationList);
    // The manager is only available if tracing is enabled.
    if (TracingManager* manager = TracingManager::Get()) {
      info.trace_id.reset(new int(manager->RequestTrace()));
    }
    info.flow = flow;
#if defined(OS_MACOSX)
    const bool use_system_window_frame = true;
#else
    const bool use_system_window_frame = false;
#endif
    info.use_system_window_frame =
        base::MakeUnique<bool>(use_system_window_frame);

    std::unique_ptr<base::ListValue> args =
        feedback_private::OnFeedbackRequested::Create(info);

    auto event = base::MakeUnique<Event>(
        events::FEEDBACK_PRIVATE_ON_FEEDBACK_REQUESTED,
        feedback_private::OnFeedbackRequested::kEventName, std::move(args),
        browser_context_);

    EventRouter::Get(browser_context_)
        ->DispatchEventToExtension(extension_misc::kFeedbackExtensionId,
                                   std::move(event));
  }
}

// static
base::Closure* FeedbackPrivateGetStringsFunction::test_callback_ = NULL;

ExtensionFunction::ResponseAction FeedbackPrivateGetStringsFunction::Run() {
  auto params = feedback_private::GetStrings::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());

#define SET_STRING(id, idr) \
  dict->SetString(id, l10n_util::GetStringUTF16(idr))
  SET_STRING("page-title",
             params->flow == FeedbackFlow::FEEDBACK_FLOW_SADTABCRASH
                 ? IDS_FEEDBACK_REPORT_PAGE_TITLE_SAD_TAB_FLOW
                 : IDS_FEEDBACK_REPORT_PAGE_TITLE);
  SET_STRING("additionalInfo", IDS_FEEDBACK_ADDITIONAL_INFO_LABEL);
  SET_STRING("minimize-btn-label", IDS_FEEDBACK_MINIMIZE_BUTTON_LABEL);
  SET_STRING("close-btn-label", IDS_FEEDBACK_CLOSE_BUTTON_LABEL);
  SET_STRING("page-url", IDS_FEEDBACK_REPORT_URL_LABEL);
  SET_STRING("screenshot", IDS_FEEDBACK_SCREENSHOT_LABEL);
  SET_STRING("user-email", IDS_FEEDBACK_USER_EMAIL_LABEL);
  SET_STRING("anonymous-user", IDS_FEEDBACK_ANONYMOUS_EMAIL_OPTION);
#if defined(OS_CHROMEOS)
  if (arc::IsArcPlayStoreEnabledForProfile(
          Profile::FromBrowserContext(browser_context()))) {
    SET_STRING("sys-info",
               IDS_FEEDBACK_INCLUDE_SYSTEM_INFORMATION_AND_METRICS_CHKBOX_ARC);
  } else {
    SET_STRING("sys-info",
               IDS_FEEDBACK_INCLUDE_SYSTEM_INFORMATION_AND_METRICS_CHKBOX);
  }
#else
  SET_STRING("sys-info", IDS_FEEDBACK_INCLUDE_SYSTEM_INFORMATION_CHKBOX);
#endif
  SET_STRING("attach-file-label", IDS_FEEDBACK_ATTACH_FILE_LABEL);
  SET_STRING("attach-file-note", IDS_FEEDBACK_ATTACH_FILE_NOTE);
  SET_STRING("attach-file-to-big", IDS_FEEDBACK_ATTACH_FILE_TO_BIG);
  SET_STRING("reading-file", IDS_FEEDBACK_READING_FILE);
  SET_STRING("send-report", IDS_FEEDBACK_SEND_REPORT);
  SET_STRING("cancel", IDS_CANCEL);
  SET_STRING("no-description", IDS_FEEDBACK_NO_DESCRIPTION);
  SET_STRING("privacy-note", IDS_FEEDBACK_PRIVACY_NOTE);
  SET_STRING("performance-trace",
             IDS_FEEDBACK_INCLUDE_PERFORMANCE_TRACE_CHECKBOX);
  // Add the localized strings needed for the "system information" page.
  SET_STRING("sysinfoPageTitle", IDS_FEEDBACK_SYSINFO_PAGE_TITLE);
  SET_STRING("sysinfoPageDescription", IDS_ABOUT_SYS_DESC);
  SET_STRING("sysinfoPageTableTitle", IDS_ABOUT_SYS_TABLE_TITLE);
  SET_STRING("sysinfoPageExpandAllBtn", IDS_ABOUT_SYS_EXPAND_ALL);
  SET_STRING("sysinfoPageCollapseAllBtn", IDS_ABOUT_SYS_COLLAPSE_ALL);
  SET_STRING("sysinfoPageExpandBtn", IDS_ABOUT_SYS_EXPAND);
  SET_STRING("sysinfoPageCollapseBtn", IDS_ABOUT_SYS_COLLAPSE);
  SET_STRING("sysinfoPageStatusLoading", IDS_FEEDBACK_SYSINFO_PAGE_LOADING);
  // And the localized strings needed for the SRT Download Prompt.
  SET_STRING("srtPromptBody", IDS_FEEDBACK_SRT_PROMPT_BODY);
  SET_STRING("srtPromptAcceptButton", IDS_FEEDBACK_SRT_PROMPT_ACCEPT_BUTTON);
  SET_STRING("srtPromptDeclineButton",
             IDS_FEEDBACK_SRT_PROMPT_DECLINE_BUTTON);
#undef SET_STRING

  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  webui::SetLoadTimeDataDefaults(app_locale, dict.get());


  if (test_callback_ && !test_callback_->is_null())
    test_callback_->Run();

  return RespondNow(OneArgument(std::move(dict)));
}

ExtensionFunction::ResponseAction FeedbackPrivateGetUserEmailFunction::Run() {
  SigninManagerBase* signin_manager = SigninManagerFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context()));
  return RespondNow(OneArgument(base::MakeUnique<base::Value>(
      signin_manager ? signin_manager->GetAuthenticatedAccountInfo().email
                     : std::string())));
}

ExtensionFunction::ResponseAction
FeedbackPrivateGetSystemInformationFunction::Run() {
  FeedbackService* service = FeedbackPrivateAPI::GetFactoryInstance()
                                 ->Get(browser_context())
                                 ->GetService();
  DCHECK(service);
  service->GetSystemInformation(
      base::Bind(
          &FeedbackPrivateGetSystemInformationFunction::OnCompleted, this));
  return RespondLater();
}

void FeedbackPrivateGetSystemInformationFunction::OnCompleted(
    std::unique_ptr<system_logs::SystemLogsResponse> sys_info) {
  SystemInformationList sys_info_list;
  if (sys_info) {
    sys_info_list.reserve(sys_info->size());
    for (auto& itr : *sys_info) {
      SystemInformation sys_info_entry;
      sys_info_entry.key = std::move(itr.first);
      sys_info_entry.value = std::move(itr.second);
      sys_info_list.emplace_back(std::move(sys_info_entry));
    }
  }

  Respond(ArgumentList(
      feedback_private::GetSystemInformation::Results::Create(sys_info_list)));
}

bool FeedbackPrivateSendFeedbackFunction::RunAsync() {
  std::unique_ptr<feedback_private::SendFeedback::Params> params(
      feedback_private::SendFeedback::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  const FeedbackInfo &feedback_info = params->feedback;

  // Populate feedback data.
  scoped_refptr<FeedbackData> feedback_data(new FeedbackData());
  feedback_data->set_context(GetProfile());
  feedback_data->set_description(feedback_info.description);

  if (feedback_info.product_id)
    feedback_data->set_product_id(*feedback_info.product_id);
  if (feedback_info.category_tag)
    feedback_data->set_category_tag(*feedback_info.category_tag);
  if (feedback_info.page_url)
    feedback_data->set_page_url(*feedback_info.page_url);
  if (feedback_info.email)
    feedback_data->set_user_email(*feedback_info.email);
  if (feedback_info.trace_id)
    feedback_data->set_trace_id(*feedback_info.trace_id);

  if (feedback_info.attached_file_blob_uuid &&
      !feedback_info.attached_file_blob_uuid->empty()) {
    feedback_data->set_attached_filename(
        StripFakepath((*feedback_info.attached_file).name));
    feedback_data->set_attached_file_uuid(
        *feedback_info.attached_file_blob_uuid);
  }

  if (feedback_info.screenshot_blob_uuid &&
      !feedback_info.screenshot_blob_uuid->empty()) {
    feedback_data->set_screenshot_uuid(*feedback_info.screenshot_blob_uuid);
  }

  auto sys_logs = base::MakeUnique<FeedbackData::SystemLogsMap>();
  const SystemInformationList* sys_info =
      feedback_info.system_information.get();
  if (sys_info) {
    for (const SystemInformation& info : *sys_info)
      sys_logs->emplace(info.key, info.value);
  }

  feedback_data->SetAndCompressSystemInfo(std::move(sys_logs));

  FeedbackService* service =
      FeedbackPrivateAPI::GetFactoryInstance()->Get(GetProfile())->GetService();
  DCHECK(service);

  if (feedback_info.send_histograms) {
    auto histograms = base::MakeUnique<std::string>();
    *histograms = base::StatisticsRecorder::ToJSON(std::string());
    if (!histograms->empty())
      feedback_data->SetAndCompressHistograms(std::move(histograms));
  }

  service->SendFeedback(
      GetProfile(),
      feedback_data,
      base::Bind(&FeedbackPrivateSendFeedbackFunction::OnCompleted, this));

  return true;
}

void FeedbackPrivateSendFeedbackFunction::OnCompleted(
    bool success) {
  results_ = feedback_private::SendFeedback::Results::Create(
      success ? feedback_private::STATUS_SUCCESS :
                feedback_private::STATUS_DELAYED);
  SendResponse(true);

  if (!success) {
    // Sending the feedback has been delayed as the user is offline. Show a
    // message box to indicate that.
    chrome::ShowWarningMessageBox(
        nullptr, l10n_util::GetStringUTF16(IDS_FEEDBACK_OFFLINE_DIALOG_TITLE),
        l10n_util::GetStringUTF16(IDS_FEEDBACK_OFFLINE_DIALOG_TEXT));
  }
}

AsyncExtensionFunction::ResponseAction
FeedbackPrivateLogSrtPromptResultFunction::Run() {
  std::unique_ptr<feedback_private::LogSrtPromptResult::Params> params(
      feedback_private::LogSrtPromptResult::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  const feedback_private::SrtPromptResult result = params->result;

  switch (result) {
    case feedback_private::SRT_PROMPT_RESULT_ACCEPTED:
      base::RecordAction(base::UserMetricsAction("Feedback.SrtPromptAccepted"));
      break;
    case feedback_private::SRT_PROMPT_RESULT_DECLINED:
      base::RecordAction(base::UserMetricsAction("Feedback.SrtPromptDeclined"));
      break;
    case feedback_private::SRT_PROMPT_RESULT_CLOSED:
      base::RecordAction(base::UserMetricsAction("Feedback.SrtPromptClosed"));
      break;
    default:
      return RespondNow(Error("Invalid arugment."));
  }
  return RespondNow(NoArguments());
}

}  // namespace extensions
