// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/feedback_data.h"

#include "base/json/json_string_value_serializer.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/feedback/feedback_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/about_sync_util.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_set.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

#if defined(OS_CHROMEOS)
// TODO(rkc): Remove all the code that gather sync data and move it to a
// log data source once crbug.com/138582 is fixed.
namespace {

const char kExtensionsListKey[] = "extensions";

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

void AddExtensionInfoLogs(chromeos::system::LogDictionaryType* logs) {
  bool reporting_enabled = false;
  chromeos::CrosSettings::Get()->GetBoolean(chromeos::kStatsReportingPref,
                                            &reporting_enabled);
  if (!reporting_enabled)
    return;

  Profile* default_profile =
      g_browser_process->profile_manager()->GetDefaultProfile();
  if (!default_profile)
    return;

  ExtensionService* service =
      extensions::ExtensionSystem::Get(default_profile)->extension_service();
  if (!service)
    return;

  std::string extensions_list;
  const ExtensionSet* extensions = service->extensions();
  for (ExtensionSet::const_iterator it = extensions->begin();
       it != extensions->end();
       ++it) {
    const extensions::Extension* extension = *it;
    if (extensions_list.empty()) {
      extensions_list = extension->name();
    } else {
      extensions_list += ", " + extension->name();
    }
  }

  if (!extensions_list.empty())
    (*logs)[kExtensionsListKey] = extensions_list;
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

FeedbackData::FeedbackData(Profile* profile,
                           const std::string& category_tag,
                           const std::string& page_url,
                           const std::string& description,
                           const std::string& user_email,
                           ScreenshotDataPtr image
#if defined(OS_CHROMEOS)
                           , chromeos::system::LogDictionaryType* sys_info
                           , std::string* zip_content
                           , const std::string& timestamp
                           , const std::string& attached_filename
                           , std::string* attached_filedata
#endif
                           ) {
  UpdateData(profile,
             category_tag,
             page_url,
             description,
             user_email,
             image
#if defined(OS_CHROMEOS)
             , sys_info
             , true
             , timestamp
             , attached_filename
             , attached_filedata
#endif
             );
#if defined(OS_CHROMEOS)
  sys_info_ = sys_info;
  zip_content_ = zip_content;
#endif
}


FeedbackData::~FeedbackData() {
}

void FeedbackData::UpdateData(Profile* profile,
                              const std::string& category_tag,
                              const std::string& page_url,
                              const std::string& description,
                              const std::string& user_email,
                              ScreenshotDataPtr image
#if defined(OS_CHROMEOS)
                              , const bool send_sys_info
                              , const bool sent_report
                              , const std::string& timestamp
                              , const std::string& attached_filename
                              , std::string* attached_filedata
#endif
                              ) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  profile_ = profile;
  category_tag_ = category_tag;
  page_url_ = page_url;
  description_ = description;
  user_email_ = user_email;
  image_ = image;
#if defined(OS_CHROMEOS)
  send_sys_info_ = send_sys_info;
  sent_report_ = sent_report;
  timestamp_ = timestamp;
  attached_filename_ = attached_filename;
  attached_filedata_ = attached_filedata;
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

  FeedbackUtil::SendReport(*this);

  // Either the report is sent, and and this object may delete itself, or the
  // report is pending the attached file read, and another FeedbackData has
  // been created to hold this data - hence delete this object.
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
    AddExtensionInfoLogs(logs);

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
