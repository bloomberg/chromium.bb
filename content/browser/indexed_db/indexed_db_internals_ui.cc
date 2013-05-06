// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_internals_ui.h"

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_vector.h"
#include "base/threading/platform_thread.h"
#include "base/values.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_url_parameters.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "grit/content_resources.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/zlib/google/zip.h"
#include "webkit/base/file_path_string_conversions.h"
#include "webkit/database/database_util.h"

using webkit_database::DatabaseUtil;

namespace content {

IndexedDBInternalsUI::IndexedDBInternalsUI(WebUI* web_ui)
    : WebUIController(web_ui) {
  web_ui->RegisterMessageCallback(
      "getAllOrigins",
      base::Bind(&IndexedDBInternalsUI::GetAllOrigins, base::Unretained(this)));

  web_ui->RegisterMessageCallback(
      "downloadOriginData",
      base::Bind(&IndexedDBInternalsUI::DownloadOriginData,
                 base::Unretained(this)));

  WebUIDataSource* source =
      WebUIDataSource::Create(kChromeUIIndexedDBInternalsHost);
  source->SetUseJsonJSFormatV2();
  source->SetJsonPath("strings.js");
  source->AddResourcePath("indexeddb_internals.js",
                          IDR_INDEXED_DB_INTERNALS_JS);
  source->AddResourcePath("indexeddb_internals.css",
                          IDR_INDEXED_DB_INTERNALS_CSS);
  source->SetDefaultResource(IDR_INDEXED_DB_INTERNALS_HTML);

  BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  WebUIDataSource::Add(browser_context, source);
}

IndexedDBInternalsUI::~IndexedDBInternalsUI() {}

void IndexedDBInternalsUI::AddContextFromStoragePartition(
    ContextList* contexts,
    std::vector<base::FilePath>* paths,
    StoragePartition* partition) {
  scoped_refptr<IndexedDBContext> context = partition->GetIndexedDBContext();
  contexts->push_back(context);
  paths->push_back(partition->GetPath());
}

void IndexedDBInternalsUI::GetAllOrigins(const base::ListValue* args) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  BrowserContext* browser_context =
      web_ui()->GetWebContents()->GetBrowserContext();

  scoped_ptr<std::vector<base::FilePath> > paths(
      new std::vector<base::FilePath>);
  scoped_ptr<ContextList> contexts(new ContextList);
  BrowserContext::StoragePartitionCallback cb =
      base::Bind(&AddContextFromStoragePartition, contexts.get(), paths.get());
  BrowserContext::ForEachStoragePartition(browser_context, cb);

  BrowserThread::PostTask(
      BrowserThread::WEBKIT_DEPRECATED,
      FROM_HERE,
      base::Bind(&IndexedDBInternalsUI::GetAllOriginsOnWebkitThread,
                 base::Unretained(this),
                 base::Passed(&contexts),
                 base::Passed(&paths)));
}

bool HostNameComparator(const IndexedDBInfo& i, const IndexedDBInfo& j) {
  return i.origin_.host() < j.origin_.host();
}

void IndexedDBInternalsUI::GetAllOriginsOnWebkitThread(
    const scoped_ptr<ContextList> contexts,
    const scoped_ptr<std::vector<base::FilePath> > context_paths) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  DCHECK_EQ(contexts->size(), context_paths->size());

  std::vector<base::FilePath>::const_iterator path_iter =
      context_paths->begin();
  for (ContextList::const_iterator iter = contexts->begin();
       iter != contexts->end();
       ++iter, ++path_iter) {
    IndexedDBContext* context = *iter;
    const base::FilePath& context_path = *path_iter;

    scoped_ptr<std::vector<IndexedDBInfo> > info_list(
        new std::vector<IndexedDBInfo>(context->GetAllOriginsInfo()));
    std::sort(info_list->begin(), info_list->end(), HostNameComparator);
    BrowserThread::PostTask(BrowserThread::UI,
                            FROM_HERE,
                            base::Bind(&IndexedDBInternalsUI::OnOriginsReady,
                                       base::Unretained(this),
                                       base::Passed(&info_list),
                                       context_path));
  }
}

void IndexedDBInternalsUI::OnOriginsReady(
    scoped_ptr<std::vector<IndexedDBInfo> > origins,
    const base::FilePath& path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::ListValue urls;
  for (std::vector<IndexedDBInfo>::const_iterator iter = origins->begin();
       iter != origins->end();
       ++iter) {
    base::DictionaryValue* info = new DictionaryValue;
    info->SetString("url", iter->origin_.spec());
    info->SetDouble("size", iter->size_);
    info->SetDouble("last_modified", iter->last_modified_.ToJsTime());
    info->SetString("path", iter->path_.value());
    urls.Append(info);
  }
  web_ui()->CallJavascriptFunction(
      "indexeddb.onOriginsReady", urls, base::StringValue(path.value()));
}

static void FindContext(const base::FilePath& partition_path,
                        StoragePartition** result_partition,
                        scoped_refptr<IndexedDBContextImpl>* result_context,
                        StoragePartition* storage_partition) {
  if (storage_partition->GetPath() == partition_path) {
    *result_partition = storage_partition;
    *result_context = static_cast<IndexedDBContextImpl*>(
        storage_partition->GetIndexedDBContext());
  }
}

void IndexedDBInternalsUI::DownloadOriginData(const base::ListValue* args) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  base::FilePath::StringType path_string;
  if (!args->GetString(0, &path_string))
    return;
  const base::FilePath partition_path(path_string);

  std::string url_string;
  if (!args->GetString(1, &url_string))
    return;
  const GURL origin_url(url_string);

  // search the origins to find the right context

  BrowserContext* browser_context =
      web_ui()->GetWebContents()->GetBrowserContext();

  scoped_refptr<IndexedDBContextImpl> result_context;
  StoragePartition* result_partition;
  scoped_ptr<ContextList> contexts(new ContextList);
  BrowserContext::StoragePartitionCallback cb = base::Bind(
      &FindContext, partition_path, &result_partition, &result_context);
  BrowserContext::ForEachStoragePartition(browser_context, cb);
  DCHECK(result_partition);
  DCHECK(result_context);

  BrowserThread::PostTask(
      BrowserThread::WEBKIT_DEPRECATED,
      FROM_HERE,
      base::Bind(&IndexedDBInternalsUI::DownloadOriginDataOnWebkitThread,
                 base::Unretained(this),
                 result_partition->GetPath(),
                 result_context,
                 origin_url));
}

void IndexedDBInternalsUI::DownloadOriginDataOnWebkitThread(
    const base::FilePath& partition_path,
    const scoped_refptr<IndexedDBContextImpl> context,
    const GURL& origin_url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));

  if (!context->IsInOriginSet(origin_url))
    return;

  context->ForceClose(origin_url);

  base::ScopedTempDir temp_dir;
  if (!temp_dir.CreateUniqueTempDir())
    return;

  // This will get cleaned up on the File thread after the download
  // has completed.
  base::FilePath temp_path = temp_dir.Take();

  base::string16 origin_id = DatabaseUtil::GetOriginIdentifier(origin_url);
  base::FilePath::StringType zip_name =
      webkit_base::WebStringToFilePathString(origin_id);
  base::FilePath zip_path =
      temp_path.Append(zip_name).AddExtension(FILE_PATH_LITERAL("zip"));

  // This happens on the "webkit" thread (which is really just the IndexedDB
  // thread) as a simple way to avoid another script reopening the origin
  // while we are zipping.
  zip::Zip(context->GetFilePath(origin_url), zip_path, true);

  BrowserThread::PostTask(BrowserThread::UI,
                          FROM_HERE,
                          base::Bind(&IndexedDBInternalsUI::OnDownloadDataReady,
                                     base::Unretained(this),
                                     partition_path,
                                     origin_url,
                                     temp_path,
                                     zip_path));
}

void IndexedDBInternalsUI::OnDownloadDataReady(
    const base::FilePath& partition_path,
    const GURL& origin_url,
    const base::FilePath temp_path,
    const base::FilePath zip_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const GURL url = GURL(FILE_PATH_LITERAL("file://") + zip_path.value());
  BrowserContext* browser_context =
      web_ui()->GetWebContents()->GetBrowserContext();
  scoped_ptr<DownloadUrlParameters> dl_params(
      DownloadUrlParameters::FromWebContents(web_ui()->GetWebContents(), url));
  DownloadManager* dlm = BrowserContext::GetDownloadManager(browser_context);

  const GURL referrer(web_ui()->GetWebContents()->GetURL());
  dl_params->set_referrer(
      content::Referrer(referrer, WebKit::WebReferrerPolicyDefault));

  // This is how to watch for the download to finish: first wait for it
  // to start, then attach a DownloadItem::Observer to observe the
  // state change to the finished state.
  dl_params->set_callback(base::Bind(&IndexedDBInternalsUI::OnDownloadStarted,
                                     base::Unretained(this),
                                     partition_path,
                                     origin_url,
                                     temp_path));
  dlm->DownloadUrl(dl_params.Pass());
}

// The entire purpose of this class is to delete the temp file after
// the download is complete.
class FileDeleter : public DownloadItem::Observer {
 public:
  explicit FileDeleter(const base::FilePath& temp_dir) : temp_dir_(temp_dir) {}
  virtual ~FileDeleter();

  virtual void OnDownloadUpdated(DownloadItem* download) OVERRIDE;
  virtual void OnDownloadOpened(DownloadItem* item) OVERRIDE {}
  virtual void OnDownloadRemoved(DownloadItem* item) OVERRIDE {}
  virtual void OnDownloadDestroyed(DownloadItem* item) OVERRIDE {}

 private:
  const base::FilePath temp_dir_;
};

void FileDeleter::OnDownloadUpdated(DownloadItem* item) {
  switch (item->GetState()) {
    case DownloadItem::IN_PROGRESS:
      break;
    case DownloadItem::COMPLETE:
    case DownloadItem::CANCELLED:
    case DownloadItem::INTERRUPTED: {
      item->RemoveObserver(this);
      BrowserThread::DeleteOnFileThread::Destruct(this);
      break;
    }
    default:
      NOTREACHED();
  }
}

FileDeleter::~FileDeleter() {
  base::ScopedTempDir path;
  bool will_delete ALLOW_UNUSED = path.Set(temp_dir_);
  DCHECK(will_delete);
}

void IndexedDBInternalsUI::OnDownloadStarted(
    const base::FilePath& partition_path,
    const GURL& origin_url,
    const base::FilePath& temp_path,
    DownloadItem* item,
    net::Error error) {

  if (error != net::OK) {
    LOG(ERROR) << "Error downloading database dump: "
               << net::ErrorToString(error);
    return;
  }

  item->AddObserver(new FileDeleter(temp_path));
  web_ui()->CallJavascriptFunction("indexeddb.onOriginDownloadReady",
                                   base::StringValue(partition_path.value()),
                                   base::StringValue(origin_url.spec()));
}

}  // namespace content
