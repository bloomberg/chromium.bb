// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_save_page_api.h"

#include "base/file_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "content/browser/child_process_security_policy.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/download/mhtml_generation_manager.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"

using content::BrowserThread;

// Error messages.
const char* const kFileTooBigError = "The MHTML file generated is too big.";
const char* const kMHTMLGenerationFailedError = "Failed to generate MHTML.";
const char* const kSizeRetrievalError =
    "Failed to retrieve size of generated MHTML.";
const char* const kTemporaryFileError = "Failed to create a temporary file.";
const char* const kTabClosedError = "Cannot find the tab for thie request.";

SavePageAsMHTMLFunction::SavePageAsMHTMLFunction() : tab_id_(0) {
}

SavePageAsMHTMLFunction::~SavePageAsMHTMLFunction() {
}

bool SavePageAsMHTMLFunction::RunImpl() {
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &tab_id_));

  AddRef();  // Balanced in ReturnFailure/ReturnSuccess()

  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this, &SavePageAsMHTMLFunction::CreateTemporaryFile));
  return true;
}

void SavePageAsMHTMLFunction::CreateTemporaryFile() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  // TODO(jcivelli): http://crbug.com/97489 we don't clean-up the temporary file
  //                 at this point.  It must be done before we can take that API
  //                 out of experimental.
  bool success = file_util::CreateTemporaryFile(&mhtml_path_);
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(this, &SavePageAsMHTMLFunction::TemporaryFileCreated,
                        success));
}

void SavePageAsMHTMLFunction::TemporaryFileCreated(bool success) {
  if (!success) {
    ReturnFailure(kTemporaryFileError);
    return;
  }

  Browser* browser = NULL;
  TabContentsWrapper* tab_contents_wrapper = NULL;

  if (!ExtensionTabUtil::GetTabById(tab_id_, profile(), include_incognito(),
      &browser, NULL, &tab_contents_wrapper, NULL)) {
    ReturnFailure(kTabClosedError);
    return;
  }

  TabContents* tab_contents = tab_contents_wrapper->tab_contents();
  registrar_.Add(
      this, content::NOTIFICATION_MHTML_GENERATED,
      content::Source<RenderViewHost>(tab_contents->render_view_host()));
  // TODO(jcivelli): we should listen for navigation in the tab, tab closed,
  //                 renderer died.
  g_browser_process->mhtml_generation_manager()->GenerateMHTML(
      tab_contents, mhtml_path_);
}

void SavePageAsMHTMLFunction::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(type == content::NOTIFICATION_MHTML_GENERATED);

  const MHTMLGenerationManager::NotificationDetails* save_details =
      content::Details<MHTMLGenerationManager::NotificationDetails>(details).
          ptr();

  if (mhtml_path_ != save_details->file_path) {
    // This could happen if there are concurrent MHTML generations going on for
    // the same tab.
    LOG(WARNING) << "Received a notification that MHTML was generated but for a"
        " different file.";
    return;
  }

  registrar_.RemoveAll();

  if (save_details->file_size <= 0) {
    ReturnFailure(kMHTMLGenerationFailedError);
    return;
  }

  if (save_details->file_size > std::numeric_limits<int>::max()) {
    ReturnFailure(kFileTooBigError);
    return;
  }

  ReturnSuccess(save_details->file_size);
}

void SavePageAsMHTMLFunction::ReturnFailure(const std::string& error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  error_ = error;

  SendResponse(false);

  Release();  // Balanced in Run()
}

void SavePageAsMHTMLFunction::ReturnSuccess(int64 file_size) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  int child_id = render_view_host()->process()->GetID();
  ChildProcessSecurityPolicy::GetInstance()->GrantReadFile(
      child_id, mhtml_path_);

  DictionaryValue* dict = new DictionaryValue();
  result_.reset(dict);
  dict->SetString("mhtmlFilePath", mhtml_path_.value());
  dict->SetInteger("mhtmlFileLength", file_size);

  SendResponse(true);

  Release();  // Balanced in Run()
}
