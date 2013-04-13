// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_internals_ui.h"

#include <algorithm>

#include "base/bind.h"
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
      base::Bind(&IndexedDBInternalsUI::GetAllOrigins,
                 base::Unretained(this)));

  WebUIDataSource* source =
      WebUIDataSource::Create(chrome::kChromeUIIndexedDBInternalsHost);
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

IndexedDBInternalsUI::~IndexedDBInternalsUI() {
}

void IndexedDBInternalsUI::GetAllOrigins(const base::ListValue* args) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  BrowserContext* browser_context =
      web_ui()->GetWebContents()->GetBrowserContext();

  // TODO(alecflett): do this for each storage partition in the context
  StoragePartition* partition =
      BrowserContext::GetDefaultStoragePartition(browser_context);
  scoped_refptr<IndexedDBContext> context = partition->GetIndexedDBContext();

  BrowserThread::PostTask(
      BrowserThread::WEBKIT_DEPRECATED, FROM_HERE,
      base::Bind(
          &IndexedDBInternalsUI::GetAllOriginsOnWebkitThread,
          base::Unretained(this),
          context));
}

bool HostNameComparator(const IndexedDBInfo& i, const IndexedDBInfo& j) {
  return i.origin.host() < j.origin.host();
}

void IndexedDBInternalsUI::GetAllOriginsOnWebkitThread(
    scoped_refptr<IndexedDBContext> context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));

  scoped_ptr<std::vector<IndexedDBInfo> > origins(
      new std::vector<IndexedDBInfo>(context->GetAllOriginsInfo()));
  std::sort(origins->begin(), origins->end(), HostNameComparator);

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&IndexedDBInternalsUI::OnOriginsReady, base::Unretained(this),
                 base::Passed(&origins)));
}

void IndexedDBInternalsUI::OnOriginsReady(
    scoped_ptr<std::vector<IndexedDBInfo> > origins) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::ListValue urls;
  for (std::vector<IndexedDBInfo>::const_iterator iter = origins->begin();
       iter != origins->end(); ++iter) {
    base::DictionaryValue* info = new DictionaryValue;
    info->SetString("url", iter->origin.spec());
    info->SetDouble("size", iter->size);
    info->SetDouble("last_modified", iter->last_modified.ToJsTime());
    urls.Append(info);
  }
  web_ui()->CallJavascriptFunction("indexeddb.onOriginsReady", urls);
}
}
