// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/fake_drive_service.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "chrome/browser/google_apis/test_util.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace google_apis {

FakeDriveService::FakeDriveService() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

FakeDriveService::~FakeDriveService() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

bool FakeDriveService::LoadResourceListForWapi(
    const std::string& relative_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  scoped_ptr<Value> raw_value = test_util::LoadJSONFile(relative_path);
  base::DictionaryValue* as_dict = NULL;
  base::Value* feed = NULL;
  base::DictionaryValue* feed_as_dict = NULL;

  // Extract the "feed" from the raw value and take the ownership.
  // Note that Remove() transfers the ownership to |feed|.
  if (raw_value->GetAsDictionary(&as_dict) &&
      as_dict->Remove("feed", &feed) &&
      feed->GetAsDictionary(&feed_as_dict)) {
    resource_list_value_.reset(feed_as_dict);
  }

  return resource_list_value_;
}

bool FakeDriveService::LoadAccountMetadataForWapi(
    const std::string& relative_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  account_metadata_value_ = test_util::LoadJSONFile(relative_path);
  return account_metadata_value_;
}

void FakeDriveService::Initialize(Profile* profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveService::AddObserver(DriveServiceObserver* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveService::RemoveObserver(DriveServiceObserver* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

bool FakeDriveService::CanStartOperation() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return true;
}

void FakeDriveService::CancelAll() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

bool FakeDriveService::CancelForFilePath(const FilePath& file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return true;
}

OperationProgressStatusList FakeDriveService::GetProgressStatusList() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return OperationProgressStatusList();
}

bool FakeDriveService::HasAccessToken() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return true;
}

bool FakeDriveService::HasRefreshToken() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return true;
}
void FakeDriveService::GetResourceList(
    const GURL& feed_url,
    int64 start_changestamp,
    const std::string& search_query,
    bool shared_with_me,
    const std::string& directory_resource_id,
    const GetResourceListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<ResourceList> resource_list =
      ResourceList::CreateFrom(*resource_list_value_);
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback,
                 HTTP_SUCCESS,
                 base::Passed(&resource_list)));
}

void FakeDriveService::GetResourceEntry(
    const std::string& resource_id,
    const GetResourceEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  base::DictionaryValue* resource_list_dict = NULL;
  base::ListValue* entries = NULL;
  // Go through entries and return the one that matches |resource_id|.
  if (resource_list_value_->GetAsDictionary(&resource_list_dict) &&
      resource_list_dict->GetList("entry", &entries)) {
    for (size_t i = 0; i < entries->GetSize(); ++i) {
      base::DictionaryValue* entry = NULL;
      base::DictionaryValue* resource_id_dict = NULL;
      std::string current_resource_id;
      if (entries->GetDictionary(i, &entry) &&
          entry->GetDictionary("gd$resourceId", &resource_id_dict) &&
          resource_id_dict->GetString("$t", &current_resource_id) &&
          resource_id == current_resource_id) {
        scoped_ptr<ResourceEntry> resource_entry =
            ResourceEntry::CreateFrom(*entry);
        MessageLoop::current()->PostTask(
            FROM_HERE,
            base::Bind(callback, HTTP_SUCCESS, base::Passed(&resource_entry)));
        return;
      }
    }
  }

  scoped_ptr<ResourceEntry> null;
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_NOT_FOUND, base::Passed(&null)));
}

void FakeDriveService::GetAccountMetadata(
    const GetAccountMetadataCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<AccountMetadataFeed> account_metadata =
      AccountMetadataFeed::CreateFrom(*account_metadata_value_);
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback,
                 HTTP_SUCCESS,
                 base::Passed(&account_metadata)));
}

void FakeDriveService::GetApplicationInfo(
    const GetDataCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
}

void FakeDriveService::DeleteResource(
    const GURL& edit_url,
    const EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  base::DictionaryValue* resource_list_dict = NULL;
  base::ListValue* entries = NULL;
  // Go through entries and remove the one that matches |edit_url|.
  if (resource_list_value_->GetAsDictionary(&resource_list_dict) &&
      resource_list_dict->GetList("entry", &entries)) {
    for (size_t i = 0; i < entries->GetSize(); ++i) {
      base::DictionaryValue* entry = NULL;
      base::ListValue* links = NULL;
      if (entries->GetDictionary(i, &entry) &&
          entry->GetList("link", &links)) {
        for (size_t j = 0; j < links->GetSize(); ++j) {
          base::DictionaryValue* link = NULL;
          std::string rel;
          std::string href;
          if (links->GetDictionary(j, &link) &&
              link->GetString("rel", &rel) &&
              link->GetString("href", &href) &&
              rel == "edit" &&
              GURL(href) == edit_url) {
            entries->Remove(i, NULL);
            MessageLoop::current()->PostTask(
                FROM_HERE, base::Bind(callback, HTTP_SUCCESS));
            return;
          }
        }
      }
    }
  }

  MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, HTTP_NOT_FOUND));
}

void FakeDriveService::DownloadHostedDocument(
    const FilePath& virtual_path,
    const FilePath& local_cache_path,
    const GURL& content_url,
    DocumentExportFormat format,
    const DownloadActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
}

void FakeDriveService::DownloadFile(
    const FilePath& virtual_path,
    const FilePath& local_cache_path,
    const GURL& content_url,
    const DownloadActionCallback& download_action_callback,
    const GetContentCallback& get_content_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!download_action_callback.is_null());
}

void FakeDriveService::CopyHostedDocument(
    const std::string& resource_id,
    const FilePath::StringType& new_name,
    const GetResourceEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
}

void FakeDriveService::RenameResource(
    const GURL& edit_url,
    const FilePath::StringType& new_name,
    const EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
}

void FakeDriveService::AddResourceToDirectory(
    const GURL& parent_content_url,
    const GURL& edit_url,
    const EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
}

void FakeDriveService::RemoveResourceFromDirectory(
    const GURL& parent_content_url,
    const std::string& resource_id,
    const EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
}

void FakeDriveService::AddNewDirectory(
    const GURL& parent_content_url,
    const FilePath::StringType& directory_name,
    const GetResourceEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
}

void FakeDriveService::InitiateUpload(
    const InitiateUploadParams& params,
    const InitiateUploadCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
}

void FakeDriveService::ResumeUpload(const ResumeUploadParams& params,
                                    const ResumeUploadCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
}

void FakeDriveService::AuthorizeApp(const GURL& edit_url,
                                    const std::string& app_id,
                                    const AuthorizeAppCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
}

}  // namespace google_apis
