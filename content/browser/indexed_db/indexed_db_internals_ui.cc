// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_internals_ui.h"

#include <algorithm>

#include "base/bind.h"
#include "base/memory/scoped_vector.h"
#include "base/threading/platform_thread.h"
#include "base/values.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "grit/content_resources.h"

namespace content {

IndexedDBInternalsUI::IndexedDBInternalsUI(WebUI* web_ui)
    : WebUIController(web_ui) {
  web_ui->RegisterMessageCallback(
      "getAllOrigins",
      base::Bind(&IndexedDBInternalsUI::GetAllOrigins, base::Unretained(this)));

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
    scoped_ptr<ContextList> contexts,
    scoped_ptr<std::vector<base::FilePath> > context_paths) {
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

}  // namespace content
