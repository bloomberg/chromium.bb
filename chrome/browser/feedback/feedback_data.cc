// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/feedback_data.h"

#include "base/json/json_string_value_serializer.h"
#include "base/values.h"
#include "chrome/browser/feedback/feedback_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/about_sync_util.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

#if defined(OS_CHROMEOS)
// TODO(rkc): Remove all the code that gather sync data and move it to a
// log data source once crbug.com/138582 is fixed.
namespace {

void AddSyncLogs(chromeos::system::LogDictionaryType* logs) {
  Profile* profile = ProfileManager::GetDefaultProfile();
  if (!ProfileSyncServiceFactory::GetInstance()->HasProfileSyncService(
      profile))
    return;

  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile);
  scoped_ptr<DictionaryValue> sync_logs(
      sync_ui_util::ConstructAboutInformation(service));

  // Remove credentials section.
  ListValue* details = NULL;
  sync_logs->GetList(kDetailsKey, &details);
  if (!details)
    return;
  for (ListValue::iterator it = details->begin();
      it != details->end(); ++it) {
    DictionaryValue* dict = NULL;
    if ((*it)->GetAsDictionary(&dict)) {
      std::string title;
      dict->GetString("title", &title);
      if (title == kCredentialsTitle) {
        details->Erase(it, NULL);
        break;
      }
    }
  }

  // Add sync logs to logs.
  std::string sync_logs_string;
  JSONStringValueSerializer serializer(&sync_logs_string);
  serializer.Serialize(*sync_logs.get());
  (*logs)[kSyncDataKey] = sync_logs_string;
}

}
#endif // OS_CHROMEOS

FeedbackData::FeedbackData()
    : profile_(NULL)
#if defined(OS_CHROMEOS)
    , sys_info_(NULL)
    , zip_content_(NULL)
    , sent_report_(false)
    , send_sys_info_(false)
#endif
{
}

FeedbackData::~FeedbackData() {}

void FeedbackData::UpdateData(Profile* profile,
                              const std::string& target_tab_url,
                              const std::string& category_tag,
                              const std::string& page_url,
                              const std::string& description,
                              const std::string& user_email,
                              ScreenshotDataPtr image
#if defined(OS_CHROMEOS)
                              , const bool send_sys_info
                              , const bool sent_report
                              , const std::string& timestamp
#endif
                              ) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  profile_ = profile;
  target_tab_url_ = target_tab_url;
  category_tag_ = category_tag;
  page_url_ = page_url;
  description_ = description;
  user_email_ = user_email;
  image_ = image;
#if defined(OS_CHROMEOS)
  send_sys_info_ = send_sys_info;
  sent_report_ = sent_report;
  timestamp_ = timestamp;
#endif
}

void FeedbackData::SendReport() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
#if defined(OS_CHROMEOS)
  if (sent_report_)
    return;  // We already received the syslogs and sent the report.

  // Set send_report_ to ensure that we only send one report.
  sent_report_ = true;
#endif

  gfx::Rect& screen_size = FeedbackUtil::GetScreenshotSize();
  FeedbackUtil::SendReport(profile_
                           , category_tag_
                           , page_url_
                           , description_
                           , user_email_
                           , image_
                           , screen_size.width()
                           , screen_size.height()
#if defined(OS_CHROMEOS)
                           , zip_content_ ? zip_content_->c_str() : NULL
                           , zip_content_ ? zip_content_->length() : 0
                           , send_sys_info_ ? sys_info_ : NULL
                           , timestamp_
#endif
                           );

#if defined(OS_CHROMEOS)
  if (sys_info_) {
    delete sys_info_;
    sys_info_ = NULL;
  }
  if (zip_content_) {
    delete zip_content_;
    zip_content_ = NULL;
  }
#endif

  // Delete this object once the report has been sent.
  delete this;
}

#if defined(OS_CHROMEOS)
// SyslogsComplete may be called before UpdateData, in which case, we do not
// want to delete the logs that were gathered, and we do not want to send the
// report either. Instead simply populate |sys_info_| and |zip_content_|.
void FeedbackData::SyslogsComplete(chromeos::system::LogDictionaryType* logs,
                                   std::string* zip_content) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (sent_report_) {
    // We already sent the report, just delete the data.
    if (logs)
      delete logs;
    if (zip_content)
      delete zip_content;
  } else {

    // TODO(rkc): Move to the correct place once crbug.com/138582 is done.
    AddSyncLogs(logs);

    // Will get deleted when SendReport() is called.
    zip_content_ = zip_content;
    sys_info_ = logs;

    if (send_sys_info_) {
      // We already prepared the report, send it now.
      this->SendReport();
    }
  }
}
#endif
