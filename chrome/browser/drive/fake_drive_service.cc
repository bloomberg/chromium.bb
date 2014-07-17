// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/drive/fake_drive_service.h"

#include <string>

#include "base/file_util.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/md5.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/drive/drive_api_util.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/drive/drive_api_parser.h"
#include "google_apis/drive/test_util.h"
#include "net/base/escape.h"
#include "net/base/url_util.h"

using content::BrowserThread;
using google_apis::AboutResource;
using google_apis::AboutResourceCallback;
using google_apis::AppList;
using google_apis::AppListCallback;
using google_apis::AuthStatusCallback;
using google_apis::AuthorizeAppCallback;
using google_apis::CancelCallback;
using google_apis::ChangeList;
using google_apis::ChangeListCallback;
using google_apis::ChangeResource;
using google_apis::DownloadActionCallback;
using google_apis::EntryActionCallback;
using google_apis::FileList;
using google_apis::FileListCallback;
using google_apis::FileResource;
using google_apis::FileResourceCallback;
using google_apis::GDATA_FILE_ERROR;
using google_apis::GDATA_NO_CONNECTION;
using google_apis::GDATA_OTHER_ERROR;
using google_apis::GDataErrorCode;
using google_apis::GetContentCallback;
using google_apis::GetShareUrlCallback;
using google_apis::HTTP_BAD_REQUEST;
using google_apis::HTTP_CREATED;
using google_apis::HTTP_NOT_FOUND;
using google_apis::HTTP_NO_CONTENT;
using google_apis::HTTP_PRECONDITION;
using google_apis::HTTP_RESUME_INCOMPLETE;
using google_apis::HTTP_SUCCESS;
using google_apis::InitiateUploadCallback;
using google_apis::ParentReference;
using google_apis::ProgressCallback;
using google_apis::UploadRangeResponse;
using google_apis::drive::UploadRangeCallback;
namespace test_util = google_apis::test_util;

namespace drive {
namespace {

// Returns true if the entry matches with the search query.
// Supports queries consist of following format.
// - Phrases quoted by double/single quotes
// - AND search for multiple words/phrases segmented by space
// - Limited attribute search.  Only "title:" is supported.
bool EntryMatchWithQuery(const ChangeResource& entry,
                         const std::string& query) {
  base::StringTokenizer tokenizer(query, " ");
  tokenizer.set_quote_chars("\"'");
  while (tokenizer.GetNext()) {
    std::string key, value;
    const std::string& token = tokenizer.token();
    if (token.find(':') == std::string::npos) {
      base::TrimString(token, "\"'", &value);
    } else {
      base::StringTokenizer key_value(token, ":");
      key_value.set_quote_chars("\"'");
      if (!key_value.GetNext())
        return false;
      key = key_value.token();
      if (!key_value.GetNext())
        return false;
      base::TrimString(key_value.token(), "\"'", &value);
    }

    // TODO(peria): Deal with other attributes than title.
    if (!key.empty() && key != "title")
      return false;
    // Search query in the title.
    if (!entry.file() ||
        entry.file()->title().find(value) == std::string::npos)
      return false;
  }
  return true;
}

void ScheduleUploadRangeCallback(const UploadRangeCallback& callback,
                                 int64 start_position,
                                 int64 end_position,
                                 GDataErrorCode error,
                                 scoped_ptr<FileResource> entry) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback,
                 UploadRangeResponse(error,
                                     start_position,
                                     end_position),
                 base::Passed(&entry)));
}

void EntryActionCallbackAdapter(
    const EntryActionCallback& callback,
    GDataErrorCode error, scoped_ptr<FileResource> file) {
  callback.Run(error);
}

void FileListCallbackAdapter(const FileListCallback& callback,
                             GDataErrorCode error,
                             scoped_ptr<ChangeList> change_list) {
  scoped_ptr<FileList> file_list;
  if (change_list) {
    file_list.reset(new FileList);
    file_list->set_next_link(change_list->next_link());
    for (size_t i = 0; i < change_list->items().size(); ++i) {
      const ChangeResource& entry = *change_list->items()[i];
      if (entry.file())
        file_list->mutable_items()->push_back(new FileResource(*entry.file()));
    }
  }
  callback.Run(error, file_list.Pass());
}

}  // namespace

struct FakeDriveService::EntryInfo {
  google_apis::ChangeResource change_resource;
  GURL share_url;
  std::string content_data;
};

struct FakeDriveService::UploadSession {
  std::string content_type;
  int64 content_length;
  std::string parent_resource_id;
  std::string resource_id;
  std::string etag;
  std::string title;

  int64 uploaded_size;

  UploadSession()
      : content_length(0),
        uploaded_size(0) {}

  UploadSession(
      std::string content_type,
      int64 content_length,
      std::string parent_resource_id,
      std::string resource_id,
      std::string etag,
      std::string title)
    : content_type(content_type),
      content_length(content_length),
      parent_resource_id(parent_resource_id),
      resource_id(resource_id),
      etag(etag),
      title(title),
      uploaded_size(0) {
  }
};

FakeDriveService::FakeDriveService()
    : about_resource_(new AboutResource),
      published_date_seq_(0),
      next_upload_sequence_number_(0),
      default_max_results_(0),
      resource_id_count_(0),
      file_list_load_count_(0),
      change_list_load_count_(0),
      directory_load_count_(0),
      about_resource_load_count_(0),
      app_list_load_count_(0),
      blocked_file_list_load_count_(0),
      offline_(false),
      never_return_all_file_list_(false),
      share_url_base_("https://share_url/") {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  about_resource_->set_largest_change_id(654321);
  about_resource_->set_quota_bytes_total(9876543210);
  about_resource_->set_quota_bytes_used(6789012345);
  about_resource_->set_root_folder_id(GetRootResourceId());
}

FakeDriveService::~FakeDriveService() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  STLDeleteValues(&entries_);
}

bool FakeDriveService::LoadAppListForDriveApi(
    const std::string& relative_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Load JSON data, which must be a dictionary.
  scoped_ptr<base::Value> value = test_util::LoadJSONFile(relative_path);
  CHECK_EQ(base::Value::TYPE_DICTIONARY, value->GetType());
  app_info_value_.reset(
      static_cast<base::DictionaryValue*>(value.release()));
  return app_info_value_;
}

void FakeDriveService::AddApp(const std::string& app_id,
                              const std::string& app_name,
                              const std::string& product_id,
                              const std::string& create_url) {
  if (app_json_template_.empty()) {
    base::FilePath path =
        test_util::GetTestFilePath("drive/applist_app_template.json");
    CHECK(base::ReadFileToString(path, &app_json_template_));
  }

  std::string app_json = app_json_template_;
  ReplaceSubstringsAfterOffset(&app_json, 0, "$AppId", app_id);
  ReplaceSubstringsAfterOffset(&app_json, 0, "$AppName", app_name);
  ReplaceSubstringsAfterOffset(&app_json, 0, "$ProductId", product_id);
  ReplaceSubstringsAfterOffset(&app_json, 0, "$CreateUrl", create_url);

  JSONStringValueSerializer json(app_json);
  std::string error_message;
  scoped_ptr<base::Value> value(json.Deserialize(NULL, &error_message));
  CHECK_EQ(base::Value::TYPE_DICTIONARY, value->GetType());

  base::ListValue* item_list;
  CHECK(app_info_value_->GetListWithoutPathExpansion("items", &item_list));
  item_list->Append(value.release());
}

void FakeDriveService::RemoveAppByProductId(const std::string& product_id) {
  base::ListValue* item_list;
  CHECK(app_info_value_->GetListWithoutPathExpansion("items", &item_list));
  for (size_t i = 0; i < item_list->GetSize(); ++i) {
    base::DictionaryValue* item;
    CHECK(item_list->GetDictionary(i, &item));
    const char kKeyProductId[] = "productId";
    std::string item_product_id;
    if (item->GetStringWithoutPathExpansion(kKeyProductId, &item_product_id) &&
        product_id == item_product_id) {
      item_list->Remove(i, NULL);
      return;
    }
  }
}

bool FakeDriveService::HasApp(const std::string& app_id) const {
  base::ListValue* item_list;
  CHECK(app_info_value_->GetListWithoutPathExpansion("items", &item_list));
  for (size_t i = 0; i < item_list->GetSize(); ++i) {
    base::DictionaryValue* item;
    CHECK(item_list->GetDictionary(i, &item));
    const char kKeyId[] = "id";
    std::string item_id;
    if (item->GetStringWithoutPathExpansion(kKeyId, &item_id) &&
        item_id == app_id) {
      return true;
    }
  }

  return false;
}

void FakeDriveService::SetQuotaValue(int64 used, int64 total) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  about_resource_->set_quota_bytes_used(used);
  about_resource_->set_quota_bytes_total(total);
}

GURL FakeDriveService::GetFakeLinkUrl(const std::string& resource_id) {
  return GURL("https://fake_server/" + net::EscapePath(resource_id));
}

void FakeDriveService::Initialize(const std::string& account_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveService::AddObserver(DriveServiceObserver* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveService::RemoveObserver(DriveServiceObserver* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

bool FakeDriveService::CanSendRequest() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return true;
}

ResourceIdCanonicalizer FakeDriveService::GetResourceIdCanonicalizer() const {
  return util::GetIdentityResourceIdCanonicalizer();
}

bool FakeDriveService::HasAccessToken() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return true;
}

void FakeDriveService::RequestAccessToken(const AuthStatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  callback.Run(google_apis::HTTP_NOT_MODIFIED, "fake_access_token");
}

bool FakeDriveService::HasRefreshToken() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return true;
}

void FakeDriveService::ClearAccessToken() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveService::ClearRefreshToken() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

std::string FakeDriveService::GetRootResourceId() const {
  return "fake_root";
}

CancelCallback FakeDriveService::GetAllFileList(
    const FileListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (never_return_all_file_list_) {
    ++blocked_file_list_load_count_;
    return CancelCallback();
  }

  GetChangeListInternal(0,  // start changestamp
                        std::string(),  // empty search query
                        std::string(),  // no directory resource id,
                        0,  // start offset
                        default_max_results_,
                        &file_list_load_count_,
                        base::Bind(&FileListCallbackAdapter, callback));
  return CancelCallback();
}

CancelCallback FakeDriveService::GetFileListInDirectory(
    const std::string& directory_resource_id,
    const FileListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!directory_resource_id.empty());
  DCHECK(!callback.is_null());

  GetChangeListInternal(0,  // start changestamp
                        std::string(),  // empty search query
                        directory_resource_id,
                        0,  // start offset
                        default_max_results_,
                        &directory_load_count_,
                        base::Bind(&FileListCallbackAdapter, callback));
  return CancelCallback();
}

CancelCallback FakeDriveService::Search(
    const std::string& search_query,
    const FileListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!search_query.empty());
  DCHECK(!callback.is_null());

  GetChangeListInternal(0,  // start changestamp
                        search_query,
                        std::string(),  // no directory resource id,
                        0,  // start offset
                        default_max_results_,
                        NULL,
                        base::Bind(&FileListCallbackAdapter, callback));
  return CancelCallback();
}

CancelCallback FakeDriveService::SearchByTitle(
    const std::string& title,
    const std::string& directory_resource_id,
    const FileListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!title.empty());
  DCHECK(!callback.is_null());

  // Note: the search implementation here doesn't support quotation unescape,
  // so don't escape here.
  GetChangeListInternal(0,  // start changestamp
                        base::StringPrintf("title:'%s'", title.c_str()),
                        directory_resource_id,
                        0,  // start offset
                        default_max_results_,
                        NULL,
                        base::Bind(&FileListCallbackAdapter, callback));
  return CancelCallback();
}

CancelCallback FakeDriveService::GetChangeList(
    int64 start_changestamp,
    const ChangeListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  GetChangeListInternal(start_changestamp,
                        std::string(),  // empty search query
                        std::string(),  // no directory resource id,
                        0,  // start offset
                        default_max_results_,
                        &change_list_load_count_,
                        callback);
  return CancelCallback();
}

CancelCallback FakeDriveService::GetRemainingChangeList(
    const GURL& next_link,
    const ChangeListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!next_link.is_empty());
  DCHECK(!callback.is_null());

  // "changestamp", "q", "parent" and "start-offset" are parameters to
  // implement "paging" of the result on FakeDriveService.
  // The URL should be the one filled in GetChangeListInternal of the
  // previous method invocation, so it should start with "http://localhost/?".
  // See also GetChangeListInternal.
  DCHECK_EQ(next_link.host(), "localhost");
  DCHECK_EQ(next_link.path(), "/");

  int64 start_changestamp = 0;
  std::string search_query;
  std::string directory_resource_id;
  int start_offset = 0;
  int max_results = default_max_results_;
  std::vector<std::pair<std::string, std::string> > parameters;
  if (base::SplitStringIntoKeyValuePairs(
          next_link.query(), '=', '&', &parameters)) {
    for (size_t i = 0; i < parameters.size(); ++i) {
      if (parameters[i].first == "changestamp") {
        base::StringToInt64(parameters[i].second, &start_changestamp);
      } else if (parameters[i].first == "q") {
        search_query =
            net::UnescapeURLComponent(parameters[i].second,
                                      net::UnescapeRule::URL_SPECIAL_CHARS);
      } else if (parameters[i].first == "parent") {
        directory_resource_id =
            net::UnescapeURLComponent(parameters[i].second,
                                      net::UnescapeRule::URL_SPECIAL_CHARS);
      } else if (parameters[i].first == "start-offset") {
        base::StringToInt(parameters[i].second, &start_offset);
      } else if (parameters[i].first == "max-results") {
        base::StringToInt(parameters[i].second, &max_results);
      }
    }
  }

  GetChangeListInternal(start_changestamp, search_query, directory_resource_id,
                        start_offset, max_results, NULL, callback);
  return CancelCallback();
}

CancelCallback FakeDriveService::GetRemainingFileList(
    const GURL& next_link,
    const FileListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!next_link.is_empty());
  DCHECK(!callback.is_null());

  return GetRemainingChangeList(
      next_link, base::Bind(&FileListCallbackAdapter, callback));
}

CancelCallback FakeDriveService::GetFileResource(
    const std::string& resource_id,
    const FileResourceCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (offline_) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   GDATA_NO_CONNECTION,
                   base::Passed(scoped_ptr<FileResource>())));
    return CancelCallback();
  }

  EntryInfo* entry = FindEntryByResourceId(resource_id);
  if (entry && entry->change_resource.file()) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, HTTP_SUCCESS, base::Passed(make_scoped_ptr(
            new FileResource(*entry->change_resource.file())))));
    return CancelCallback();
  }

  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_NOT_FOUND,
                 base::Passed(scoped_ptr<FileResource>())));
  return CancelCallback();
}

CancelCallback FakeDriveService::GetShareUrl(
    const std::string& resource_id,
    const GURL& /* embed_origin */,
    const GetShareUrlCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (offline_) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   GDATA_NO_CONNECTION,
                   GURL()));
    return CancelCallback();
  }

  EntryInfo* entry = FindEntryByResourceId(resource_id);
  if (entry) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, HTTP_SUCCESS, entry->share_url));
    return CancelCallback();
  }

  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_NOT_FOUND, GURL()));
  return CancelCallback();
}

CancelCallback FakeDriveService::GetAboutResource(
    const AboutResourceCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (offline_) {
    scoped_ptr<AboutResource> null;
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   GDATA_NO_CONNECTION, base::Passed(&null)));
    return CancelCallback();
  }

  ++about_resource_load_count_;
  scoped_ptr<AboutResource> about_resource(new AboutResource(*about_resource_));
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback,
                 HTTP_SUCCESS, base::Passed(&about_resource)));
  return CancelCallback();
}

CancelCallback FakeDriveService::GetAppList(const AppListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK(app_info_value_);

  if (offline_) {
    scoped_ptr<AppList> null;
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   GDATA_NO_CONNECTION,
                   base::Passed(&null)));
    return CancelCallback();
  }

  ++app_list_load_count_;
  scoped_ptr<AppList> app_list(AppList::CreateFrom(*app_info_value_));
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_SUCCESS, base::Passed(&app_list)));
  return CancelCallback();
}

CancelCallback FakeDriveService::DeleteResource(
    const std::string& resource_id,
    const std::string& etag,
    const EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (offline_) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, GDATA_NO_CONNECTION));
    return CancelCallback();
  }

  EntryInfo* entry = FindEntryByResourceId(resource_id);
  if (entry) {
    ChangeResource* change = &entry->change_resource;
    const FileResource* file = change->file();
    if (change->is_deleted()) {
      base::MessageLoop::current()->PostTask(
          FROM_HERE, base::Bind(callback, HTTP_NOT_FOUND));
      return CancelCallback();
    }

    if (!etag.empty() && etag != file->etag()) {
      base::MessageLoop::current()->PostTask(
          FROM_HERE, base::Bind(callback, HTTP_PRECONDITION));
      return CancelCallback();
    }

    change->set_deleted(true);
    AddNewChangestamp(change);
    change->set_file(scoped_ptr<FileResource>());
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, HTTP_NO_CONTENT));
    return CancelCallback();
  }

  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, HTTP_NOT_FOUND));
  return CancelCallback();
}

CancelCallback FakeDriveService::TrashResource(
    const std::string& resource_id,
    const EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (offline_) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, GDATA_NO_CONNECTION));
    return CancelCallback();
  }

  EntryInfo* entry = FindEntryByResourceId(resource_id);
  if (entry) {
    ChangeResource* change = &entry->change_resource;
    FileResource* file = change->mutable_file();
    GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
    if (change->is_deleted() || file->labels().is_trashed()) {
      error = HTTP_NOT_FOUND;
    } else {
      file->mutable_labels()->set_trashed(true);
      AddNewChangestamp(change);
      error = HTTP_SUCCESS;
    }
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, error));
    return CancelCallback();
  }

  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, HTTP_NOT_FOUND));
  return CancelCallback();
}

CancelCallback FakeDriveService::DownloadFile(
    const base::FilePath& local_cache_path,
    const std::string& resource_id,
    const DownloadActionCallback& download_action_callback,
    const GetContentCallback& get_content_callback,
    const ProgressCallback& progress_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!download_action_callback.is_null());

  if (offline_) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(download_action_callback,
                   GDATA_NO_CONNECTION,
                   base::FilePath()));
    return CancelCallback();
  }

  EntryInfo* entry = FindEntryByResourceId(resource_id);
  if (!entry) {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(download_action_callback, HTTP_NOT_FOUND, base::FilePath()));
    return CancelCallback();
  }

  const FileResource* file = entry->change_resource.file();
  const std::string& content_data = entry->content_data;
  int64 file_size = file->file_size();
  DCHECK_EQ(static_cast<size_t>(file_size), content_data.size());

  if (!get_content_callback.is_null()) {
    const int64 kBlockSize = 5;
    for (int64 i = 0; i < file_size; i += kBlockSize) {
      const int64 size = std::min(kBlockSize, file_size - i);
      scoped_ptr<std::string> content_for_callback(
          new std::string(content_data.substr(i, size)));
      base::MessageLoopProxy::current()->PostTask(
          FROM_HERE,
          base::Bind(get_content_callback, HTTP_SUCCESS,
                     base::Passed(&content_for_callback)));
    }
  }

  if (test_util::WriteStringToFile(local_cache_path, content_data)) {
    if (!progress_callback.is_null()) {
      // See also the comment in ResumeUpload(). For testing that clients
      // can handle the case progress_callback is called multiple times,
      // here we invoke the callback twice.
      base::MessageLoopProxy::current()->PostTask(
          FROM_HERE,
          base::Bind(progress_callback, file_size / 2, file_size));
      base::MessageLoopProxy::current()->PostTask(
          FROM_HERE,
          base::Bind(progress_callback, file_size, file_size));
    }
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(download_action_callback,
                   HTTP_SUCCESS,
                   local_cache_path));
    return CancelCallback();
  }

  // Failed to write the content.
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(download_action_callback, GDATA_FILE_ERROR, base::FilePath()));
  return CancelCallback();
}

CancelCallback FakeDriveService::CopyResource(
    const std::string& resource_id,
    const std::string& in_parent_resource_id,
    const std::string& new_title,
    const base::Time& last_modified,
    const FileResourceCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (offline_) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   GDATA_NO_CONNECTION,
                   base::Passed(scoped_ptr<FileResource>())));
    return CancelCallback();
  }

  const std::string& parent_resource_id = in_parent_resource_id.empty() ?
      GetRootResourceId() : in_parent_resource_id;

  EntryInfo* entry = FindEntryByResourceId(resource_id);
  if (entry) {
    // Make a copy and set the new resource ID and the new title.
    scoped_ptr<EntryInfo> copied_entry(new EntryInfo);
    copied_entry->content_data = entry->content_data;
    copied_entry->share_url = entry->share_url;
    copied_entry->change_resource.set_file(
        make_scoped_ptr(new FileResource(*entry->change_resource.file())));

    ChangeResource* new_change = &copied_entry->change_resource;
    FileResource* new_file = new_change->mutable_file();
    const std::string new_resource_id = GetNewResourceId();
    new_change->set_file_id(new_resource_id);
    new_file->set_file_id(new_resource_id);
    new_file->set_title(new_title);

    ParentReference parent;
    parent.set_file_id(parent_resource_id);
    parent.set_parent_link(GetFakeLinkUrl(parent_resource_id));
    std::vector<ParentReference> parents;
    parents.push_back(parent);
    *new_file->mutable_parents() = parents;

    if (!last_modified.is_null())
      new_file->set_modified_date(last_modified);

    AddNewChangestamp(new_change);
    UpdateETag(new_file);

    // Add the new entry to the map.
    entries_[new_resource_id] = copied_entry.release();

    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   HTTP_SUCCESS,
                   base::Passed(make_scoped_ptr(new FileResource(*new_file)))));
    return CancelCallback();
  }

  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_NOT_FOUND,
                 base::Passed(scoped_ptr<FileResource>())));
  return CancelCallback();
}

CancelCallback FakeDriveService::UpdateResource(
    const std::string& resource_id,
    const std::string& parent_resource_id,
    const std::string& new_title,
    const base::Time& last_modified,
    const base::Time& last_viewed_by_me,
    const google_apis::FileResourceCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (offline_) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, GDATA_NO_CONNECTION,
                              base::Passed(scoped_ptr<FileResource>())));
    return CancelCallback();
  }

  EntryInfo* entry = FindEntryByResourceId(resource_id);
  if (entry) {
    ChangeResource* change = &entry->change_resource;
    FileResource* file = change->mutable_file();

    if (!new_title.empty())
      file->set_title(new_title);

    // Set parent if necessary.
    if (!parent_resource_id.empty()) {
      ParentReference parent;
      parent.set_file_id(parent_resource_id);
      parent.set_parent_link(GetFakeLinkUrl(parent_resource_id));

      std::vector<ParentReference> parents;
      parents.push_back(parent);
      *file->mutable_parents() = parents;
    }

    if (!last_modified.is_null())
      file->set_modified_date(last_modified);

    if (!last_viewed_by_me.is_null())
      file->set_last_viewed_by_me_date(last_viewed_by_me);

    AddNewChangestamp(change);
    UpdateETag(file);

    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, HTTP_SUCCESS,
                   base::Passed(make_scoped_ptr(new FileResource(*file)))));
    return CancelCallback();
  }

  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_NOT_FOUND,
                 base::Passed(scoped_ptr<FileResource>())));
  return CancelCallback();
}

CancelCallback FakeDriveService::RenameResource(
    const std::string& resource_id,
    const std::string& new_title,
    const EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  return UpdateResource(
      resource_id, std::string(), new_title, base::Time(), base::Time(),
      base::Bind(&EntryActionCallbackAdapter, callback));
}

CancelCallback FakeDriveService::AddResourceToDirectory(
    const std::string& parent_resource_id,
    const std::string& resource_id,
    const EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (offline_) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, GDATA_NO_CONNECTION));
    return CancelCallback();
  }

  EntryInfo* entry = FindEntryByResourceId(resource_id);
  if (entry) {
    ChangeResource* change = &entry->change_resource;
    // On the real Drive server, resources do not necessary shape a tree
    // structure. That is, each resource can have multiple parent.
    // We mimic the behavior here; AddResourceToDirectoy just adds
    // one more parent, not overwriting old ones.
    ParentReference parent;
    parent.set_file_id(parent_resource_id);
    parent.set_parent_link(GetFakeLinkUrl(parent_resource_id));
    change->mutable_file()->mutable_parents()->push_back(parent);

    AddNewChangestamp(change);
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, HTTP_SUCCESS));
    return CancelCallback();
  }

  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, HTTP_NOT_FOUND));
  return CancelCallback();
}

CancelCallback FakeDriveService::RemoveResourceFromDirectory(
    const std::string& parent_resource_id,
    const std::string& resource_id,
    const EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (offline_) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, GDATA_NO_CONNECTION));
    return CancelCallback();
  }

  EntryInfo* entry = FindEntryByResourceId(resource_id);
  if (entry) {
    ChangeResource* change = &entry->change_resource;
    FileResource* file = change->mutable_file();
    std::vector<ParentReference>* parents = file->mutable_parents();
    for (size_t i = 0; i < parents->size(); ++i) {
      if ((*parents)[i].file_id() == parent_resource_id) {
        parents->erase(parents->begin() + i);
        AddNewChangestamp(change);
        base::MessageLoop::current()->PostTask(
            FROM_HERE, base::Bind(callback, HTTP_NO_CONTENT));
        return CancelCallback();
      }
    }
  }

  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, HTTP_NOT_FOUND));
  return CancelCallback();
}

CancelCallback FakeDriveService::AddNewDirectory(
    const std::string& parent_resource_id,
    const std::string& directory_title,
    const AddNewDirectoryOptions& options,
    const FileResourceCallback& callback) {
  return AddNewDirectoryWithResourceId(
      "",
      parent_resource_id.empty() ? GetRootResourceId() : parent_resource_id,
      directory_title,
      options,
      callback);
}

CancelCallback FakeDriveService::InitiateUploadNewFile(
    const std::string& content_type,
    int64 content_length,
    const std::string& parent_resource_id,
    const std::string& title,
    const InitiateUploadNewFileOptions& options,
    const InitiateUploadCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (offline_) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, GDATA_NO_CONNECTION, GURL()));
    return CancelCallback();
  }

  if (parent_resource_id != GetRootResourceId() &&
      !entries_.count(parent_resource_id)) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, HTTP_NOT_FOUND, GURL()));
    return CancelCallback();
  }

  GURL session_url = GetNewUploadSessionUrl();
  upload_sessions_[session_url] =
      UploadSession(content_type, content_length,
                    parent_resource_id,
                    "",  // resource_id
                    "",  // etag
                    title);

  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_SUCCESS, session_url));
  return CancelCallback();
}

CancelCallback FakeDriveService::InitiateUploadExistingFile(
    const std::string& content_type,
    int64 content_length,
    const std::string& resource_id,
    const InitiateUploadExistingFileOptions& options,
    const InitiateUploadCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (offline_) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, GDATA_NO_CONNECTION, GURL()));
    return CancelCallback();
  }

  EntryInfo* entry = FindEntryByResourceId(resource_id);
  if (!entry) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, HTTP_NOT_FOUND, GURL()));
    return CancelCallback();
  }

  FileResource* file = entry->change_resource.mutable_file();
  if (!options.etag.empty() && options.etag != file->etag()) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, HTTP_PRECONDITION, GURL()));
    return CancelCallback();
  }
  // TODO(hashimoto): Update |file|'s metadata with |options|.

  GURL session_url = GetNewUploadSessionUrl();
  upload_sessions_[session_url] =
      UploadSession(content_type, content_length,
                    "",  // parent_resource_id
                    resource_id,
                    file->etag(),
                    "" /* title */);

  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_SUCCESS, session_url));
  return CancelCallback();
}

CancelCallback FakeDriveService::GetUploadStatus(
    const GURL& upload_url,
    int64 content_length,
    const UploadRangeCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  return CancelCallback();
}

CancelCallback FakeDriveService::ResumeUpload(
      const GURL& upload_url,
      int64 start_position,
      int64 end_position,
      int64 content_length,
      const std::string& content_type,
      const base::FilePath& local_file_path,
      const UploadRangeCallback& callback,
      const ProgressCallback& progress_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  FileResourceCallback completion_callback
      = base::Bind(&ScheduleUploadRangeCallback,
                   callback, start_position, end_position);

  if (offline_) {
    completion_callback.Run(GDATA_NO_CONNECTION, scoped_ptr<FileResource>());
    return CancelCallback();
  }

  if (!upload_sessions_.count(upload_url)) {
    completion_callback.Run(HTTP_NOT_FOUND, scoped_ptr<FileResource>());
    return CancelCallback();
  }

  UploadSession* session = &upload_sessions_[upload_url];

  // Chunks are required to be sent in such a ways that they fill from the start
  // of the not-yet-uploaded part with no gaps nor overlaps.
  if (session->uploaded_size != start_position) {
    completion_callback.Run(HTTP_BAD_REQUEST, scoped_ptr<FileResource>());
    return CancelCallback();
  }

  if (!progress_callback.is_null()) {
    // In the real GDataWapi/Drive DriveService, progress is reported in
    // nondeterministic timing. In this fake implementation, we choose to call
    // it twice per one ResumeUpload. This is for making sure that client code
    // works fine even if the callback is invoked more than once; it is the
    // crucial difference of the progress callback from others.
    // Note that progress is notified in the relative offset in each chunk.
    const int64 chunk_size = end_position - start_position;
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(progress_callback, chunk_size / 2, chunk_size));
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(progress_callback, chunk_size, chunk_size));
  }

  if (content_length != end_position) {
    session->uploaded_size = end_position;
    completion_callback.Run(HTTP_RESUME_INCOMPLETE, scoped_ptr<FileResource>());
    return CancelCallback();
  }

  std::string content_data;
  if (!base::ReadFileToString(local_file_path, &content_data)) {
    session->uploaded_size = end_position;
    completion_callback.Run(GDATA_FILE_ERROR, scoped_ptr<FileResource>());
    return CancelCallback();
  }
  session->uploaded_size = end_position;

  // |resource_id| is empty if the upload is for new file.
  if (session->resource_id.empty()) {
    DCHECK(!session->parent_resource_id.empty());
    DCHECK(!session->title.empty());
    const EntryInfo* new_entry = AddNewEntry(
        "",  // auto generate resource id.
        session->content_type,
        content_data,
        session->parent_resource_id,
        session->title,
        false);  // shared_with_me
    if (!new_entry) {
      completion_callback.Run(HTTP_NOT_FOUND, scoped_ptr<FileResource>());
      return CancelCallback();
    }

    completion_callback.Run(HTTP_CREATED, make_scoped_ptr(
        new FileResource(*new_entry->change_resource.file())));
    return CancelCallback();
  }

  EntryInfo* entry = FindEntryByResourceId(session->resource_id);
  if (!entry) {
    completion_callback.Run(HTTP_NOT_FOUND, scoped_ptr<FileResource>());
    return CancelCallback();
  }

  ChangeResource* change = &entry->change_resource;
  FileResource* file = change->mutable_file();
  if (file->etag().empty() || session->etag != file->etag()) {
    completion_callback.Run(HTTP_PRECONDITION, scoped_ptr<FileResource>());
    return CancelCallback();
  }

  file->set_md5_checksum(base::MD5String(content_data));
  entry->content_data = content_data;
  file->set_file_size(end_position);
  AddNewChangestamp(change);
  UpdateETag(file);

  completion_callback.Run(HTTP_SUCCESS, make_scoped_ptr(
      new FileResource(*file)));
  return CancelCallback();
}

CancelCallback FakeDriveService::AuthorizeApp(
    const std::string& resource_id,
    const std::string& app_id,
    const AuthorizeAppCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (entries_.count(resource_id) == 0) {
    callback.Run(google_apis::HTTP_NOT_FOUND, GURL());
    return CancelCallback();
  }

  callback.Run(HTTP_SUCCESS,
               GURL(base::StringPrintf(open_url_format_.c_str(),
                                       resource_id.c_str(),
                                       app_id.c_str())));
  return CancelCallback();
}

CancelCallback FakeDriveService::UninstallApp(
    const std::string& app_id,
    const google_apis::EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // Find app_id from app_info_value_ and delete.
  google_apis::GDataErrorCode error = google_apis::HTTP_NOT_FOUND;
  if (offline_) {
    error = google_apis::GDATA_NO_CONNECTION;
  } else {
    base::ListValue* items = NULL;
    if (app_info_value_->GetList("items", &items)) {
      for (size_t i = 0; i < items->GetSize(); ++i) {
        base::DictionaryValue* item = NULL;
        std::string id;
        if (items->GetDictionary(i, &item) && item->GetString("id", &id) &&
            id == app_id) {
          if (items->Remove(i, NULL))
            error = google_apis::HTTP_NO_CONTENT;
          break;
        }
      }
    }
  }

  base::MessageLoop::current()->PostTask(FROM_HERE,
                                         base::Bind(callback, error));
  return CancelCallback();
}

void FakeDriveService::AddNewFile(const std::string& content_type,
                                  const std::string& content_data,
                                  const std::string& parent_resource_id,
                                  const std::string& title,
                                  bool shared_with_me,
                                  const FileResourceCallback& callback) {
  AddNewFileWithResourceId("", content_type, content_data, parent_resource_id,
                           title, shared_with_me, callback);
}

void FakeDriveService::AddNewFileWithResourceId(
    const std::string& resource_id,
    const std::string& content_type,
    const std::string& content_data,
    const std::string& parent_resource_id,
    const std::string& title,
    bool shared_with_me,
    const FileResourceCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (offline_) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   GDATA_NO_CONNECTION,
                   base::Passed(scoped_ptr<FileResource>())));
    return;
  }

  const EntryInfo* new_entry = AddNewEntry(resource_id,
                                           content_type,
                                           content_data,
                                           parent_resource_id,
                                           title,
                                           shared_with_me);
  if (!new_entry) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, HTTP_NOT_FOUND,
                   base::Passed(scoped_ptr<FileResource>())));
    return;
  }

  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_CREATED,
                 base::Passed(make_scoped_ptr(
                     new FileResource(*new_entry->change_resource.file())))));
}

CancelCallback FakeDriveService::AddNewDirectoryWithResourceId(
    const std::string& resource_id,
    const std::string& parent_resource_id,
    const std::string& directory_title,
    const AddNewDirectoryOptions& options,
    const FileResourceCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (offline_) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   GDATA_NO_CONNECTION,
                   base::Passed(scoped_ptr<FileResource>())));
    return CancelCallback();
  }

  const EntryInfo* new_entry = AddNewEntry(resource_id,
                                           util::kDriveFolderMimeType,
                                           "",  // content_data
                                           parent_resource_id,
                                           directory_title,
                                           false);  // shared_with_me
  if (!new_entry) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, HTTP_NOT_FOUND,
                   base::Passed(scoped_ptr<FileResource>())));
    return CancelCallback();
  }

  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_CREATED,
                 base::Passed(make_scoped_ptr(
                     new FileResource(*new_entry->change_resource.file())))));
  return CancelCallback();
}

void FakeDriveService::SetLastModifiedTime(
    const std::string& resource_id,
    const base::Time& last_modified_time,
    const FileResourceCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (offline_) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   GDATA_NO_CONNECTION,
                   base::Passed(scoped_ptr<FileResource>())));
    return;
  }

  EntryInfo* entry = FindEntryByResourceId(resource_id);
  if (!entry) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, HTTP_NOT_FOUND,
                   base::Passed(scoped_ptr<FileResource>())));
    return;
  }

  ChangeResource* change = &entry->change_resource;
  FileResource* file = change->mutable_file();
  file->set_modified_date(last_modified_time);

  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_SUCCESS,
                 base::Passed(make_scoped_ptr(new FileResource(*file)))));
}

FakeDriveService::EntryInfo* FakeDriveService::FindEntryByResourceId(
    const std::string& resource_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  EntryInfoMap::iterator it = entries_.find(resource_id);
  // Deleted entries don't have FileResource.
  return it != entries_.end() && it->second->change_resource.file() ?
      it->second : NULL;
}

std::string FakeDriveService::GetNewResourceId() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  ++resource_id_count_;
  return base::StringPrintf("resource_id_%d", resource_id_count_);
}

void FakeDriveService::UpdateETag(google_apis::FileResource* file) {
  file->set_etag(
      "etag_" + base::Int64ToString(about_resource_->largest_change_id()));
}

void FakeDriveService::AddNewChangestamp(google_apis::ChangeResource* change) {
  about_resource_->set_largest_change_id(
      about_resource_->largest_change_id() + 1);
  change->set_change_id(about_resource_->largest_change_id());
}

const FakeDriveService::EntryInfo* FakeDriveService::AddNewEntry(
    const std::string& given_resource_id,
    const std::string& content_type,
    const std::string& content_data,
    const std::string& parent_resource_id,
    const std::string& title,
    bool shared_with_me) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!parent_resource_id.empty() &&
      parent_resource_id != GetRootResourceId() &&
      !entries_.count(parent_resource_id)) {
    return NULL;
  }

  const std::string resource_id =
      given_resource_id.empty() ? GetNewResourceId() : given_resource_id;
  if (entries_.count(resource_id))
    return NULL;
  GURL upload_url = GURL("https://xxx/upload/" + resource_id);

  scoped_ptr<EntryInfo> new_entry(new EntryInfo);
  ChangeResource* new_change = &new_entry->change_resource;
  FileResource* new_file = new FileResource;
  new_change->set_file(make_scoped_ptr(new_file));

  // Set the resource ID and the title
  new_change->set_file_id(resource_id);
  new_file->set_file_id(resource_id);
  new_file->set_title(title);
  // Set the contents, size and MD5 for a file.
  if (content_type != util::kDriveFolderMimeType) {
    new_entry->content_data = content_data;
    new_file->set_file_size(content_data.size());
    new_file->set_md5_checksum(base::MD5String(content_data));
  }

  if (shared_with_me) {
    // Set current time to mark the file as shared_with_me.
    new_file->set_shared_with_me_date(base::Time::Now());
  }

  std::string escaped_resource_id = net::EscapePath(resource_id);

  // Set mime type.
  new_file->set_mime_type(content_type);

  // Set alternate link if needed.
  if (content_type == util::kGoogleDocumentMimeType)
    new_file->set_alternate_link(GURL("https://document_alternate_link"));

  // Set parents.
  if (!parent_resource_id.empty()) {
    ParentReference parent;
    parent.set_file_id(parent_resource_id);
    parent.set_parent_link(GetFakeLinkUrl(parent.file_id()));
    std::vector<ParentReference> parents;
    parents.push_back(parent);
    *new_file->mutable_parents() = parents;
  }

  new_entry->share_url = net::AppendOrReplaceQueryParameter(
      share_url_base_, "name", title);

  AddNewChangestamp(new_change);
  UpdateETag(new_file);

  base::Time published_date =
      base::Time() + base::TimeDelta::FromMilliseconds(++published_date_seq_);
  new_file->set_created_date(published_date);

  EntryInfo* raw_new_entry = new_entry.release();
  entries_[resource_id] = raw_new_entry;
  return raw_new_entry;
}

void FakeDriveService::GetChangeListInternal(
    int64 start_changestamp,
    const std::string& search_query,
    const std::string& directory_resource_id,
    int start_offset,
    int max_results,
    int* load_counter,
    const ChangeListCallback& callback) {
  if (offline_) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   GDATA_NO_CONNECTION,
                   base::Passed(scoped_ptr<ChangeList>())));
    return;
  }

  // Filter out entries per parameters like |directory_resource_id| and
  // |search_query|.
  ScopedVector<ChangeResource> entries;
  int num_entries_matched = 0;
  for (EntryInfoMap::iterator it = entries_.begin(); it != entries_.end();
       ++it) {
    const ChangeResource& entry = it->second->change_resource;
    bool should_exclude = false;

    // If |directory_resource_id| is set, exclude the entry if it's not in
    // the target directory.
    if (!directory_resource_id.empty()) {
      // Get the parent resource ID of the entry.
      std::string parent_resource_id;
      if (entry.file() && !entry.file()->parents().empty())
        parent_resource_id = entry.file()->parents()[0].file_id();

      if (directory_resource_id != parent_resource_id)
        should_exclude = true;
    }

    // If |search_query| is set, exclude the entry if it does not contain the
    // search query in the title.
    if (!should_exclude && !search_query.empty() &&
        !EntryMatchWithQuery(entry, search_query)) {
      should_exclude = true;
    }

    // If |start_changestamp| is set, exclude the entry if the
    // changestamp is older than |largest_changestamp|.
    // See https://developers.google.com/google-apps/documents-list/
    // #retrieving_all_changes_since_a_given_changestamp
    if (start_changestamp > 0 && entry.change_id() < start_changestamp)
      should_exclude = true;

    // If the caller requests other list than change list by specifying
    // zero-|start_changestamp|, exclude deleted entry from the result.
    const bool deleted = entry.is_deleted() ||
        (entry.file() && entry.file()->labels().is_trashed());
    if (!start_changestamp && deleted)
      should_exclude = true;

    // The entry matched the criteria for inclusion.
    if (!should_exclude)
      ++num_entries_matched;

    // If |start_offset| is set, exclude the entry if the entry is before the
    // start index. <= instead of < as |num_entries_matched| was
    // already incremented.
    if (start_offset > 0 && num_entries_matched <= start_offset)
      should_exclude = true;

    if (!should_exclude) {
      scoped_ptr<ChangeResource> entry_copied(new ChangeResource);
      entry_copied->set_change_id(entry.change_id());
      entry_copied->set_file_id(entry.file_id());
      entry_copied->set_deleted(entry.is_deleted());
      if (entry.file()) {
        entry_copied->set_file(
            make_scoped_ptr(new FileResource(*entry.file())));
      }
      entry_copied->set_modification_date(entry.modification_date());
      entries.push_back(entry_copied.release());
    }
  }

  scoped_ptr<ChangeList> change_list(new ChangeList);
  if (start_changestamp > 0 && start_offset == 0) {
    change_list->set_largest_change_id(about_resource_->largest_change_id());
  }

  // If |max_results| is set, trim the entries if the number exceeded the max
  // results.
  if (max_results > 0 && entries.size() > static_cast<size_t>(max_results)) {
    entries.erase(entries.begin() + max_results, entries.end());
    // Adds the next URL.
    // Here, we embed information which is needed for continuing the
    // GetChangeList request in the next invocation into url query
    // parameters.
    GURL next_url(base::StringPrintf(
        "http://localhost/?start-offset=%d&max-results=%d",
        start_offset + max_results,
        max_results));
    if (start_changestamp > 0) {
      next_url = net::AppendOrReplaceQueryParameter(
          next_url, "changestamp",
          base::Int64ToString(start_changestamp).c_str());
    }
    if (!search_query.empty()) {
      next_url = net::AppendOrReplaceQueryParameter(
          next_url, "q", search_query);
    }
    if (!directory_resource_id.empty()) {
      next_url = net::AppendOrReplaceQueryParameter(
          next_url, "parent", directory_resource_id);
    }

    change_list->set_next_link(next_url);
  }
  *change_list->mutable_items() = entries.Pass();

  if (load_counter)
    *load_counter += 1;
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_SUCCESS, base::Passed(&change_list)));
}

GURL FakeDriveService::GetNewUploadSessionUrl() {
  return GURL("https://upload_session_url/" +
              base::Int64ToString(next_upload_sequence_number_++));
}

google_apis::CancelCallback FakeDriveService::AddPermission(
    const std::string& resource_id,
    const std::string& email,
    google_apis::drive::PermissionRole role,
    const google_apis::EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  NOTREACHED();
  return CancelCallback();
}

}  // namespace drive
