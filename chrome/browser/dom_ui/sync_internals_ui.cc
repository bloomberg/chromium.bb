// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/sync_internals_ui.h"

#include "base/ref_counted.h"
#include "base/task.h"
#include "base/tracked_objects.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/dom_ui/sync_internals_html_source.h"
#include "chrome/browser/dom_ui/sync_internals_message_handler.h"
#include "chrome/browser/tab_contents/tab_contents.h"

SyncInternalsUI::SyncInternalsUI(TabContents* contents) : DOMUI(contents) {
  SyncInternalsMessageHandler* message_handler =
      new SyncInternalsMessageHandler(contents->profile());
  message_handler->Attach(this);
  AddMessageHandler(message_handler);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(
          ChromeURLDataManager::GetInstance(),
          &ChromeURLDataManager::AddDataSource,
          make_scoped_refptr(new SyncInternalsHTMLSource())));
}

SyncInternalsUI::~SyncInternalsUI() {}
