// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/feedback_private/feedback_private_api.h"

#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/feedback_private/feedback_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/feedback/tracing_manager.h"
#include "extensions/browser/event_router.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "url/url_util.h"

using feedback::FeedbackData;

namespace {

// Getting the filename of a blob prepends a "C:\fakepath" to the filename.
// This is undesirable, strip it if it exists.
std::string StripFakepath(const std::string& path) {
  const char kFakePathStr[] = "C:\\fakepath\\";
  if (StartsWithASCII(path, kFakePathStr, false))
    return path.substr(arraysize(kFakePathStr) - 1);
  return path;
}

}  // namespace

namespace extensions {

namespace feedback_private = api::feedback_private;

using feedback_private::SystemInformation;
using feedback_private::FeedbackInfo;

char kFeedbackExtensionId[] = "gfdkimpbcpahaombhbimeihdjnejgicl";

static base::LazyInstance<BrowserContextKeyedAPIFactory<FeedbackPrivateAPI> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<FeedbackPrivateAPI>*
FeedbackPrivateAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

FeedbackPrivateAPI::FeedbackPrivateAPI(content::BrowserContext* context)
    : browser_context_(context), service_(FeedbackService::CreateInstance()) {}

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
  // TODO(rkc): Remove logging once crbug.com/284662 is closed.
  LOG(WARNING) << "FEEDBACK_DEBUG: Feedback requested.";
  if (browser_context_ && EventRouter::Get(browser_context_)) {
    FeedbackInfo info;
    info.description = description_template;
    info.category_tag = make_scoped_ptr(new std::string(category_tag));
    info.page_url = make_scoped_ptr(new std::string(page_url.spec()));
    info.system_information.reset(new SystemInformationList);
    // The manager is only available if tracing is enabled.
    if (TracingManager* manager = TracingManager::Get()) {
      info.trace_id.reset(new int(manager->RequestTrace()));
    }

    scoped_ptr<base::ListValue> args(new base::ListValue());
    args->Append(info.ToValue().release());

    scoped_ptr<Event> event(new Event(
        feedback_private::OnFeedbackRequested::kEventName, args.Pass()));
    event->restrict_to_browser_context = browser_context_;

    // TODO(rkc): Remove logging once crbug.com/284662 is closed.
    LOG(WARNING) << "FEEDBACK_DEBUG: Dispatching onFeedbackRequested event.";
    EventRouter::Get(browser_context_)
        ->DispatchEventToExtension(kFeedbackExtensionId, event.Pass());
  }
}

// static
base::Closure* FeedbackPrivateGetStringsFunction::test_callback_ = NULL;

bool FeedbackPrivateGetStringsFunction::RunSync() {
  base::DictionaryValue* dict = new base::DictionaryValue();
  SetResult(dict);

#define SET_STRING(id, idr) \
  dict->SetString(id, l10n_util::GetStringUTF16(idr))
  SET_STRING("page-title", IDS_FEEDBACK_REPORT_PAGE_TITLE);
  SET_STRING("page-url", IDS_FEEDBACK_REPORT_URL_LABEL);
  SET_STRING("screenshot", IDS_FEEDBACK_SCREENSHOT_LABEL);
  SET_STRING("user-email", IDS_FEEDBACK_USER_EMAIL_LABEL);
#if defined(OS_CHROMEOS)
  SET_STRING("sys-info",
             IDS_FEEDBACK_INCLUDE_SYSTEM_INFORMATION_AND_METRICS_CHKBOX);
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
#undef SET_STRING

  webui::SetFontAndTextDirection(dict);

  if (test_callback_ && !test_callback_->is_null())
    test_callback_->Run();

  return true;
}

bool FeedbackPrivateGetUserEmailFunction::RunSync() {
  // TODO(rkc): Remove logging once crbug.com/284662 is closed.
  LOG(WARNING) << "FEEDBACK_DEBUG: User e-mail requested.";
  FeedbackService* service =
      FeedbackPrivateAPI::GetFactoryInstance()->Get(GetProfile())->GetService();
  DCHECK(service);
  SetResult(new base::StringValue(service->GetUserEmail()));
  return true;
}

bool FeedbackPrivateGetSystemInformationFunction::RunAsync() {
  // TODO(rkc): Remove logging once crbug.com/284662 is closed.
  LOG(WARNING) << "FEEDBACK_DEBUG: System information requested.";
  FeedbackService* service =
      FeedbackPrivateAPI::GetFactoryInstance()->Get(GetProfile())->GetService();
  DCHECK(service);
  service->GetSystemInformation(
      base::Bind(
          &FeedbackPrivateGetSystemInformationFunction::OnCompleted, this));
  return true;
}

void FeedbackPrivateGetSystemInformationFunction::OnCompleted(
    const SystemInformationList& sys_info) {
  results_ = feedback_private::GetSystemInformation::Results::Create(
      sys_info);
  SendResponse(true);
}

bool FeedbackPrivateSendFeedbackFunction::RunAsync() {
  scoped_ptr<feedback_private::SendFeedback::Params> params(
      feedback_private::SendFeedback::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  const FeedbackInfo &feedback_info = params->feedback;

  std::string attached_file_uuid;
  if (feedback_info.attached_file_blob_uuid.get() &&
      !feedback_info.attached_file_blob_uuid->empty())
    attached_file_uuid = *feedback_info.attached_file_blob_uuid;

  std::string screenshot_uuid;
  if (feedback_info.screenshot_blob_uuid.get() &&
      !feedback_info.screenshot_blob_uuid->empty())
    screenshot_uuid = *feedback_info.screenshot_blob_uuid;

  // Populate feedback data.
  scoped_refptr<FeedbackData> feedback_data(new FeedbackData());
  feedback_data->set_context(GetProfile());
  feedback_data->set_description(feedback_info.description);

  if (feedback_info.category_tag.get())
    feedback_data->set_category_tag(*feedback_info.category_tag.get());
  if (feedback_info.page_url.get())
    feedback_data->set_page_url(*feedback_info.page_url.get());
  if (feedback_info.email.get())
    feedback_data->set_user_email(*feedback_info.email.get());

  if (!attached_file_uuid.empty()) {
    feedback_data->set_attached_filename(
        StripFakepath((*feedback_info.attached_file.get()).name));
    feedback_data->set_attached_file_uuid(attached_file_uuid);
  }

  if (!screenshot_uuid.empty())
    feedback_data->set_screenshot_uuid(screenshot_uuid);

  if (feedback_info.trace_id.get()) {
    feedback_data->set_trace_id(*feedback_info.trace_id.get());
  }

  scoped_ptr<FeedbackData::SystemLogsMap> sys_logs(
      new FeedbackData::SystemLogsMap);
  SystemInformationList* sys_info = feedback_info.system_information.get();
  if (sys_info) {
    for (SystemInformationList::iterator it = sys_info->begin();
         it != sys_info->end(); ++it)
      (*sys_logs.get())[it->get()->key] = it->get()->value;
  }
  feedback_data->SetAndCompressSystemInfo(sys_logs.Pass());

  FeedbackService* service =
      FeedbackPrivateAPI::GetFactoryInstance()->Get(GetProfile())->GetService();
  DCHECK(service);

  if (feedback_info.send_histograms) {
    scoped_ptr<std::string> histograms(new std::string);
    service->GetHistograms(histograms.get());
    if (!histograms->empty())
      feedback_data->SetAndCompressHistograms(histograms.Pass());
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
}

}  // namespace extensions
