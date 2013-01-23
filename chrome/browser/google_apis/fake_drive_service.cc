// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/fake_drive_service.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/google_apis/drive_api_parser.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "chrome/browser/google_apis/test_util.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/escape.h"

using content::BrowserThread;

namespace google_apis {

FakeDriveService::FakeDriveService()
    : largest_changestamp_(0),
      default_max_results_(0),
      resource_id_count_(0),
      resource_list_load_count_(0),
      account_metadata_load_count_(0),
      offline_(false) {
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

  // Update the largest_changestamp_.
  scoped_ptr<AccountMetadataFeed> account_metadata =
      AccountMetadataFeed::CreateFrom(*account_metadata_value_);
  largest_changestamp_ = account_metadata->largest_changestamp();

  // Add the largest changestamp to the existing entries.
  // This will be used to generate change lists in GetResourceList().
  if (resource_list_value_) {
    base::DictionaryValue* resource_list_dict = NULL;
    base::ListValue* entries = NULL;
    if (resource_list_value_->GetAsDictionary(&resource_list_dict) &&
        resource_list_dict->GetList("entry", &entries)) {
      for (size_t i = 0; i < entries->GetSize(); ++i) {
        base::DictionaryValue* entry = NULL;
        if (entries->GetDictionary(i, &entry)) {
          entry->SetString("docs$changestamp.value",
                           base::Int64ToString(largest_changestamp_));
        }
      }
    }
  }

  return account_metadata_value_;
}

bool FakeDriveService::LoadAppListForDriveApi(
    const std::string& relative_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  app_info_value_ = test_util::LoadJSONFile(relative_path);
  return app_info_value_;
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

std::string FakeDriveService::GetRootResourceId() const {
  return "fake_root";
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

  if (offline_) {
    scoped_ptr<ResourceList> null;
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   GDATA_NO_CONNECTION,
                   base::Passed(&null)));
    return;
  }

  // "start-offset" is a parameter only used in the FakeDriveService to
  // implement pagenation.
  int start_offset = 0;
  int max_results = default_max_results_;
  std::vector<std::pair<std::string, std::string> > parameters;
  if (base::SplitStringIntoKeyValuePairs(
          feed_url.query(), '=', '&', &parameters)) {
    for (size_t i = 0; i < parameters.size(); ++i) {
      if (parameters[i].first == "start-offset")
        base::StringToInt(parameters[i].second, &start_offset);
      if (parameters[i].first == "max-results")
        base::StringToInt(parameters[i].second, &max_results);
    }
  }

  scoped_ptr<ResourceList> resource_list =
      ResourceList::CreateFrom(*resource_list_value_);

  // Filter out entries per parameters like |directory_resource_id| and
  // |search_query|.
  ScopedVector<ResourceEntry>* entries = resource_list->mutable_entries();

  int num_entries_matched = 0;
  for (size_t i = 0; i < entries->size();) {
    ResourceEntry* entry = (*entries)[i];
    bool should_exclude = false;

    // If |directory_resource_id| is set, exclude the entry if it's not in
    // the target directory.
    if (!directory_resource_id.empty()) {
      // Get the parent resource ID of the entry. If the parent link does
      // not exist, the entry must be in the root directory.
      std::string parent_resource_id = "folder:root";
      const google_apis::Link* parent_link =
          entry->GetLinkByType(Link::LINK_PARENT);
      if (parent_link) {
        parent_resource_id =
            net::UnescapeURLComponent(parent_link->href().ExtractFileName(),
                                      net::UnescapeRule::URL_SPECIAL_CHARS);
      }
      if (directory_resource_id != parent_resource_id)
        should_exclude = true;
    }

    // If |search_query| is set, exclude the entry if it does not contain the
    // search query in the title.
    if (!search_query.empty()) {
      if (UTF16ToUTF8(entry->title()).find(search_query) == std::string::npos)
        should_exclude = true;
    }

    // If |start_changestamp| is set, exclude the entry if the
    // changestamp is older than |largest_changestamp|.
    // See https://developers.google.com/google-apps/documents-list/
    // #retrieving_all_changes_since_a_given_changestamp
    if (start_changestamp > 0 && entry->changestamp() < start_changestamp)
      should_exclude = true;

    // The entry matched the criteria for inclusion.
    if (!should_exclude)
      ++num_entries_matched;

    // If |start_offset| is set, exclude the entry if the entry is before the
    // start index. <= instead of < as |num_entries_matched| was
    // already incremented.
    if (start_offset > 0 && num_entries_matched <= start_offset)
      should_exclude = true;

    if (should_exclude)
      entries->erase(entries->begin() + i);
    else
      ++i;
  }

  // If |max_results| is set, trim the entries if the number exceeded the max
  // results.
  if (max_results > 0 && entries->size() > static_cast<size_t>(max_results)) {
    entries->erase(entries->begin() + max_results, entries->end());
    // Adds the next URL.
    const GURL next_url(
        base::StringPrintf(
            "http://localhost/?start-offset=%d&max-results=%d",
            start_offset + max_results,
            max_results));
    Link* link = new Link;
    link->set_type(Link::LINK_NEXT);
    link->set_href(next_url);
    resource_list->mutable_links()->push_back(link);
  }

  ++resource_list_load_count_;
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

  if (offline_) {
    scoped_ptr<ResourceEntry> null;
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   GDATA_NO_CONNECTION,
                   base::Passed(&null)));
    return;
  }

  base::DictionaryValue* entry = FindEntryByResourceId(resource_id);
  if (entry) {
    scoped_ptr<ResourceEntry> resource_entry =
        ResourceEntry::CreateFrom(*entry);
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, HTTP_SUCCESS, base::Passed(&resource_entry)));
    return;
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

  if (offline_) {
    scoped_ptr<AccountMetadataFeed> null;
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   GDATA_NO_CONNECTION,
                   base::Passed(&null)));
    return;
  }

  ++account_metadata_load_count_;
  scoped_ptr<AccountMetadataFeed> account_metadata =
      AccountMetadataFeed::CreateFrom(*account_metadata_value_);
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback,
                 HTTP_SUCCESS,
                 base::Passed(&account_metadata)));
}

void FakeDriveService::GetAppList(const GetAppListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (offline_) {
    scoped_ptr<AppList> null;
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   GDATA_NO_CONNECTION,
                   base::Passed(&null)));
    return;
  }

  scoped_ptr<AppList> app_list(AppList::CreateFrom(*app_info_value_));
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_SUCCESS, base::Passed(&app_list)));
}

void FakeDriveService::DeleteResource(
    const GURL& edit_url,
    const EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (offline_) {
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, GDATA_NO_CONNECTION));
    return;
  }

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

  // TODO(satorux): Add support for returning "deleted" entries in
  // changelists from GetResourceList().
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

  if (offline_) {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(download_action_callback,
                   GDATA_NO_CONNECTION,
                   FilePath()));
    return;
  }

  base::DictionaryValue* entry = FindEntryByContentUrl(content_url);
  if (!entry) {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(download_action_callback, HTTP_NOT_FOUND, FilePath()));
    return;
  }

  // Write "x"s of the file size specified in the entry.
  std::string file_size_string;
  entry->GetString("docs$size.$t", &file_size_string);
  int64 file_size = 0;
  if (base::StringToInt64(file_size_string, &file_size)) {
    std::string content(file_size, 'x');
    DCHECK_EQ(static_cast<size_t>(file_size), content.size());

    if (static_cast<int>(content.size()) ==
        file_util::WriteFile(local_cache_path,
                             content.data(),
                             content.size())) {
      base::MessageLoopProxy::current()->PostTask(
          FROM_HERE,
          base::Bind(download_action_callback,
                     HTTP_SUCCESS,
                     local_cache_path));
      return;
    }
  }

  // Failed to write the content.
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(download_action_callback, GDATA_FILE_ERROR, FilePath()));
}

void FakeDriveService::CopyHostedDocument(
    const std::string& resource_id,
    const std::string& new_name,
    const GetResourceEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (offline_) {
    scoped_ptr<ResourceEntry> null;
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   GDATA_NO_CONNECTION,
                   base::Passed(&null)));
    return;
  }

  base::DictionaryValue* resource_list_dict = NULL;
  base::ListValue* entries = NULL;
  // Go through entries and copy the one that matches |resource_id|.
  if (resource_list_value_->GetAsDictionary(&resource_list_dict) &&
      resource_list_dict->GetList("entry", &entries)) {
    for (size_t i = 0; i < entries->GetSize(); ++i) {
      base::DictionaryValue* entry = NULL;
      base::DictionaryValue* resource_id_dict = NULL;
      base::ListValue* categories = NULL;
      std::string current_resource_id;
      if (entries->GetDictionary(i, &entry) &&
          entry->GetDictionary("gd$resourceId", &resource_id_dict) &&
          resource_id_dict->GetString("$t", &current_resource_id) &&
          resource_id == current_resource_id &&
          entry->GetList("category", &categories)) {
        // Check that the resource is a hosted document. We consider it a
        // hosted document if the kind is neither "folder" nor "file".
        for (size_t k = 0; k < categories->GetSize(); ++k) {
          base::DictionaryValue* category = NULL;
          std::string scheme, term;
          if (categories->GetDictionary(k, &category) &&
              category->GetString("scheme", &scheme) &&
              category->GetString("term", &term) &&
              scheme == "http://schemas.google.com/g/2005#kind" &&
              term != "http://schemas.google.com/docs/2007#file" &&
              term != "http://schemas.google.com/docs/2007#folder") {
            // Make a copy and set the new resource ID and the new title.
            scoped_ptr<DictionaryValue> copied_entry(entry->DeepCopy());
            copied_entry->SetString("gd$resourceId.$t",
                                    resource_id + "_copied");
            copied_entry->SetString("title.$t", new_name);

            AddNewChangestamp(copied_entry.get());

            // Parse the new entry.
            scoped_ptr<ResourceEntry> resource_entry =
                ResourceEntry::CreateFrom(*copied_entry);
            // Add it to the resource list.
            entries->Append(copied_entry.release());

            MessageLoop::current()->PostTask(
                FROM_HERE,
                base::Bind(callback,
                           HTTP_SUCCESS,
                           base::Passed(&resource_entry)));
            return;
          }
        }
      }
    }
  }

  scoped_ptr<ResourceEntry> null;
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_NOT_FOUND, base::Passed(&null)));
}

void FakeDriveService::RenameResource(
    const GURL& edit_url,
    const std::string& new_name,
    const EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (offline_) {
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, GDATA_NO_CONNECTION));
    return;
  }

  base::DictionaryValue* entry = FindEntryByEditUrl(edit_url);
  if (entry) {
    entry->SetString("title.$t", new_name);
    AddNewChangestamp(entry);
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, HTTP_SUCCESS));
    return;
  }

  MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, HTTP_NOT_FOUND));
}

void FakeDriveService::AddResourceToDirectory(
    const GURL& parent_content_url,
    const GURL& edit_url,
    const EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (offline_) {
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, GDATA_NO_CONNECTION));
    return;
  }

  base::DictionaryValue* entry = FindEntryByEditUrl(edit_url);
  if (entry) {
    base::ListValue* links = NULL;
    if (entry->GetList("link", &links)) {
      bool parent_link_found = false;
      for (size_t i = 0; i < links->GetSize(); ++i) {
        base::DictionaryValue* link = NULL;
        std::string rel;
        if (links->GetDictionary(i, &link) &&
            link->GetString("rel", &rel) &&
            rel == "http://schemas.google.com/docs/2007#parent") {
          link->SetString("href", parent_content_url.spec());
          parent_link_found = true;
        }
      }
      // The parent link does not exist if a resource is in the root
      // directory.
      if (!parent_link_found) {
        base::DictionaryValue* link = new base::DictionaryValue;
        link->SetString("rel", "http://schemas.google.com/docs/2007#parent");
        link->SetString("href", parent_content_url.spec());
        links->Append(link);
      }

      AddNewChangestamp(entry);
      MessageLoop::current()->PostTask(
          FROM_HERE, base::Bind(callback, HTTP_SUCCESS));
      return;
    }
  }

  MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, HTTP_NOT_FOUND));
}

void FakeDriveService::RemoveResourceFromDirectory(
    const GURL& parent_content_url,
    const std::string& resource_id,
    const EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (offline_) {
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, GDATA_NO_CONNECTION));
    return;
  }

  base::DictionaryValue* entry = FindEntryByResourceId(resource_id);
  if (entry) {
    base::ListValue* links = NULL;
    if (entry->GetList("link", &links)) {
      for (size_t i = 0; i < links->GetSize(); ++i) {
        base::DictionaryValue* link = NULL;
        std::string rel;
        std::string href;
        if (links->GetDictionary(i, &link) &&
            link->GetString("rel", &rel) &&
            link->GetString("href", &href) &&
            rel == "http://schemas.google.com/docs/2007#parent" &&
            GURL(href) == parent_content_url) {
          links->Remove(i, NULL);
          AddNewChangestamp(entry);
          MessageLoop::current()->PostTask(
              FROM_HERE, base::Bind(callback, HTTP_SUCCESS));
          return;
        }
      }

      // We are dealing with a no-"parent"-link file as in the root directory.
      if (parent_content_url.is_empty()) {
        AddNewChangestamp(entry);
        MessageLoop::current()->PostTask(
            FROM_HERE, base::Bind(callback, HTTP_SUCCESS));
        return;
      }
    }
  }

  MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, HTTP_NOT_FOUND));
}

void FakeDriveService::AddNewDirectory(
    const GURL& parent_content_url,
    const std::string& directory_name,
    const GetResourceEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (offline_) {
    scoped_ptr<ResourceEntry> null;
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   GDATA_NO_CONNECTION,
                   base::Passed(&null)));
    return;
  }

  // If the parent content URL is not empty, the parent should exist.
  if (!parent_content_url.is_empty()) {
    base::DictionaryValue* parent_entry =
        FindEntryByContentUrl(parent_content_url);
    if (!parent_entry) {
      scoped_ptr<ResourceEntry> null;
      MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(callback, HTTP_NOT_FOUND, base::Passed(&null)));
      return;
    }
  }

  const std::string new_resource_id = GetNewResourceId();

  scoped_ptr<base::DictionaryValue> new_entry(new base::DictionaryValue);
  // Set the resource ID and the title
  new_entry->SetString("gd$resourceId.$t", new_resource_id);
  new_entry->SetString("title.$t", directory_name);

  // Add "category" which sets the resource type to folder.
  base::ListValue* categories = new base::ListValue;
  base::DictionaryValue* category = new base::DictionaryValue;
  category->SetString("label", "folder");
  category->SetString("scheme", "http://schemas.google.com/g/2005#kind");
  category->SetString("term", "http://schemas.google.com/docs/2007#folder");
  categories->Append(category);
  new_entry->Set("category", categories);

  // Add "content" which sets the content URL.
  base::DictionaryValue* content = new base::DictionaryValue;
  content->SetString("src", "https://xxx/content/" + new_resource_id);
  new_entry->Set("content", content);

  // Add "link" which sets the parent URL and the edit URL.
  base::ListValue* links = new base::ListValue;
  if (!parent_content_url.is_empty()) {
    base::DictionaryValue* parent_link = new base::DictionaryValue;
    parent_link->SetString("href", parent_content_url.spec());
    parent_link->SetString("rel",
                           "http://schemas.google.com/docs/2007#parent");
    links->Append(parent_link);
  }
  base::DictionaryValue* edit_link = new base::DictionaryValue;
  edit_link->SetString("href", "https://xxx/edit/" + new_resource_id);
  edit_link->SetString("rel", "edit");
  links->Append(edit_link);
  new_entry->Set("link", links);

  AddNewChangestamp(new_entry.get());

  // Add the new entry to the resource list.
  base::DictionaryValue* resource_list_dict = NULL;
  base::ListValue* entries = NULL;
  if (resource_list_value_->GetAsDictionary(&resource_list_dict) &&
      resource_list_dict->GetList("entry", &entries)) {
    // Parse the entry before releasing it.
    scoped_ptr<ResourceEntry> parsed_entry(
        ResourceEntry::CreateFrom(*new_entry));

    entries->Append(new_entry.release());

    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, HTTP_SUCCESS, base::Passed(&parsed_entry)));
    return;
  }

  scoped_ptr<ResourceEntry> null;
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_NOT_FOUND, base::Passed(&null)));
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

base::DictionaryValue* FakeDriveService::FindEntryByResourceId(
    const std::string& resource_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

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
        return entry;
      }
    }
  }

  return NULL;
}

base::DictionaryValue* FakeDriveService::FindEntryByEditUrl(
    const GURL& edit_url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  base::DictionaryValue* resource_list_dict = NULL;
  base::ListValue* entries = NULL;
  // Go through entries and return the one that matches |edit_url|.
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
            return entry;
          }
        }
      }
    }
  }

  return NULL;
}

base::DictionaryValue* FakeDriveService::FindEntryByContentUrl(
    const GURL& content_url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  base::DictionaryValue* resource_list_dict = NULL;
  base::ListValue* entries = NULL;
  // Go through entries and return the one that matches |content_url|.
  if (resource_list_value_->GetAsDictionary(&resource_list_dict) &&
      resource_list_dict->GetList("entry", &entries)) {
    for (size_t i = 0; i < entries->GetSize(); ++i) {
      base::DictionaryValue* entry = NULL;
      base::DictionaryValue* content = NULL;
      std::string current_content_url;
      if (entries->GetDictionary(i, &entry) &&
          entry->GetDictionary("content", &content) &&
          content->GetString("src", &current_content_url) &&
          content_url == GURL(current_content_url)) {
        return entry;
      }
    }
  }

  return NULL;
}

std::string FakeDriveService::GetNewResourceId() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  ++resource_id_count_;
  return base::StringPrintf("resource_id_%d", resource_id_count_);
}

void FakeDriveService::AddNewChangestamp(base::DictionaryValue* entry) {
  ++largest_changestamp_;
  entry->SetString("docs$changestamp.value",
                   base::Int64ToString(largest_changestamp_));
}

}  // namespace google_apis
